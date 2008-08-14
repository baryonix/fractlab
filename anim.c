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


typedef enum socket_type_enum {
	SOCK_TYPE_LISTENER,
	SOCK_TYPE_CLIENT,
	SOCK_TYPE_TERM /* termination indicator (the reading end of a pipe) */
} socket_type_t;


union socket_data {
	struct net_client *client;
};


struct socket_desc {
	socket_type_t type;
	int fd;
	union socket_data data;
};


struct net_client {
	struct sockaddr_storage addr;
	socklen_t addrlen;
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
	struct socket_desc *sockets;
	unsigned net_threads_total;
	unsigned net_threads_busy; /* number of net threads currently working (not including idle ones) */
	int term_pipe_r, term_pipe_w;
	char **client_index;
};


static gpointer thread_func (gpointer data);
static gpointer network_thread (gpointer data);
static struct work_list_item *generate_work_list (frame_func_t frame_func, void *data);
static void free_work_list (struct work_list_item *list);
static void free_work_list_item (struct work_list_item *item);
static struct work_list_item *get_work (struct anim_state *state);
static int add_socket (struct anim_state *state, socket_type_t type, int fd);
static int create_listener (const struct addrinfo *ai);
static void accept_connection (struct anim_state *state, unsigned i);
static bool send_render_command (struct anim_state *state, unsigned client_id, unsigned thread_id, unsigned frame_no, const struct mandeldata *md);
static bool process_net_input (struct anim_state *state, unsigned i);
static void disconnect_client (struct anim_state *state, unsigned i);


static struct color colors[COLORS];

static long clock_ticks;

gint img_width = 200, img_height = 200, frame_count = 0;
static gint zoom_threads = 1;
static gint compression = -1;
static gint start_frame = 0;
static const gchar *network_port = NULL;
static gint no_dns = 0;
static const gchar *index_file = NULL;


static GOptionEntry option_entries[] = {
	{"frames", 'n', 0, G_OPTION_ARG_INT, &frame_count, "# of frames in animation", "N"},
	{"start-frame", 'S', 0, G_OPTION_ARG_INT, &start_frame, "Start rendering at frame N", "N"},
	{"width", 'W', 0, G_OPTION_ARG_INT, &img_width, "Image width", "PIXELS"},
	{"height", 'H', 0, G_OPTION_ARG_INT, &img_height, "Image height", "PIXELS"},
	{"threads", 'T', 0, G_OPTION_ARG_INT, &zoom_threads, "Parallel rendering with N threads", "N"},
	{"compression", 'C', 0, G_OPTION_ARG_INT, &compression, "Compression level for PNG output (0..9)", "LEVEL"},
	{"listen", 'l', 0, G_OPTION_ARG_STRING, &network_port, "Listen on PORT for network rendering", "PORT"},
	{"no-dns", 'N', 0, G_OPTION_ARG_NONE, &no_dns, "Don't resolve hostnames of clients via DNS"},
	{"index-file", 'I', 0, G_OPTION_ARG_FILENAME, &index_file, "Record in FILE which frame was rendered on which client", "FILE"},
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
	if (index_file != NULL) {
		state->client_index = malloc (frame_count * sizeof (*state->client_index));
		memset (state->client_index, 0, frame_count * sizeof (*state->client_index));
	}
	if (network_port != NULL) {
		int pipefd[2];
		if (pipe (pipefd) < 0) {
			fprintf (stderr, "* ERROR: pipe() failed: %s\n", strerror (errno));
			return;
		}
		state->term_pipe_r = pipefd[0];
		state->term_pipe_w = pipefd[1];
		net_thread = g_thread_create (network_thread, state, TRUE, NULL);
	} else {
		state->term_pipe_r = -1;
		state->term_pipe_w = -1;
	}
	for (i = 0; i < zoom_threads; i++)
		threads[i] = g_thread_create (thread_func, state, TRUE, NULL);
	if (network_port != NULL)
		g_thread_join (net_thread);
	for (i = 0; i < zoom_threads; i++)
		g_thread_join (threads[i]);
	if (index_file != NULL && state->client_index != NULL) {
		FILE *ixfile = fopen (index_file, "w");
		if (ixfile != NULL) {
			fprintf (stderr, "* INFO: Writing index to file [%s].\n", index_file);
			for (i = start_frame; i < frame_count; i++)
				fprintf (ixfile, "%d %s\n", i, (state->client_index[i] != NULL) ? state->client_index[i] : "<NOT DONE>");
			fclose (ixfile);
		} else
			fprintf (stderr, "* ERROR: Writing index file [%s]: %s\n", index_file, strerror (errno));
	}
	g_mutex_free (state->mutex);
}


static gpointer
thread_func (gpointer data)
{
	struct anim_state *state = (struct anim_state *) data;
	while (TRUE) {
		g_mutex_lock (state->mutex);
		struct work_list_item *item = get_work (state);
		if (state->work_list == NULL && state->term_pipe_w >= 0) {
			/*
			 * Close the writing end of the pipe. This will cause the network
			 * thread to wake up if it is currently in poll(). If no network
			 * clients are left working, it will then terminate.
			 */
			close (state->term_pipe_w);
			state->term_pipe_w = -1;
		}
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
		bool clock_ok = zoom_threads == 1 && network_port == NULL && clock_ticks > 0;
		clock_ok = clock_ok && times (&time_before) != (clock_t) -1;
#endif
		render_to_png (&item->md, filename, compression, &bits, colors, img_width, img_height, 1);

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

		if (state->client_index != NULL) {
			g_mutex_lock (state->mutex);
			state->client_index[item->i] = "<LOCAL>";
			g_mutex_unlock (state->mutex);
		}
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
			add_socket (state, SOCK_TYPE_LISTENER, l);
		}
	}
	freeaddrinfo (ai);

	if (listeners == 0) {
		fprintf (stderr, "* ERROR: Could not create any listening sockets.\n");
		return NULL;
	}

	fprintf (stderr, "* INFO: Created %u listening sockets.\n", listeners);

	add_socket (state, SOCK_TYPE_TERM, state->term_pipe_r);

	while (true) {
		int nevents = poll (state->pollfds, state->socket_count, -1);
		if (nevents < 0) {
			if (errno != EINTR)
				fprintf (stderr, "* WARNING: poll() failed: %s\n", strerror (errno));
			continue;
		}

		unsigned i, socket_count = state->socket_count;
		int events_left = nevents;
		for (i = 0; events_left > 0 && i < socket_count; i++) {
			if (state->pollfds[i].revents != 0)
				events_left--;
			else
				continue;

			switch (state->sockets[i].type) {
				case SOCK_TYPE_LISTENER: {
					if ((state->pollfds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
						fprintf (stderr, "* UH-OH: poll() detected error on listening socket. Trying to continue...\n");
						break;
					}
					if ((state->pollfds[i].revents & POLLIN) != 0)
						accept_connection (state, i);
					break;
				}

				case SOCK_TYPE_CLIENT: {
					struct net_client *client = state->sockets[i].data.client;

					if ((state->pollfds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
						fprintf (stderr, "* WARNING: poll() detected error on connection to client %s. Will be disconnected.\n", client->name);
						client->dying = true;
						break;
					}

					if ((state->pollfds[i].revents & POLLIN) != 0 && !process_net_input (state, i)) {
						client->dying = true;
						break;
					}

					/*
					 * We make the assumption that POLLOUT only ever gets set in revents if it was
					 * set in events before. We also assume that we only ever set it in events if
					 * there is data to send. Some extra consistency checking would be nice...
					 */
					if ((state->pollfds[i].revents & POLLOUT) != 0) {
						errno = 0;
						size_t written = fwrite (client->output_buf + client->output_pos, 1, client->output_size - client->output_pos, client->f);
						if (errno != 0 && errno != EAGAIN) {
							fprintf (stderr, "* ERROR: Sending to client: %s\n", strerror (errno));
							client->dying = true;
							break;
						}

						client->output_pos += written;
						if (client->output_pos >= client->output_size) {
							/* Buffer has been completely sent. */
							client->output_pos = 0;
							client->output_size = 0;
							state->pollfds[i].events &= ~POLLOUT;
						}
					}

					break;
				}

				case SOCK_TYPE_TERM:
					/*
					 * We don't do anything here, we only need the pipe to
					 * wake us up while poll()ing.
					 */
					break;
			}
		}

		/*
		 * Scan the client list again and remove any clients that are in
		 * dying state. Put their current work items back to the work list
		 * for retry.
		 */
		for (i = 0; i < state->socket_count; i++)
			if (state->sockets[i].type == SOCK_TYPE_CLIENT && state->sockets[i].data.client->dying) {
				disconnect_client (state, i);
				fprintf (stderr, "* INFO: Client %s disconnected, total capacity now %u threads.\n", state->sockets[i].data.client->name, (unsigned) zoom_threads + state->net_threads_total); /* XXX wrong client name is output here */

			}

		/*
		 * After we did all the I/O stuff for this iteration, we now give
		 * work to any idle clients.
		 */
		bool all_done = false;
		for (i = 0; nevents > 0 && !all_done && i < state->socket_count; i++) {
			int j;
			if (state->sockets[i].type != SOCK_TYPE_CLIENT)
				continue;

			struct net_client *client = state->sockets[i].data.client;

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
				state->net_threads_busy++;
			}
			g_mutex_unlock (state->mutex);
		}

		g_mutex_lock (state->mutex);
		struct work_list_item *item = state->work_list;
		g_mutex_unlock (state->mutex);
		if (item == NULL && state->net_threads_busy == 0)
			break;
	}

	fprintf (stderr, "* INFO: Network thread terminating, disconnecting all clients.\n");

	/*
	 * Close the writing end of the pipe first to avoid SIGPIPE under
	 * any circumstances.
	 */
	g_mutex_lock (state->mutex);
	if (state->term_pipe_w >= 0) {
		close (state->term_pipe_w);
		state->term_pipe_w = -1;
	}
	g_mutex_unlock (state->mutex);

	unsigned i = 0;
	while (i < state->socket_count)
		switch (state->sockets[i].type) {
			case SOCK_TYPE_LISTENER:
			case SOCK_TYPE_TERM:
				close (state->sockets[i].fd);
				i++;
				break;

			case SOCK_TYPE_CLIENT:
				disconnect_client (state, i);
				break;
		}

	return NULL;
}


static int
add_socket (struct anim_state *state, socket_type_t type, int fd)
{
	int idx = state->socket_count++;
	state->pollfds = realloc (state->pollfds, state->socket_count * sizeof (*state->pollfds));
	state->sockets = realloc (state->sockets, state->socket_count * sizeof (*state->sockets));
	state->pollfds[idx].fd = fd;
	state->pollfds[idx].events = POLLIN;
	state->sockets[idx].type = type;
	state->sockets[idx].fd = fd;
	return idx;
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
		fprintf (stderr, "* ERROR: fcntl (enable O_NONBLOCK): %s\n", strerror (errno));
		close (sock);
		return -1;
	}

	static const int one = 1;

	if (setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof (one)) < 0)
		fprintf (stderr, "* WARNING: setsockopt (enable SO_REUSEADDR): %s\n", strerror (errno));

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
	int fd = accept (state->pollfds[i].fd, (struct sockaddr *) &client->addr, &client->addrlen);
	if (fd < 0) {
		fprintf (stderr, "* ERROR: accept(): %s\n", strerror (errno));
		free (client);
		return;
	}

	/*
	 * If we have an IPv6-mapped IPv4 address, convert it into a real IPv4
	 * address. This gives a more aesthetic look to the result of
	 * getnameinfo() on Linux (it will represent v6-mapped addresses in
	 * the "::ffff:1.2.3.4" format otherwise).
	 */
	static const unsigned char v6mapped_prefix[12] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff};
	if (client->addr.ss_family == AF_INET6) {
		struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *) &client->addr;
		if (memcmp (sin6->sin6_addr.s6_addr, v6mapped_prefix, 12) == 0) {
			struct sockaddr_in *sin = (struct sockaddr_in *) &client->addr;
			uint16_t port = sin6->sin6_port;
			memmove (&sin->sin_addr.s_addr, sin6->sin6_addr.s6_addr + 12, 4);
			sin->sin_family = AF_INET;
			sin->sin_port = port;
		}
	}

	int niflags = NI_NOFQDN | NI_NUMERICSERV;
	if (no_dns)
		niflags |= NI_NUMERICHOST;
	int r = getnameinfo ((struct sockaddr *) &client->addr, client->addrlen, client->name, sizeof (client->name), NULL, 0, niflags);
	if (r != 0) {
		fprintf (stderr, "* WARNING: getnameinfo() failed: %s\n", gai_strerror (r));
		my_safe_strcpy (client->name, "???", sizeof (client->name));
	}
	fprintf (stderr, "* INFO: New connection from client %s.\n", client->name);

	if (fcntl (fd, F_SETFL, O_NONBLOCK) < 0) {
		fprintf (stderr, "* ERROR: fcntl (enable O_NONBLOCK): %s\n", strerror (errno));
		close (fd);
		free (client);
		return;
	}

	static const int one = 1;
	if (setsockopt (fd, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof (one)) < 0)
		fprintf (stderr, "* WARNING: setsockopt (enable SO_KEEPALIVE): %s\n", strerror (errno));

	client->f = fdopen (fd, "r+");
	if (client->f == NULL) {
		fprintf (stderr, "* ERROR: fdopen(): %s\n", strerror (errno));
		close (fd);
		free (client);
	}

	/*
	 * XXX Without buffering, I/O is _badly_ inefficient.
	 * But with buffering, some race conditions may exist as we are
	 * using non-blocking I/O. This needs further investigation.
	 */
	if (setvbuf (client->f, NULL, _IONBF, 0) != 0)
		fprintf (stderr, "* WARNING: setvbuf(): %s\n", strerror (errno));

	int idx = add_socket (state, SOCK_TYPE_CLIENT, fd);
	state->sockets[idx].data.client = client;
}


static bool
send_render_command (struct anim_state *state, unsigned client_id, unsigned thread_id, unsigned frame_no, const struct mandeldata *md)
{
	/*
	 * XXX This doesn't look exactly like an exercise in efficiency...
	 * The io_buffer code should get a bit smarter, which would probably
	 * make most of the byte counting performed here superfluous.
	 */
	struct net_client *client = state->sockets[client_id].data.client;
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
process_net_input (struct anim_state *state, unsigned i)
{
	struct net_client *client = state->sockets[i].data.client;

	if (fgets (client->input_buf + client->input_pos, sizeof (client->input_buf) - client->input_pos, client->f) == NULL) {
		if (feof (client->f))
			fprintf (stderr, "* WARNING: EOF from client %s.\n", client->name);
		else
			fprintf (stderr, "* WARNING: Error reading from client %s: %s\n", client->name, strerror (errno));
		return false;
	}
	client->input_pos += strlen (client->input_buf + client->input_pos);
	if (client->input_pos >= sizeof (client->input_buf) - 1 && client->input_buf[client->input_pos - 1] != '\n') {
		fprintf (stderr, "* WARNING: Buffer overrun on network input from client %s. Nasty, nasty client. Dropping connection.\n", client->name);
		return false;
	}
	if (client->input_pos >= 1 && client->input_buf[client->input_pos - 1] == '\n') {
		/* Complete line read, parse it. */
		client->input_pos = 0;
		char *saveptr;
		const char *keyword = strtok_r (client->input_buf, NETWORK_DELIM, &saveptr);
		if (keyword == NULL) {
			fprintf (stderr, "* WARNING: Empty message from client %s.\n", client->name);
			return false;
		}

		if (strcmp (keyword, "MOIN") == 0) {
			if (client->state != CSTATE_INITIAL) {
				fprintf (stderr, "* WARNING: MOIN message while not in CSTATE_INITIAL from client %s.\n", client->name);
				return false;
			}
			const char *arg1 = strtok_r (NULL, NETWORK_DELIM, &saveptr);
			if (arg1 == NULL) {
				fprintf (stderr, "* WARNING: Invalid MOIN message from client %s.\n", client->name);
				return false;
			}
			int j = atoi (arg1);
			if (j < 1) {
				fprintf (stderr, "* WARNING: Invalid thread count in MOIN message from client %s.\n", client->name);
				return false;
			}
			state->net_threads_total += j;
			fprintf (stderr, "* INFO: Client %s ready to rumble (%d threads). Total capacity now %u threads.\n", client->name, j, (unsigned) zoom_threads + state->net_threads_total);
			client->thread_count = j;
			client->state = CSTATE_WORKING;
			client->work_items = malloc (client->thread_count * sizeof (*client->work_items));
			memset (client->work_items, 0, client->thread_count * sizeof (*client->work_items));
		} else if (client->state == CSTATE_INITIAL) {
			fprintf (stderr, "* WARNING: Non-MOIN message in CSTATE_INITIAL, from client %s.\n", client->name);
			return false;
		} else if (strcmp (keyword, "DONE") == 0) {
			const char *arg1 = strtok_r (NULL, NETWORK_DELIM, &saveptr);
			if (arg1 == NULL) {
				fprintf (stderr, "* WARNING: Invalid DONE message from client %s.\n", client->name);
				return false;
			}
			int j = atoi (arg1);
			if (j < 0 || j >= client->thread_count) {
				fprintf (stderr, "* WARNING: Invalid thread id in DONE message from client %s.\n", client->name);
				return false;
			}
			fprintf (stderr, "Frame %d done, on %s.\n", client->work_items[j]->i, client->name);
			/* XXX save the information that this client successfully rendered frame i */
			if (state->client_index != NULL) {
				g_mutex_lock (state->mutex);
				state->client_index[client->work_items[j]->i] = strdup (client->name);
				g_mutex_unlock (state->mutex);
			}
			free_work_list_item (client->work_items[j]);
			client->work_items[j] = NULL;
			state->net_threads_busy--;
		} else {
			fprintf (stderr, "* WARNING: Invalid message from client %s.\n", client->name);
			return false;
		}
	}
	return true;
}


static void
disconnect_client (struct anim_state *state, unsigned i)
{
	struct net_client *client = state->sockets[i].data.client;

	state->net_threads_total -= client->thread_count;

	/* No error checks here, there's nothing we could do anyway. */
	fputs ("TERMINATE\r\n", client->f);
	fclose (client->f);
	close (state->sockets[i].fd);

	g_mutex_lock (state->mutex);
	unsigned j;
	for (j = 0; j < client->thread_count; j++) {
		struct work_list_item *item = client->work_items[j];
		if (item == NULL)
			continue;
		fprintf (stderr, "* WARNING: Will have to re-render frame %d due to client failure.\n", item->i);
		item->next = state->work_list;
		state->work_list = item;
		state->net_threads_busy--;
	}
	g_mutex_unlock (state->mutex);

	free (client->work_items);
	free (client);

	state->socket_count--;
	if (i < state->socket_count) {
		memcpy (&state->pollfds[i], &state->pollfds[state->socket_count], sizeof (*state->pollfds));
		memcpy (&state->sockets[i], &state->sockets[state->socket_count], sizeof (*state->sockets));
	}
}
