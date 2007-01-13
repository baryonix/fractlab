#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <glib.h>

#include <gmp.h>
#include <mpfr.h>


#include "anim.h"
#include "util.h"
#include "file.h"
#include "defs.h"
#include "mandelbrot.h"




struct zoom_state {
	mpfr_t x0, xn, d0, dn, a, ln_a, b, c;
};

struct thread_state {
	struct zoom_state xstate, ystate;
};

static void init_zoom_state (struct zoom_state *state, mpf_t x0, mpf_t magf0, mpf_t xn, mpf_t magfn, unsigned long n);

static gchar *start_coords = NULL, *target_coords = NULL;
static double aspect;
static gint zpower = 2;
static gdouble log_factor = 0.0;
static gint maxiter = DEFAULT_MAXITER;


static GOptionEntry option_entries [] = {
	{"start-coords", 's', 0, G_OPTION_ARG_FILENAME, &start_coords, "Start coordinates", "FILE"},
	{"target-coords", 't', 0, G_OPTION_ARG_FILENAME, &target_coords, "Target coordinates", "FILE"},
	{"maxiter", 'i', 0, G_OPTION_ARG_INT, &maxiter, "Maximum # of iterations", "N"},
	{"z-power", 'Z', 0, G_OPTION_ARG_INT, &zpower, "Set power of Z in iteration to N", "N"},
	{"log-factor", 'l', 0, G_OPTION_ARG_DOUBLE, &log_factor, "Use logarithmic colors, color = LF * ln (iter)", "LF"},
	{NULL}
};


static void
parse_command_line (int *argc, char ***argv)
{
	GOptionContext *context = g_option_context_new (NULL);
	g_option_context_add_main_entries (context, option_entries, "mandel-zoom");
	g_option_context_add_group (context, anim_get_option_group ());
	g_option_context_parse (context, argc, argv, NULL);
}


/*
 * Note that this function initializes the state for the calculation
 * of frames 0 .. n, so we'll actually generate n + 1 frames.
 */
static void
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


static void
get_frame (const struct zoom_state *state, unsigned long i, mpfr_t x, mpfr_t d)
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


static void
render_frame (void *data, struct mandeldata *md, unsigned long i)
{
	const struct thread_state *state = (const struct thread_state *) data;
	mpfr_t cfr, dfr, tmp0;

	md->type = FRACTAL_MANDELBROT;
	md->zpower = zpower;
	md->maxiter = maxiter;
	md->log_factor = log_factor;

	mpfr_init (cfr);
	mpfr_init (dfr);
	mpfr_init (tmp0);

	get_frame (&state->xstate, i, cfr, dfr);
	mpfr_ui_div (dfr, 1, dfr, GMP_RNDN);
	mpfr_get_f (md->area.center.real, cfr, GMP_RNDN);
	mpfr_get_f (md->area.magf, dfr, GMP_RNDN);
	get_frame (&state->ystate, i, cfr, dfr);
	mpfr_get_f (md->area.center.imag, cfr, GMP_RNDN);

	mpfr_clear (cfr);
	mpfr_clear (dfr);
	mpfr_clear (tmp0);
}


int
main (int argc, char **argv)
{
	mpf_set_default_prec (1024); /* ! */
	mpfr_set_default_prec (1024); /* ! */
	struct thread_state state[1];
	mpf_t cx0, cy0, magf0, cxn, cyn, magfn;
	mpf_t mpaspect;
	mpf_init (cx0);
	mpf_init (cy0);
	mpf_init (magf0);
	mpf_init (cxn);
	mpf_init (cyn);
	mpf_init (magfn);
	mpf_init (mpaspect);
	parse_command_line (&argc, &argv);
	FILE *f = NULL;
	if (start_coords == NULL || !(f = fopen (start_coords, "r")) || !fread_coords_as_center (f, cx0, cy0, magf0)) {
		if (f != NULL)
			fclose (f);
		fprintf (stderr, "* Error: No start coordinates specified.\n");
		return 1;
	}
	fclose (f);
	f = NULL;
	if (target_coords == NULL || !(f = fopen (target_coords, "r")) || !fread_coords_as_center (f, cxn, cyn, magfn)) {
		if (f != NULL)
			fclose (f);
		fprintf (stderr, "* Error: No target coordinates specified.\n");
		return 1;
	}
	fclose (f);

	aspect = (double) img_width / img_height;
	mpf_set_d (mpaspect, aspect);
	if (aspect > 1.0) {
		init_zoom_state (&state->ystate, cy0, magf0, cyn, magfn, frame_count - 1);
		mpf_div (magf0, magf0, mpaspect);
		mpf_div (magfn, magfn, mpaspect);
		init_zoom_state (&state->xstate, cx0, magf0, cxn, magfn, frame_count - 1);
	} else {
		init_zoom_state (&state->xstate, cx0, magf0, cxn, magfn, frame_count - 1);
		mpf_mul (magf0, magf0, mpaspect);
		mpf_mul (magfn, magfn, mpaspect);
		init_zoom_state (&state->ystate, cy0, magf0, cyn, magfn, frame_count - 1);
	}

	anim_render (render_frame, state);

	return 0;
}
