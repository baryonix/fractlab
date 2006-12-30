#include <stdlib.h>
#include <math.h>
#include <png.h>

#include "anim.h"
#include "util.h"
#include "file.h"
#include "defs.h"
#include "mandelbrot.h"


struct color {
	unsigned char r, g, b;
};


struct anim_state {
	GMutex *mutex;
	unsigned long i;
	frame_func_t frame_func;
	void *data;
};


static void write_png (const struct anim_state *state, const struct mandeldata *md, const char *filename);
static gpointer thread_func (gpointer data);
static void render_frame (struct anim_state *state, unsigned long i);


struct color colors[COLORS];

const int thread_count = 1;

gint maxiter = DEFAULT_MAXITER, frame_count = 0;
static gdouble log_factor = 0.0;
gint img_width = 200, img_height = 200;
static gint zoom_threads = 1;
static gint compression = -1;
static gint start_frame = 0;



static GOptionEntry option_entries[] = {
	{"maxiter", 'i', 0, G_OPTION_ARG_INT, &maxiter, "Maximum # of iterations", "N"},
	{"frames", 'n', 0, G_OPTION_ARG_INT, &frame_count, "# of frames in animation", "N"},
	{"start-frame", 'S', 0, G_OPTION_ARG_INT, &start_frame, "Start rendering at frame N", "N"},
	{"log-factor", 'l', 0, G_OPTION_ARG_DOUBLE, &log_factor, "Use logarithmic colors, color = LF * ln (iter)", "LF"},
	{"width", 'W', 0, G_OPTION_ARG_INT, &img_width, "Image width", "PIXELS"},
	{"height", 'H', 0, G_OPTION_ARG_INT, &img_height, "Image height", "PIXELS"},
	{"threads", 'T', 0, G_OPTION_ARG_INT, &zoom_threads, "Parallel rendering with N threads", "N"},
	{"compression", 'C', 0, G_OPTION_ARG_INT, &compression, "Compression level for PNG output (0..9)", "LEVEL"},
	{NULL}
};


GOptionGroup *
anim_get_option_group (void)
{
	GOptionGroup *group = g_option_group_new ("anim", "Animation Options", "Animation Options", NULL, NULL);
	g_option_group_add_entries (group, option_entries);
	return group;
}


void anim_render
(frame_func_t frame_func, void *data)
{
	struct anim_state state[1];
	state->frame_func = frame_func;
	state->data = data;
	state->i = start_frame;

	int i;
	for (i = 0; i < COLORS; i++) {
		colors[i].r = (unsigned short) (sin (2 * M_PI * i / COLORS) * 127) + 128;
		colors[i].g = (unsigned short) (sin (4 * M_PI * i / COLORS) * 127) + 128;
		colors[i].b = (unsigned short) (sin (6 * M_PI * i / COLORS) * 127) + 128;
	}

	if (zoom_threads > 1) {
		g_thread_init (NULL);
		GThread *threads[zoom_threads];
		state->mutex = g_mutex_new ();
		state->i = start_frame;
		for (i = 0; i < zoom_threads; i++)
			threads[i] = g_thread_create (thread_func, state, TRUE, NULL);
		for (i = 0; i < zoom_threads; i++)
			g_thread_join (threads[i]);
		g_mutex_free (state->mutex);
	} else
		for (i = start_frame; i < frame_count; i++)
			render_frame (state, i);
}


static void
write_png (const struct anim_state *state, const struct mandeldata *md, const char *filename)
{
	FILE *f = fopen (filename, "wb");

	png_structp png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	png_infop info_ptr = png_create_info_struct (png_ptr);

	if (setjmp (png_jmpbuf (png_ptr)))
		fprintf (stderr, "PNG longjmp!\n");

	//png_set_swap (png_ptr); /* FIXME this should only be done on little-endian systems */
	png_init_io (png_ptr, f);
	if (compression == 0)
		png_set_filter (png_ptr, 0, PNG_FILTER_NONE);
	if (compression >= 0)
		png_set_compression_level (png_ptr, compression);
	png_set_IHDR (png_ptr, info_ptr, md->w, md->h, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_write_info (png_ptr, info_ptr);

	unsigned char row[md->w * 3];
	png_bytep row_ptr[] = {(png_bytep) row};

	unsigned int x, y;
	//int opct = -1;

	for (y = 0; y < md->h; y++) {
		for (x = 0; x < md->w; x++) {
			unsigned int i = mandel_get_pixel (md, x, y);

			i %= 256;

			row[3 * x + 0] = colors[i].r;
			row[3 * x + 1] = colors[i].g;
			row[3 * x + 2] = colors[i].b;
		}
		png_write_rows (png_ptr, row_ptr, 1);
	}

	png_write_end (png_ptr, info_ptr);
	png_destroy_write_struct (&png_ptr, &info_ptr);

	fclose (f);
}


static gpointer
thread_func (gpointer data)
{
	struct anim_state *state = (struct anim_state *) data;
	while (TRUE) {
		unsigned long i;
		g_mutex_lock (state->mutex);
		i = state->i++;
		g_mutex_unlock (state->mutex);
		if (i >= frame_count)
			break;

		render_frame (state, i);
	}
	return NULL;
}


static void
render_frame (struct anim_state *state, unsigned long i)
{
	struct mandeldata md[1];
	memset (md, 0, sizeof (*md));
	md->data = malloc (img_width * img_height * sizeof (unsigned));
	md->w = img_width;
	md->h = img_height;
	md->maxiter = maxiter;
	md->render_method = RM_MARIANI_SILVER;
	md->log_factor = log_factor;

	state->frame_func (state->data, md, i);

	mandel_init_coords (md);
	mandel_render (md);

	char name[64];
	sprintf (name, "file%06lu.png", i);
	write_png (state, md, name);
	free (md->data);

	fprintf (stderr, "* Frame %ld done", i);
	unsigned bits = get_precision (md);
	if (bits == 0)
		fprintf (stderr, ", using FP arithmetic");
	else
		fprintf (stderr, ", using MP arithmetic (%d bits precision)", bits);
	fprintf (stderr, ".\n");

	mpf_clear (md->xmin_f);
	mpf_clear (md->xmax_f);
	mpf_clear (md->ymin_f);
	mpf_clear (md->ymax_f);
}
