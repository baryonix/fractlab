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
#include "fractal-render.h"




struct zoom_state {
	mpfr_t x0, xn, d0, dn, a, ln_a, b, c;
};

struct thread_state {
	struct mandeldata md;
	struct zoom_state xstate, ystate;
};

static void init_zoom_state (struct zoom_state *state, mpf_srcptr x0, mpf_srcptr magf0, mpf_srcptr xn, mpf_srcptr magfn, unsigned long n);

static gchar *start_coords = NULL, *target_coords = NULL;
static double aspect;


static GOptionEntry option_entries [] = {
	{"start-coords", 's', 0, G_OPTION_ARG_FILENAME, &start_coords, "Start coordinates", "FILE"},
	{"target-coords", 't', 0, G_OPTION_ARG_FILENAME, &target_coords, "Target coordinates", "FILE"},
	{NULL}
};


static bool
parse_command_line (int *argc, char ***argv)
{
	GError *err = NULL;
	GOptionContext *context = g_option_context_new (NULL);
	g_option_context_add_main_entries (context, option_entries, "mandel-zoom");
	g_option_context_add_group (context, anim_get_option_group ());
	if (!g_option_context_parse (context, argc, argv, &err)) {
		fprintf (stderr, "* ERROR: %s\n", err->message);
		return false;
	}
	return true;
}


/*
 * Note that this function initializes the state for the calculation
 * of frames 0 .. n, so we'll actually generate n + 1 frames.
 */
static void
init_zoom_state (struct zoom_state *state, mpf_srcptr x0, mpf_srcptr magf0, mpf_srcptr xn, mpf_srcptr magfn, unsigned long n)
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

	mandeldata_clone (md, &state->md);

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
	char errbuf[1024];
	mpf_set_default_prec (1024); /* ! */
	mpfr_set_default_prec (1024); /* ! */
	struct thread_state state[1];
	mpf_t mpaspect;
	mpf_init (mpaspect);
	if (!parse_command_line (&argc, &argv))
		return 2;
	struct mandeldata md0[1], *const mdn = &state->md;

	if (start_coords == NULL) {
		fprintf (stderr, "* Error: No start coordinates specified.\n");
		return 1;
	}
	
	if (!read_mandeldata (start_coords, md0, errbuf, sizeof (errbuf))) {
		fprintf (stderr, "%s: cannot read: %s\n", start_coords, errbuf);
		return 1;
	}
	
	if (target_coords == NULL) {
		fprintf (stderr, "* Error: No target coordinates specified.\n");
		return 1;
	}

	if (!read_mandeldata (target_coords, mdn, errbuf, sizeof (errbuf))) {
		fprintf (stderr, "%s: cannot read: %s\n", target_coords, errbuf);
		return 1;
	}

	aspect = (double) img_width / img_height;
	mpf_set_d (mpaspect, aspect);
	if (aspect > 1.0) {
		//init_zoom_state (&state->ystate, cy0, magf0, cyn, magfn, frame_count - 1);
		init_zoom_state (&state->ystate, md0->area.center.imag, md0->area.magf, mdn->area.center.imag, mdn->area.magf, frame_count - 1);
		mpf_div (md0->area.magf, md0->area.magf, mpaspect);
		mpf_div (mdn->area.magf, mdn->area.magf, mpaspect);
		//init_zoom_state (&state->xstate, cx0, magf0, cxn, magfn, frame_count - 1);
		init_zoom_state (&state->xstate, md0->area.center.real, md0->area.magf, mdn->area.center.real, mdn->area.magf, frame_count - 1);
	} else {
		//init_zoom_state (&state->xstate, cx0, magf0, cxn, magfn, frame_count - 1);
		init_zoom_state (&state->xstate, md0->area.center.real, md0->area.magf, mdn->area.center.real, mdn->area.magf, frame_count - 1);
		mpf_mul (md0->area.magf, md0->area.magf, mpaspect);
		mpf_mul (mdn->area.magf, mdn->area.magf, mpaspect);
		//init_zoom_state (&state->ystate, cy0, magf0, cyn, magfn, frame_count - 1);
		init_zoom_state (&state->ystate, md0->area.center.imag, md0->area.magf, mdn->area.center.imag, mdn->area.magf, frame_count - 1);
	}

	mandeldata_clear (md0);

	anim_render (render_frame, state);

	mandeldata_clear (&state->md);

	return 0;
}
