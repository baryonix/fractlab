#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <unistd.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <poll.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <strings.h>

#include "anim.h"
#include "util.h"
#include "file.h"
#include "defs.h"
#include "fractal-render.h"
#include "render-png.h"


#define NETWORK_DELIM " \t\r\n"


struct work_list_item {
	int i;
	struct mandeldata md;
	struct work_list_item *next;
};


typedef enum client_state_enum {
	CSTATE_INITIAL,
	CSTATE_WORKING
} client_state_t;


struct net_client {
	struct sockaddr_storage addr;
	socklen_t addrlen;
	int fd;
	FILE *f;
	char input_buf[1024], output_buf[65536];
	size_t input_pos, output_size, output_pos;
	unsigned thread_count;
	client_state_t state;
	struct work_list_item **work_items;
	bool dying;
	char name[256];
};


struct anim_state {
	GMutex *mutex;
	unsigned thread_count;
	struct work_list_item *work_list;
	unsigned socket_count;
	struct pollfd *pollfds;
	struct net_client **clients;
};


static gpointer thread_func (gpointer data);
static gpointer network_thread (gpointer data);
static struct work_list_item *generate_work_list (frame_func_t frame_func, void *data);
static void free_work_list (struct work_list_item *list);
static void free_work_list_item (struct work_list_item *item);
static struct work_list_item *get_work (struct anim_state *state);
static void add_socket (struct anim_state *state, int fd, struct net_client *client);
static int create_listener (const struct addrinfo *ai);
static void accept_connection (struct anim_state *state, unsigned i);
static bool send_render_command (struct anim_state *state, unsigned client_id, unsigned thread_id, unsigned frame_no, const struct mandeldata *md);
static bool process_net_input (struct net_client *client);


static struct color colors[COLORS];

static long clock_ticks;

gint img_width = 200, img_height = 200, frame_count = 0;
static gint zoom_threads = 1;
static gint compression = -1;
static gint start_frame = 0;
static const gchar *network_port = NULL;
static gint no_dns = 0;


static GOptionEntry option_entries[] = {
	{"frames", 'n', 0, G_OPTION_ARG_INT, &frame_count, "# of frames in animation", "N"},
	{"start-frame", 'S', 0, G_OPTION_ARG_INT, &start_frame, "Start rendering at frame N", "N"},
	{"width", 'W', 0, G_OPTION_ARG_INT, &img_width, "Image width", "PIXELS"},
	{"height", 'H', 0, G_OPTION_ARG_INT, &img_height, "Image height", "PIXELS"},
	{"threads", 'T', 0, G_OPTION_ARG_INT, &zoom_threads, "Parallel rendering with N threads", "N"},
	{"compression", 'C', 0, G_OPTION_ARG_INT, &compression, "Compression level for PNG output (0..9)", "LEVEL"},
	{"listen", 'l', 0, G_OPTION_ARG_STRING, &network_port, "Listen on PORT for network rendering", "PORT"},
	{"no-dns", 'N', 0, G_OPTION_ARG_NONE, &no_dns, "Don't resolve hostnames of clients via DNS"},
	{NULL}
};


GOptionGroup *
anim_get_option_group (void)
{
	GOptionGroup *group = g_option_group_new ("anim", "Animation Options", "Animation Options", NULL, NULL);
	g_option_group_add_entries (group, option_entries);
	return group;
}


void
anim_render (frame_func_t frame_func, void *data)
{
	clock_ticks = -1;
#ifdef _SC_CLK_TCK
	clock_ticks = sysconf (_SC_CLK_TCK);
#endif
#ifdef CLK_TCK
	if (clock_ticks == -1)
		clock_ticks = CLK_TCK;
#endif
	struct anim_state state[1];
	memset (state, 0, sizeof (*state));
	fprintf (stderr, "* DEBUG: Generating work list...\n");
	state->work_list = generate_work_list (frame_func, data);
	if (state->work_list == NULL)
		return;

	int i;
	for (i = 0; i < COLORS; i++) {
		colors[i].r = (unsigned short) (sin (2 * M_PI * i / COLORS) * 127) + 128;
		colors[i].g = (unsigned short) (sin (4 * M_PI * i / COLORS) * 127) + 128;
		colors[i].b = (unsigned short) (sin (6 * M_PI * i / COLORS) * 127) + 128;
	}

	g_thread_init (NULL);
	GThread *threads[zoom_threads], *net_thread = NULL;
	state->mutex = g_mutex_new ();
	if (network_port != NULL)
		net_thread = g_thread_create (network_thread, state, TRUE, NULL);
	for (i = 0; i < zoom_threads; i++)
		threads[i] = g_thread_create (thread_func, state, TRUE, NULL);
	if (network_port != NULL)
		g_thread_join (net_thread);
	for (i = 0; i < zoom_threads; i++)
		g_thread_join (threads[i]);
	g_mutex_free (state->mutex);
}


static gpointer
thread_func (gpointer data)
{
	struct anim_state *state = (struct anim_state *) data;
	while (TRUE) {
		g_mutex_lock (state->mutex);
		struct work_list_item *item = get_work (state);
		g_mutex_unlock (state->mutex);
		if (item == NULL)
			break;
		char filename[256];
		snprintf (filename, sizeof (filename), "file%06d.png", (int) item->i);

		/*
		 * Unfortunately, there is no way of determining the amount of CPU
		 * time used by the current thread.
		 * On NetBSD, CLOCK_THREAD_CPUTIME_ID is not defined at all.
		 * On Solaris, CLOCK_THREAD_CPUTIME_ID is defined, but
		 * clock_gettime (CLOCK_THREAD_CPUTIME_ID, ...) fails.
		 * On Linux, clock_gettime (CLOCK_THREAD_CPUTIME_ID, ...) succeeds,
		 * but it returns the CPU time usage of the whole process intead. Bummer!
		 * Linux also has pthread_getcpuclockid(), but it apparently always fails.
		 */
		unsigned bits;
#if defined (_SC_CLK_TCK) || defined (CLK_TCK)
		struct tms time_before, time_after;
		bool clock_ok = zoom_threads == 1 && clock_ticks > 0;
		clock_ok = clock_ok && times (&time_before) != (clock_t) -1;
#endif
		render_to_png (&item->md, filename, compression, &bits, colors, img_width, img_height);

#if defined (_SC_CLK_TCK) || defined (CLK_TCK)
		clock_ok = clock_ok && times (&time_after) != (clock_t) -1;
#endif

#ifdef _POSIX_THREAD_SAFE_FUNCTIONS
		flockfile (stderr);
#endif /* _POSIX_THREAD_SAFE_FUNCTIONS */
#if defined (_SC_CLK_TCK) || defined (CLK_TCK)
		if (clock_ok)
			fprintf (stderr, "[%7.1fs CPU] ", (double) (time_after.tms_utime + time_after.tms_stime - time_before.tms_utime - time_before.tms_stime) / clock_ticks);
#endif
		fprintf (stderr, "Frame %u done", item->i);
		if (bits == 0)
			fprintf (stderr, ", using FP arithmetic");
		else
			fprintf (stderr, ", using MP arithmetic (%d bits precision)", bits);
		fprintf (stderr, ".\n");
#ifdef _POSIX_THREAD_SAFE_FUNCTIONS
		funlockfile (stderr);
#endif /* _POSIX_THREAD_SAFE_FUNCTIONS */

		free_work_list_item (item);
	}
	return NULL;
}


static struct work_list_item *
generate_work_list (frame_func_t frame_func, void *data)
{
	struct work_list_item *cur = malloc (sizeof (*cur)), *first = cur;
	unsigned i = start_frame;

	if (i >= frame_count)
		return NULL; /* nothing to do */

	while (true) {
		cur->next = NULL;

		if (cur == NULL) {
			fprintf (stderr, "* ERROR: Out of memory while generating work list.\n");
			free_work_list (first);
			return NULL;
		}

		cur->i = i;
		frame_func (data, &cur->md, i);

		if (++i >= frame_count)
			break;

		cur->next = malloc (sizeof (*cur->next));
		cur = cur->next;
	}
	return first;
}


static void
free_work_list (struct work_list_item *list)
{
	while (list != NULL) {
		struct work_list_item *next = list->next;
		free_work_list_item (list);
		list = next;
	}
}


static void
free_work_list_item (struct work_list_item *item)
{
	mandeldata_clear (&item->md);
	free (item);
}


static struct work_list_item *
get_work (struct anim_state *state)
{
	struct work_list_item *r = state->work_list;
	if (r != NULL)
		state->work_list = r->next;
	return r;
}


/*
 * A note about locking: We currently don't lock the anim_state when we're
 * only accessing the network-specific parts of the state, as they are
 * really only accessed by this single thread.
 */
static gpointer
network_thread (gpointer data)
{
	struct anim_state *state = (struct anim_state *) data;
	struct addrinfo *ai = NULL;
	struct addrinfo aihints = {
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
		.ai_protocol = IPPROTO_TCP,
		.ai_flags = AI_PASSIVE
	};
	int r = getaddrinfo (NULL, network_port, &aihints, &ai);
	if (r != 0) {
		fprintf (stderr, "* ERROR: Cannot resolve network service name: %s\n", gai_strerror (r));
		return NULL;
	}
	struct addrinfo *aicur;
	unsigned listeners = 0;
	for (aicur = ai; aicur != NULL; aicur = aicur->ai_next) {
		int l = create_listener (aicur);
		if (l >= 0) {
			listeners++;
			add_socket (state, l, NULL);
		}
	}
	freeaddrinfo (ai);

	if (listeners == 0) {
		fprintf (stderr, "* ERROR: Could not create any listening sockets.\n");
		return NULL;
	}

	fprintf (stderr, "* INFO: Created %u listening sockets.\n", listeners);

	while (true) {
		int r = poll (state->pollfds, state->socket_count, -1);
		if (r < 0) {
			fprintf (stderr, "* ERROR: poll() failed: %s\n", strerror (errno));
			continue;
		} else if (r == 0) {
			fprintf (stderr, "* WARNING: poll() found no events, doing another iteration...\n");
			continue;
		}

		unsigned i, socket_count = state->socket_count;
		for (i = 0; i < socket_count; i++) {
			struct net_client *client = state->clients[i];
			if ((state->pollfds[i].revents & POLLIN) != 0) {
				/* There is input to be processed on this fd. */
				if (client != NULL) {
					/* This is a client connection on which we received data. */
					if (!process_net_input (client))
						client->dying = true;
				} else {
					/* This is a listening socket on which a new connection has just arrived. */
					accept_connection (state, i);
				}
			}

			/*
			 * We make the assumption that POLLOUT only ever gets set in revents if it was
			 * set in events before. We also assume that we only ever set it in events if
			 * there is data to send. Some extra consistency checking would be nice...
			 */
			if (client != NULL && !client->dying && (state->pollfds[i].revents & POLLOUT) != 0) {
				errno = 0;
				size_t written = fwrite (client->output_buf + client->output_pos, 1, client->output_size - client->output_pos, client->f);
				if (errno != 0 && errno != EAGAIN) {
					fprintf (stderr, "* ERROR: Sending to client: %s\n", strerror (errno));
					client->dying = true;
					continue;
				}

				client->output_pos += written;
				if (client->output_pos >= client->output_size) {
					/* Buffer has been completely sent. */
					client->output_pos = 0;
					client->output_size = 0;
					state->pollfds[i].events &= ~POLLOUT;
				}
			}
		}

		/* XXX This would be a good place to remove any dead clients from our data structures. */
		for (i = 0; i < state->socket_count; i++) {
			struct net_client *client = state->clients[i];
			int j;

			if (client == NULL || !client->dying)
				continue;

			/* No error checks here, there's nothing we could do anyway. */
			fputs ("TERMINATE\r\n", client->f);
			fclose (client->f);
			close (client->fd);

			g_mutex_lock (state->mutex);
			for (j = 0; j < client->thread_count; j++) {
				struct work_list_item *item = client->work_items[j];
				if (item == NULL)
					continue;
				fprintf (stderr, "* WARNING: Will have to re-render frame %d due to client failure.\n", item->i);
				item->next = state->work_list;
				state->work_list = item;
			}
			g_mutex_unlock (state->mutex);

			free (client->work_items);
			free (client);

			state->socket_count--;
			if (i < state->socket_count) {
				memcpy (&state->pollfds[i], &state->pollfds[state->socket_count], sizeof (*state->pollfds));
				state->clients[i] = state->clients[state->socket_count];
			}
		}

		/*
		 * After we did all the I/O stuff for this iteration, we now give
		 * work to any idle clients.
		 */
		bool all_done = false;
		for (i = 0; !all_done && i < state->socket_count; i++) {
			int j;
			struct net_client *client = state->clients[i];
			if (client == NULL)
				continue; /* skip over listening sockets */

			g_mutex_lock (state->mutex);
			for (j = 0; j < client->thread_count; j++) {
				if (client->work_items[j] != NULL)
					continue; /* got work already */
				struct work_list_item *item = get_work (state);
				if (item == NULL) {
					all_done = true;
					break;
				}
				send_render_command (state, i, j, item->i, &item->md);
				client->work_items[j] = item;
			}
			g_mutex_unlock (state->mutex);
		}
	}
	return NULL;
}


static void
add_socket (struct anim_state *state, int fd, struct net_client *client)
{
	state->socket_count++;
	state->pollfds = realloc (state->pollfds, state->socket_count * sizeof (*state->pollfds));
	state->clients = realloc (state->clients, state->socket_count * sizeof (*state->clients));
	state->pollfds[state->socket_count - 1].fd = fd;
	state->pollfds[state->socket_count - 1].events = POLLIN;
	state->clients[state->socket_count - 1] = client;
}


static int
create_listener (const struct addrinfo *ai)
{
	int sock = socket (ai->ai_family, ai->ai_socktype, ai->ai_protocol);
	if (sock < 0) {
		fprintf (stderr, "* ERROR: socket(): %s\n", strerror (errno));
		return -1;
	}

	/* XXX Is it any good to set O_NONBLOCK for the listening socket? */
	if (fcntl (sock, F_SETFL, O_NONBLOCK) < 0) {
		fprintf (stderr, "* ERROR: fcntl(set O_NONBLOCK): %s\n", strerror (errno));
		close (sock);
		return -1;
	}

	if (bind (sock, ai->ai_addr, ai->ai_addrlen) < 0) {
		fprintf (stderr, "* ERROR: bind(): %s\n", strerror (errno));
		close (sock);
		return -1;
	}

	/* XXX backlog is hardcoded */
	if (listen (sock, 10) < 0) {
		fprintf (stderr, "* ERROR: listen(): %s\n", strerror (errno));
		close (sock);
		return -1;
	}

	return sock;
}


static void
accept_connection (struct anim_state *state, unsigned i)
{
	struct net_client *client = malloc (sizeof (*client));
	if (client == NULL) {
		fprintf (stderr, "* ERROR: accept_connection(): Out of memory.\n");
		return;
	}
	memset (client, 0, sizeof (*client));
	client->state = CSTATE_INITIAL;

	client->addrlen = sizeof (client->addr);
	client->fd = accept (state->pollfds[i].fd, (struct sockaddr *) &client->addr, &client->addrlen);
	if (client->fd < 0) {
		fprintf (stderr, "* ERROR: accept(): %s\n", strerror (errno));
		free (client);
		return;
	}

	int niflags = NI_NOFQDN | NI_NUMERICSERV;
	if (no_dns)
		niflags |= NI_NUMERICHOST;
	int r = getnameinfo ((struct sockaddr *) &client->addr, client->addrlen, client->name, sizeof (client->name), NULL, 0, niflags);
	if (r == 0) {
		/*
		 * IPv6-mapped IPv4 addresses look crappy, we turn them into the
		 * normal IPv4 representation. And yes, we do it the dirty way.
		 */
		static const char v4map_prefix[] = "::ffff:";
		static const size_t v4map_prefix_len = sizeof (v4map_prefix) - 1;
		if (strncasecmp (client->name, v4map_prefix, v4map_prefix_len) == 0)
			memmove (client->name, client->name + v4map_prefix_len, strlen (client->name + v4map_prefix_len) + 1);
	} else {
		fprintf (stderr, "* WARNING: getnameinfo() failed: %s\n", gai_strerror (r));
		my_safe_strcpy (client->name, "???", sizeof (client->name));
	}
	fprintf (stderr, "* INFO: Accepted new connection from [%s]\n", client->name);

	if (fcntl (client->fd, F_SETFL, O_NONBLOCK) < 0) {
		fprintf (stderr, "* ERROR: fcntl(set O_NONBLOCK): %s\n", strerror (errno));
		close (client->fd);
		free (client);
		return;
	}

	client->f = fdopen (client->fd, "r+");
	if (client->f == NULL) {
		fprintf (stderr, "* ERROR: fdopen(): %s\n", strerror (errno));
		close (client->fd);
		free (client);
	}

	/* Just to be safe... */
	if (setvbuf (client->f, NULL, _IONBF, 0) != 0)
		fprintf (stderr, "* WARNING: setvbuf(): %s\n", strerror (errno));

	add_socket (state, client->fd, client);
}


static bool
send_render_command (struct anim_state *state, unsigned client_id, unsigned thread_id, unsigned frame_no, const struct mandeldata *md)
{
	/*
	 * XXX This doesn't look exactly like an exercise in efficiency...
	 * The io_buffer code should get a bit smarter, which would probably
	 * make most of the byte counting performed here superfluous.
	 */
	struct net_client *client = state->clients[client_id];
	char mdbuf1[4096], mdbuf2[4096];
	struct io_buffer iob1[1], iob2[1];
	struct io_stream ios1[1], ios2[1];
	char errbuf[1024];

	if (!io_buffer_init (iob1, mdbuf1, sizeof (mdbuf1))) {
		fprintf (stderr, "* ERROR: io_buffer_init failed\n");
		return false;
	}
	io_stream_init_buffer (ios1, iob1);
	if (!generic_write_mandeldata (ios1, md, true, errbuf, sizeof (errbuf))) {
		fprintf (stderr, "* ERROR: generic_write_mandeldata failed: %s\n", errbuf);
		io_buffer_clear (iob1);
		return false;
	}

	if (!io_buffer_init (iob2, mdbuf2, sizeof (mdbuf2))) {
		fprintf (stderr, "* ERROR: io_buffer_init failed\n");
		io_buffer_clear (iob1);
		return false;
	}
	io_stream_init_buffer (ios2, iob2);

	if (my_printf (ios2, errbuf, sizeof (errbuf), "RENDER %u %u %lu %u %u\r\n%s", thread_id, frame_no, (unsigned long) iob1->pos, (unsigned) img_width, (unsigned) img_height, iob1->buf) < 0) {
		fprintf (stderr, "* ERROR: writing RENDER request to buffer: %s\n", errbuf);
		io_buffer_clear (iob1);
		io_buffer_clear (iob2);
		return false;
	}

	if (client->output_size + iob2->pos + 1 > sizeof (client->output_buf)) {
		fprintf (stderr, "* ERROR: Output buffer too small when sending to client.\n");
		io_buffer_clear (iob1);
		io_buffer_clear (iob2);
		return false;
	}

	my_safe_strcpy (client->output_buf + client->output_size, iob2->buf, sizeof (client->output_buf) - client->output_size);
	client->output_size += iob2->pos;
	state->pollfds[client_id].events |= POLLOUT;

	/* That's it... */
	io_buffer_clear (iob1);
	io_buffer_clear (iob2);
	return true;
}


static bool
process_net_input (struct net_client *client)
{
	if (fgets (client->input_buf + client->input_pos, sizeof (client->input_buf) - client->input_pos, client->f) == NULL) {
		if (feof (client->f))
			fprintf (stderr, "* WARNING: EOF from client\n");
		else
			fprintf (stderr, "* WARNING: error from fgets: %s\n", strerror (errno));
		 return false;
	}
	client->input_pos += strlen (client->input_buf + client->input_pos);
	if (client->input_pos >= sizeof (client->input_buf) - 1 && client->input_buf[client->input_pos - 1] != '\n') {
		fprintf (stderr, "* WARNING: Buffer overrun on network input. Nasty, nasty client. Dropping connection.\n");
		return false;
	}
	if (client->input_pos >= 1 && client->input_buf[client->input_pos - 1] == '\n') {
		/* Complete line read, parse it. */
		client->input_pos = 0;
		char *saveptr;
		const char *keyword = strtok_r (client->input_buf, NETWORK_DELIM, &saveptr);
		if (keyword == NULL) {
			fprintf (stderr, "* ERROR: Cannot extract keyword from message.\n");
			return false;
		}

		if (strcmp (keyword, "MOIN") == 0) {
			if (client->state != CSTATE_INITIAL) {
				fprintf (stderr, "* ERROR: MOIN message while not in CSTATE_INITIAL.\n");
				return false;
			}
			const char *arg1 = strtok_r (NULL, NETWORK_DELIM, &saveptr);
			if (arg1 == NULL) {
				fprintf (stderr, "* ERROR: Cannot extract argument from MOIN message.\n");
				return false;
			}
			int j = atoi (arg1);
			if (j < 1) {
				fprintf (stderr, "* ERROR: Invalid thread count in MOIN message.\n");
				return false;
			}
			fprintf (stderr, "* DEBUG: Received MOIN message with %d threads.\n", j);
			client->thread_count = j;
			client->state = CSTATE_WORKING;
			client->work_items = malloc (client->thread_count * sizeof (*client->work_items));
			memset (client->work_items, 0, client->thread_count * sizeof (*client->work_items));
		} else if (client->state == CSTATE_INITIAL) {
			fprintf (stderr, "* ERROR: Only MOIN is allowed in CSTATE_INITIAL.\n");
			return false;
		} else if (strcmp (keyword, "DONE") == 0) {
			const char *arg1 = strtok_r (NULL, NETWORK_DELIM, &saveptr);
			if (arg1 == NULL) {
				fprintf (stderr, "* ERROR: Cannot extract argument from DONE message.\n");
				return false;
			}
			int j = atoi (arg1);
			if (j < 0 || j >= client->thread_count) {
				fprintf (stderr, "* ERROR: Invalid thread id in DONE message.\n");
				return false;
			}
			fprintf (stderr, "Frame %d done, precision unknown, on %s.\n", client->work_items[j]->i, client->name);
			/* XXX save the information that this client successfully rendered frame i */
			free_work_list_item (client->work_items[j]);
			client->work_items[j] = NULL;
		} else {
			fprintf (stderr, "* DEBUG: unknown keyword [%s].\n", keyword);
			return false;
		}
	}
	return true;
}
