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
	unsigned frame;
};


static gint thread_count = 3;
static struct color colors[COLORS];


gpointer
worker_thread (gpointer data)
{
	struct thread_info *info = (struct thread_info *) data;
	struct worker_state *state = info->state;

	while (true) {
		fprintf (stderr, "* INFO: Thread %u waiting for work\n", info->thread_id);
		g_cond_wait (info->cond, info->mutex);
		fprintf (stderr, "* INFO: Thread %u rendering frame %u\n", info->thread_id, info->frame);
		char buf[256];
		snprintf (buf, sizeof (buf), "file%06u.png", info->frame);
		/* XXX much stuff hard-coded here */
		render_to_png (&info->md, buf, 9, NULL, colors, 500, 500);
		size_t mlen = snprintf (buf, sizeof (buf), "DONE %u\r\n", info->thread_id);
		write (state->connection, buf, mlen);
	}

	return NULL;
}

int
main (int argc, char **argv)
{
	if (argc != 3) {
		fprintf (stderr, "* USAGE: %s <host> <port>\n", argv[0]);
		return 1;
	}

	const char *hostname = argv[1];
	int port = atoi (argv[2]);

	if (port < 1 || port > 65535) {
		fprintf (stderr, "* ERROR: invalid port number\n");
		return 1;
	}

	/* XXX should use getipnodebyname() or even getaddrinfo() instead */
	struct hostent *hent = gethostbyname (hostname);
	if (hent == NULL) {
		fprintf (stderr, "* ERROR: %s: %s\n", hostname, hstrerror (h_errno));
		return 1;
	}

	int s = socket (hent->h_addrtype, SOCK_STREAM, IPPROTO_TCP);
	if (s < 0) {
		perror ("socket()");
		return 1;
	}

	struct sockaddr_storage saddr;
	socklen_t addrlen = 0;
	saddr.ss_family = hent->h_addrtype;
	switch (hent->h_addrtype) {
		case AF_INET: {
			struct sockaddr_in *sin = (struct sockaddr_in *) &saddr;
			memcpy (&sin->sin_addr, hent->h_addr_list[0], sizeof (sin->sin_addr));
			sin->sin_port = htons (port);
			addrlen = sizeof (struct sockaddr_in);
			break;
		}

		case AF_INET6: {
			struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *) &saddr;
			memcpy (&sin6->sin6_addr, hent->h_addr_list[0], sizeof (sin6->sin6_addr));
			sin6->sin6_port = htons (port);
			addrlen = sizeof (struct sockaddr_in6);
			break;
		}

		default: {
			fprintf (stderr, "ERROR: Unknown address type (%d)\n", (int) hent->h_addrtype);
			return 1;
		}
	}

	if (connect (s, (struct sockaddr *) &saddr, addrlen) < 0) {
		perror ("connect()");
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

	int i;
	for (i = 0; i < state->thread_count; i++) {
		tinfo[i].state = state;
		tinfo[i].thread_id = i;
		tinfo[i].mutex = g_mutex_new ();
		g_mutex_lock (tinfo[i].mutex);
		tinfo[i].cond = g_cond_new ();
		tinfo[i].terminate = false;
		tinfo[i].thread = g_thread_create (worker_thread, (gpointer) &tinfo[i], true, NULL);
	}

	fprintf (f, "MOIN %u\r\n", state->thread_count);
	fflush (f);

	for (i = 0; i < COLORS; i++) {
		colors[i].r = (unsigned short) (sin (2 * M_PI * i / COLORS) * 127) + 128;
		colors[i].g = (unsigned short) (sin (4 * M_PI * i / COLORS) * 127) + 128;
		colors[i].b = (unsigned short) (sin (6 * M_PI * i / COLORS) * 127) + 128;
	}

	while (true) {
		char buf[256];
		errno = 0;
		fgets (buf, sizeof (buf), f);
		if (errno != 0) {
			fprintf (stderr, "* ERROR in fgets: %s\n", strerror (errno));
			return 1;
		}

		char *saveptr;
		const char *keyword = strtok_r (buf, NETWORK_DELIM, &saveptr);
		if (keyword == NULL) {
			fprintf (stderr, "* ERROR: Cannot extract keyword from received message.\n");
			return 1;
		}

		if (strcmp (keyword, "RENDER") == 0) {
			int tid, frame, mdlen;

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

			char *mdbuf = malloc (mdlen + 1);
			mdbuf[mdlen] = 0;
			errno = 0;
			fread (mdbuf, mdlen, 1, f);
			if (errno != 0) {
				fprintf (stderr, "* ERROR: Reading body of RENDER message: %s\n", strerror (errno));
				return 1;
			}

			g_mutex_lock (tinfo[tid].mutex);
			tinfo[tid].frame = frame;
			char errbuf[128];
			if (!sread_mandeldata (mdbuf, &tinfo[tid].md, errbuf, sizeof (errbuf))) {
				fprintf (stderr, "* ERROR: Parsing body of RENDER message: %s\n", errbuf);
				return 1;
			}
			g_mutex_unlock (tinfo[tid].mutex);
			g_cond_signal (tinfo[tid].cond);
		} else {
			fprintf (stderr, "* ERROR: Unknown message received from server.\n");
			return 1;
		}
	}

	return 0;
}
