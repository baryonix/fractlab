#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <glib.h>

#include <gmp.h>
#include <mpfr.h>

#include <png.h>


#include "util.h"
#include "file.h"
#include "defs.h"
#include "mandelbrot.h"


#define DEFAULT_MAXITER 1000


struct zoom_state {
	mpfr_t x0, xn, d0, dn, a, ln_a, b, c;
};

const int thread_count = 1;

struct color {
	unsigned char r, g, b;
};

struct color colors[COLORS];


static gchar *start_coords = NULL, *target_coords = NULL;
static gint maxiter = DEFAULT_MAXITER, frame_count = 0;
static gdouble log_factor = 0.0;
static gint img_width = 200, img_height = 200;

static double aspect;

static GOptionEntry option_entries [] = {
	{"start-coords", 's', 0, G_OPTION_ARG_FILENAME, &start_coords, "Start coordinates", "FILE"},
	{"target-coords", 't', 0, G_OPTION_ARG_FILENAME, &target_coords, "Target coordinates", "FILE"},
	{"maxiter", 'i', 0, G_OPTION_ARG_INT, &maxiter, "Maximum # of iterations"},
	{"frames", 'n', 0, G_OPTION_ARG_INT, &frame_count, "# of frames in animation"},
	{"log-factor", 'l', 0, G_OPTION_ARG_DOUBLE, &log_factor, "Use logarithmic colors, color = LF * ln (iter)", "LF"},
	{"width", 'W', 0, G_OPTION_ARG_INT, &img_width, "Image width", "PIXELS"},
	{"height", 'H', 0, G_OPTION_ARG_INT, &img_height, "Image height", "PIXELS"},
	{NULL}
};


void write_png (const struct mandeldata *md, const char *filename);


void
parse_command_line (int *argc, char ***argv)
{
	GOptionContext *context = g_option_context_new (NULL);
	g_option_context_add_main_entries (context, option_entries, "mandel-zoom");
	g_option_context_parse (context, argc, argv, NULL);
}


/*
 * Note that this function initializes the state for the calculation
 * of frames 0 .. n, so we'll actually generate n + 1 frames.
 */
void
init_zoom_state (struct zoom_state *state, mpf_t x0, mpf_t magf0, mpf_t xn, mpf_t magfn, unsigned long n)
{
	mpfr_t a_n; /* a^n */

	mpfr_init (state->x0);
	mpfr_init (state->xn);
	mpfr_init (state->d0);
	mpfr_init (state->dn);
	mpfr_init (state->a);
	mpfr_init (state->ln_a);
	mpfr_init (state->b);
	mpfr_init (state->c);
	mpfr_init (a_n);

	mpfr_set_f (state->x0, x0, GMP_RNDN);
	mpfr_set_f (state->xn, xn, GMP_RNDN);

	/* d0 = 1 / magf0 */
	mpfr_set_f (state->d0, magf0, GMP_RNDN);
	mpfr_ui_div (state->d0, 1, state->d0, GMP_RNDN);
	/* dn = 1 / magfn */
	mpfr_set_f (state->dn, magfn, GMP_RNDN);
	mpfr_ui_div (state->dn, 1, state->dn, GMP_RNDN);

	/* a_n = dn / d0 */
	mpfr_div (a_n, state->dn, state->d0, GMP_RNDN);

	/* ln_a = ln (a_n) / n */
	mpfr_log (state->ln_a, a_n, GMP_RNDN);
	mpfr_div_ui (state->ln_a, state->ln_a, n, GMP_RNDN);

	/* b = (x0 * a_n - xn) / (a_n - 1) */
	mpfr_mul (state->b, state->x0, a_n, GMP_RNDN);
	mpfr_sub (state->b, state->b, state->xn, GMP_RNDN);
	mpfr_sub_ui (a_n, a_n, 1, GMP_RNDN);
	mpfr_div (state->b, state->b, a_n, GMP_RNDN);

	/* c = x0 - b */
	mpfr_sub (state->c, state->x0, state->b, GMP_RNDN);

	mpfr_clear (a_n);
}


void
get_frame (struct zoom_state *state, unsigned long i, mpfr_t x, mpfr_t d)
{
	mpfr_t a_i;

	mpfr_init (a_i);

	/* a_i = exp (ln_a * i) */
	mpfr_mul_ui (a_i, state->ln_a, i, GMP_RNDN);
	mpfr_exp (a_i, a_i, GMP_RNDN);

	/* d = d0 * a_i */
	mpfr_mul (d, state->d0, a_i, GMP_RNDN);

	/* x = c * a_i + b */
	mpfr_mul (x, state->c, a_i, GMP_RNDN);
	mpfr_add (x, x, state->b, GMP_RNDN);

	mpfr_clear (a_i);
}


void
render_frame (struct zoom_state *xstate, struct zoom_state *ystate, unsigned long i)
{
	mpfr_t cfr, dfr, tmp0;
	struct mandeldata md[1];

	memset (md, 0, sizeof (*md));

	mpf_init (md->xmin_f);
	mpf_init (md->xmax_f);
	mpf_init (md->ymin_f);
	mpf_init (md->ymax_f);
	mpfr_init (cfr);
	mpfr_init (dfr);
	mpfr_init (tmp0);

	get_frame (xstate, i, cfr, dfr);
	mpfr_sub (tmp0, cfr, dfr, GMP_RNDN);
	mpfr_get_f (md->xmin_f, tmp0, GMP_RNDN);
	mpfr_add (tmp0, cfr, dfr, GMP_RNDN);
	mpfr_get_f (md->xmax_f, tmp0, GMP_RNDN);
	get_frame (ystate, i, cfr, dfr);
	mpfr_sub (tmp0, cfr, dfr, GMP_RNDN);
	mpfr_get_f (md->ymin_f, tmp0, GMP_RNDN);
	mpfr_add (tmp0, cfr, dfr, GMP_RNDN);
	mpfr_get_f (md->ymax_f, tmp0, GMP_RNDN);

	md->data = malloc (img_width * img_height * sizeof (unsigned));
	md->w = img_width;
	md->h = img_height;
	md->maxiter = maxiter;
	md->render_method = RM_MARIANI_SILVER;
	md->log_factor = log_factor;

	/*char xminc[1024], xmaxc[1024], yminc[1024], ymaxc[1024];
	coords_to_string (xmin, xmax, ymin, ymax, xminc, xmaxc, yminc, ymaxc, 1024);
	printf ("xmin=%s xmax=%s ymin=%s ymax=%s\n", xminc, xmaxc, yminc, ymaxc);*/

	fprintf (stderr, "Starting to render...\n");
	mandel_render (md);
	fprintf (stderr, "Rendering finished...\n");

	mpf_clear (md->xmin_f);
	mpf_clear (md->xmax_f);
	mpf_clear (md->ymin_f);
	mpf_clear (md->ymax_f);
	mpfr_clear (cfr);
	mpfr_clear (dfr);
	mpfr_clear (tmp0);

	char name[64];
	sprintf (name, "file%06lu.png", i);
	write_png (md, name);
	free (md->data);
}


void
write_png (const struct mandeldata *md, const char *filename)
{
	FILE *f = fopen (filename, "wb");

	png_structp png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	png_infop info_ptr = png_create_info_struct (png_ptr);

	if (setjmp (png_jmpbuf (png_ptr)))
		fprintf (stderr, "PNG longjmp!\n");

	//png_set_swap (png_ptr); /* FIXME this should only be done on little-endian systems */
	png_init_io (png_ptr, f);
	png_set_filter (png_ptr, 0, PNG_FILTER_NONE);
	png_set_compression_level (png_ptr, 0);
	png_set_IHDR (png_ptr, info_ptr, md->w, md->h, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_write_info (png_ptr, info_ptr);
	//png_set_flush (png_ptr, 16);

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
		/*int pct = rint ((y + 1) * 100.0 / imgheight);
		if (pct != opct) {
			printf ("[%3d%%]\r", pct);
			fflush (stdout);
			opct = pct;
		}*/
	}

	png_write_end (png_ptr, info_ptr);
	png_destroy_write_struct (&png_ptr, &info_ptr);

	fclose (f);
}


int
main (int argc, char **argv)
{
	mpf_set_default_prec (1024); /* ! */
	mpfr_set_default_prec (1024); /* ! */
	struct zoom_state xstate, ystate;
	mpf_t cx0, cy0, magf0, cxn, cyn, magfn;
	mpf_t mpaspect;
	int i;
	mpf_init (cx0);
	mpf_init (cy0);
	mpf_init (magf0);
	mpf_init (cxn);
	mpf_init (cyn);
	mpf_init (magfn);
	mpf_init (mpaspect);
	parse_command_line (&argc, &argv);
	if (start_coords == NULL || !read_center_coords_from_file (start_coords, cx0, cy0, magf0)) {
		fprintf (stderr, "* Error: No start coordinates specified.\n");
		return 1;
	}
	if (target_coords == NULL || !read_center_coords_from_file (target_coords, cxn, cyn, magfn)) {
		fprintf (stderr, "* Error: No target coordinates specified.\n");
		return 1;
	}

	for (i = 0; i < COLORS; i++) {
		colors[i].r = (unsigned short) (sin (2 * M_PI * i / COLORS) * 127) + 128;
		colors[i].g = (unsigned short) (sin (4 * M_PI * i / COLORS) * 127) + 128;
		colors[i].b = (unsigned short) (sin (6 * M_PI * i / COLORS) * 127) + 128;
	}

	aspect = (double) img_width / img_height;
	mpf_set_d (mpaspect, aspect);
	if (aspect > 1.0) {
		init_zoom_state (&ystate, cy0, magf0, cyn, magfn, frame_count - 1);
		mpf_div (magf0, magf0, mpaspect);
		mpf_div (magfn, magfn, mpaspect);
		init_zoom_state (&xstate, cx0, magf0, cxn, magfn, frame_count - 1);
	} else {
		init_zoom_state (&xstate, cx0, magf0, cxn, magfn, frame_count - 1);
		mpf_mul (magf0, magf0, mpaspect);
		mpf_mul (magfn, magfn, mpaspect);
		init_zoom_state (&ystate, cy0, magf0, cyn, magfn, frame_count - 1);
	}

	for (i = 0; i < frame_count; i++)
		render_frame (&xstate, &ystate, i);

	return 0;
}
