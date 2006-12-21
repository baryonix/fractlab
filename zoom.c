#include <stdio.h>

#include <glib.h>


#include "file.h"


struct zoom_state {
	mpf_t c0, cn, d0, dn;
};


static gchar *start_coords = NULL, *target_coords = NULL;

static GOptionEntry option_entries [] = {
	{"start-coords", 's', 0, G_OPTION_ARG_FILENAME, &start_coords, "Start coordinates", "FILE"},
	{"target-coords", 't', 0, G_OPTION_ARG_FILENAME, &target_coords, "Target coordinates", "FILE"},
	{NULL}
};


void
parse_command_line (int *argc, char ***argv)
{
	GOptionContext *context = g_option_context_new (NULL);
	g_option_context_add_main_entries (context, option_entries, "mandel-zoom");
	g_option_context_parse (context, argc, argv, NULL);
}


int
main (int argc, char **argv)
{
	mpf_set_default_prec (1024); /* ! */
	mpf_t cx0, cy0, magf0, cx1, cy1, magf1;
	mpf_init (xmin0);
	mpf_init (xmax0);
	mpf_init (ymin0);
	mpf_init (ymax0);
	mpf_init (xminn);
	mpf_init (xmaxn);
	mpf_init (yminn);
	mpf_init (ymaxn);
	parse_command_line (&argc, &argv);
	if (start_coords == NULL || !read_corner_coords_from_file (start_coords, xmin0, xmax0, ymin0, ymax0)) {
		fprintf (stderr, "* Error: No start coordinates specified.\n");
		return 1;
	}
	if (target_coords == NULL || !read_corner_coords_from_file (target_coords, xminn, xmaxn, yminn, ymaxn)) {
		fprintf (stderr, "* Error: No target coordinates specified.\n");
		return 1;
	}
	return 0;
}
