#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>

#include <glib.h>

#include <gmp.h>

#include "defs.h"
#include "file.h"
#include "fractal-render.h"
#include "render-png.h"


#define NETWORK_DELIM " \t\r\n"


struct thread_info;

struct worker_state {
	unsigned thread_count;
	int connection;
	struct thread_info *thread_info;
};


struct thread_info {
	GThread *thread;
	struct worker_state *state;
	unsigned thread_id;
	GMutex *mutex;
	GCond *cond;
	bool terminate;
	struct mandeldata md;
	unsigned frame, w, h;
};


static gint thread_count = 3;
static struct color colors[COLORS];


gpointer
worker_thread (gpointer data)
{
	struct thread_info *info = (struct thread_info *) data;
	struct worker_state *state = info->state;

	GMutex *startup_mutex = info->mutex;
	g_mutex_lock (info->mutex);
	g_cond_signal (info->cond);
	info->mutex = g_mutex_new ();
	g_mutex_lock (info->mutex);
	info->cond = g_cond_new ();
	g_mutex_unlock (startup_mutex);

	while (true) {
		fprintf (stderr, "* INFO: Thread %u waiting for work\n", info->thread_id);
		g_cond_wait (info->cond, info->mutex);
		fprintf (stderr, "* INFO: Thread %u rendering frame %u\n", info->thread_id, info->frame);
		char buf[256];
		snprintf (buf, sizeof (buf), "file%06u.png", info->frame);
		/* XXX much stuff hard-coded here */
		render_to_png (&info->md, buf, 9, NULL, colors, info->w, info->h, 1);
		mandeldata_clear (&info->md);

		/*
		 * We cannot use stdio here, because it relies on locking file
		 * handles before doing anything on them. The dispatcher thread is
		 * doing a blocking fgets() most of the time, so stdio would
		 * spend a long time here waiting for the lock. Non-blocking I/O
		 * would make the code way too complex. Thus, we use stdio for input
		 * and do output via write(2).
		 * This is dirty and possibly non-portable (works on Linux, though).
		 */
		int mlen = snprintf (buf, sizeof (buf), "DONE %u\r\n", info->thread_id);
		write (state->connection, buf, mlen);
	}

	return NULL;
}

int
main (int argc, char **argv)
{
	if (argc != 4) {
		fprintf (stderr, "* USAGE: %s <host> <port> <threads>\n", argv[0]);
		return 1;
	}

	mpf_set_default_prec (1024); /* ! */
	mpfr_set_default_prec (1024); /* ! */

	const char *hostname = argv[1];
	const char *servname = argv[2];
	thread_count = atoi (argv[3]);

	if (thread_count < 1) {
		fprintf (stderr, "* ERROR: Thread count must be >= 1.\n");
		return 1;
	}

	struct addrinfo aihints = {
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
		.ai_protocol = IPPROTO_TCP
	};
	struct addrinfo *ai = NULL;
	int r = getaddrinfo (hostname, servname, &aihints, &ai);
	if (r != 0) {
		fprintf (stderr, "* ERROR: Name lookup failed: %s\n", gai_strerror (r));
		return 1;
	}
	int s = -1;
	struct addrinfo *aicur;
	for (aicur = ai; aicur != NULL; aicur = aicur->ai_next) {
		char addrbuf[256], portbuf[16];
		if (getnameinfo (aicur->ai_addr, aicur->ai_addrlen, addrbuf, sizeof (addrbuf), portbuf, sizeof (portbuf), NI_NUMERICHOST | NI_NUMERICSERV) == 0)
			fprintf (stderr, "* INFO: Trying to connect to [%s] port [%s]... ", addrbuf, portbuf);
		else
			fprintf (stderr, "* INFO: Trying to connect to [something that getnameinfo() doesn't understand]... ");
		fflush (stderr);

		s = socket (aicur->ai_family, aicur->ai_socktype, aicur->ai_protocol);
		if (s < 0) {
			fprintf (stderr, "%s\n", strerror (errno));
			continue;
		}

		if (connect (s, aicur->ai_addr, aicur->ai_addrlen) < 0) {
			fprintf (stderr, "%s\n", strerror (errno));
			close (s);
			s = -1;
			continue;
		}

		fprintf (stderr, "okay.\n");
		break;
	}

	freeaddrinfo (ai);

	if (s < 0) {
		fprintf (stderr, "* ERROR: Cannot connect to any address of the desired host.\n");
		return 1;
	}

	FILE *f = fdopen (s, "r+");
	if (f == NULL) {
		perror ("fdopen");
		return 1;
	}

#if 0
	if (setvbuf (f, NULL, _IONBF, 0) != 0)
		perror ("setvbuf"); /* this is not fatal */
#endif

	g_thread_init (NULL);

	struct worker_state state[1];
	struct thread_info tinfo[thread_count];

	state->thread_count = thread_count;
	state->thread_info = tinfo;
	state->connection = s;

	GMutex *startup_mutex = g_mutex_new ();
	GCond *startup_cond = g_cond_new ();
	g_mutex_lock (startup_mutex);

	int i;
	for (i = 0; i < state->thread_count; i++) {
		tinfo[i].state = state;
		tinfo[i].thread_id = i;
		tinfo[i].mutex = startup_mutex;
		tinfo[i].cond = startup_cond;
		tinfo[i].terminate = false;
		tinfo[i].thread = g_thread_create (worker_thread, (gpointer) &tinfo[i], true, NULL);
		fprintf (stderr, "* DEBUG: waiting for thread %d\n", i);
		g_cond_wait (startup_cond, startup_mutex);
	}

	g_mutex_unlock (startup_mutex);
	g_mutex_free (startup_mutex);
	g_cond_free (startup_cond);

	fprintf (f, "MOIN %u\r\n", state->thread_count);
	fflush (f);

	for (i = 0; i < COLORS; i++) {
		colors[i].r = (unsigned short) (sin (2 * M_PI * i / COLORS) * 127) + 128;
		colors[i].g = (unsigned short) (sin (4 * M_PI * i / COLORS) * 127) + 128;
		colors[i].b = (unsigned short) (sin (6 * M_PI * i / COLORS) * 127) + 128;
	}

	while (true) {
		char buf[256];
		if (fgets (buf, sizeof (buf), f) == NULL) {
			if (feof (f))
				fprintf (stderr, "* ERROR: Server unexpectedly closed the connection.\n");
			else
				fprintf (stderr, "* ERROR reading from server: %s\n", strerror (errno));
			return 1;
		} else if (buf[strlen (buf) - 1] != '\n') {
			if (feof (f))
				fprintf (stderr, "* ERROR: Server unexpectedly closed the connection.\n");
			else
				fprintf (stderr, "* ERROR: Buffer overrun reading command from server.\n");
			return 1;
		}

		char *saveptr;
		const char *keyword = strtok_r (buf, NETWORK_DELIM, &saveptr);
		if (keyword == NULL) {
			fprintf (stderr, "* ERROR: Cannot extract keyword from received message.\n");
			return 1;
		}

		if (strcmp (keyword, "RENDER") == 0) {
			int tid, frame, mdlen, w, h;

			const char *arg = strtok_r (NULL, NETWORK_DELIM, &saveptr);
			if (arg == NULL) {
				fprintf (stderr, "* ERROR: Cannot extract thread id from RENDER message.\n");
				return 1;
			}
			tid = atoi (arg);
			if (tid < 0 || tid >= state->thread_count) {
				fprintf (stderr, "* ERROR: Invalid thread id in RENDER message.\n");
				return 1;
			}

			arg = strtok_r (NULL, NETWORK_DELIM, &saveptr);
			if (arg == NULL) {
				fprintf (stderr, "* ERROR: Cannot extract frame number from RENDER message.\n");
				return 1;
			}
			frame = atoi (arg);
			if (frame < 0) {
				fprintf (stderr, "* ERROR: Invalid frame number in RENDER message.\n");
				return 1;
			}

			arg = strtok_r (NULL, NETWORK_DELIM, &saveptr);
			if (arg == NULL) {
				fprintf (stderr, "* ERROR: Cannot extract body length from RENDER message.\n");
				return 1;
			}
			mdlen = atoi (arg);
			if (mdlen < 0) {
				fprintf (stderr, "* ERROR: Invalid body length in RENDER message.\n");
				return 1;
			}

			arg = strtok_r (NULL, NETWORK_DELIM, &saveptr);
			if (arg == NULL) {
				fprintf (stderr, "* ERROR: Cannot extract image width from RENDER message.\n");
				return 1;
			}
			w = atoi (arg);
			if (w <= 0) {
				fprintf (stderr, "* ERROR: Invalid image width in RENDER message.\n");
				return 1;
			}

			arg = strtok_r (NULL, NETWORK_DELIM, &saveptr);
			if (arg == NULL) {
				fprintf (stderr, "* ERROR: Cannot extract image height from RENDER message.\n");
				return 1;
			}
			h = atoi (arg);
			if (h <= 0) {
				fprintf (stderr, "* ERROR: Invalid image height in RENDER message.\n");
				return 1;
			}

			char mdbuf[mdlen + 1];
			mdbuf[mdlen] = 0;
			if (fread (mdbuf, mdlen, 1, f) < 1) {
				if (feof (f))
					fprintf (stderr, "* ERROR: Server unexpectedly closed the connection.\n");
				else
					fprintf (stderr, "* ERROR: Reading body of RENDER message: %s\n", strerror (errno));
				return 1;
			}

			g_mutex_lock (tinfo[tid].mutex);
			tinfo[tid].frame = frame;
			tinfo[tid].w = w;
			tinfo[tid].h = h;
			char errbuf[128];
			if (!sread_mandeldata (mdbuf, &tinfo[tid].md, errbuf, sizeof (errbuf))) {
				fprintf (stderr, "* ERROR: Parsing body of RENDER message: %s\n", errbuf);
				return 1;
			}
			g_mutex_unlock (tinfo[tid].mutex);
			g_cond_signal (tinfo[tid].cond);
		} else if (strcmp (keyword, "TERMINATE") == 0) {
			fprintf (stderr, "* INFO: Server requested termination.\n");
			return 0;
		} else {
			fprintf (stderr, "* ERROR: Unknown message received from server.\n");
			return 1;
		}
	}

	return 0;
}
