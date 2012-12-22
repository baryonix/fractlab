#include <stdio.h>
#include <stdbool.h>
#include <math.h>

#include <gmp.h>
#include <mpfr.h>

#include <glib.h>

#include "defs.h"
#include "fractal-render.h"
#include "file.h"
#include "render-png.h"


static gint img_width = 200, img_height = 200;
static gint thread_count = 1;
static gint compression = 9;
static gchar *output_file = NULL;
static gint aa_level = 1;

static GOptionEntry option_entries[] = {
	{"width", 'W', 0, G_OPTION_ARG_INT, &img_width, "Image width", "PIXELS"},
	{"height", 'H', 0, G_OPTION_ARG_INT, &img_height, "Image height", "PIXELS"},
	{"threads", 'T', 0, G_OPTION_ARG_INT, &thread_count, "Parallel rendering with N threads", "N"},
	{"compression", 'C', 0, G_OPTION_ARG_INT, &compression, "Compression level for PNG output (0..9)", "LEVEL"},
	{"output-file", 'o', 0, G_OPTION_ARG_FILENAME, &output_file, "Output file", "NAME"},
	{"anti-alias", 'a', 0, G_OPTION_ARG_INT, &aa_level, "Anti-aliasing level", "LEVEL"},
	{NULL}
};


static bool
parse_command_line (int *argc, char ***argv)
{
	GError *err = NULL;
	GOptionContext *context = g_option_context_new (NULL);
	g_option_context_add_main_entries (context, option_entries, "fractlab-image");
	if (!g_option_context_parse (context, argc, argv, &err)) {
		fprintf (stderr, "* ERROR: %s\n", err->message);
		return false;
	}
	return true;
}


int
main (int argc, char **argv)
{
	g_thread_init (NULL);

	mpf_set_default_prec (1024); /* ! */
	mpfr_set_default_prec (1024); /* ! */

	if (!parse_command_line (&argc, &argv))
		return 1;

	if (output_file == NULL) {
		fprintf (stderr, "* ERROR: No output file specified.\n");
		return 1;
	}

	if (argc != 2) {
		fprintf (stderr, "* ERROR: No coordinate file specified.\n");
		return 1;
	}

	struct mandeldata md;
	char errbuf[256];
	if (!read_mandeldata (argv[1], &md, errbuf, sizeof (errbuf))) {
		fprintf (stderr, "%s: cannot read: %s\n", argv[1], errbuf);
	}

	render_to_png (&md, output_file, compression, NULL, img_width, img_height, thread_count, aa_level);

	return 0;
}
