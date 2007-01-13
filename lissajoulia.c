#include <stdio.h>
#include <math.h>

#include <glib.h>

#include "anim.h"
#include "file.h"

/* preal = A * sin (a * t + delta); pimag = B * sin (b * t) */


struct lj_state {
	gdouble A, B, delta;
	gint a, b;
	mpf_t cx, cy, magf;
};


static void frame_func (void *data, struct mandeldata *md, unsigned long i);


static void
frame_func (void *data, struct mandeldata *md, unsigned long i)
{
	struct lj_state *state = (struct lj_state *) data;
	md->type = FRACTAL_JULIA;
	mpf_init_set (md->area.center.real, state->cx);
	mpf_init_set (md->area.center.imag, state->cy);
	mpf_init_set (md->area.magf, state->magf);

	double preal = state->A * sin (2 * M_PI * state->a * i / frame_count + state->delta);
	double pimag = state->B * sin (2 * M_PI * state->b * i / frame_count);

	mpf_init (md->preal_f);
	mpf_set_d (md->preal_f, preal);
	mpf_init (md->pimag_f);
	mpf_set_d (md->pimag_f, pimag);
}


int
main (int argc, char *argv[])
{
	mpf_set_default_prec (1024); /* ! */

	struct lj_state state[1];

	state->A = 1.0;
	state->B = 1.0;
	state->a = 0;
	state->b = 0;
	state->delta = 0.5;

	const gchar *coord_file = NULL;

	GOptionEntry option_entries[] = {
		{"A", 'A', 0, G_OPTION_ARG_DOUBLE, &state->A, "A"},
		{"B", 'B', 0, G_OPTION_ARG_DOUBLE, &state->B, "B"},
		{"a", 'a', 0, G_OPTION_ARG_INT, &state->a, "a"},
		{"b", 'b', 0, G_OPTION_ARG_INT, &state->b, "b"},
		{"delta", 'd', 0, G_OPTION_ARG_DOUBLE, &state->delta, "delta"},
		{"coords", 'c', 0, G_OPTION_ARG_FILENAME, &coord_file, "Coordinates"},
		{NULL}
	};

	GOptionContext *context = g_option_context_new (NULL);
	g_option_context_add_main_entries (context, option_entries, "lissajoulia");
	g_option_context_add_group (context, anim_get_option_group ());
	g_option_context_parse (context, &argc, &argv, NULL);

	state->delta *= M_PI;
	mpf_init (state->cx);
	mpf_init (state->cy);
	mpf_init (state->magf);

	FILE *f;
	if (coord_file == NULL || !(f = fopen (coord_file, "r")) || !fread_coords_as_center (f, state->cx, state->cy, state->magf)) {
		fprintf (stderr, "* Error: No coordinates specified, or error reading the file.\n");
		return 1;
	}
	fclose (f);

	anim_render (frame_func, state);

	return 0;
}
