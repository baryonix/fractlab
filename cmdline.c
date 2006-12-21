#include <stdlib.h>
#include <stdbool.h>

#include <glib.h>
#include <gtk/gtk.h>

#include "cmdline.h"


gchar *option_center_coords, *option_corner_coords;
int thread_count = 1;


static GOptionEntry option_entries[] = {
	{"center-coords", 'c', 0, G_OPTION_ARG_FILENAME, &option_center_coords, "Read center/magf coordinates from FILE", "FILE"},
	{"corner-coords", 'C', 0, G_OPTION_ARG_FILENAME, &option_corner_coords, "Read corner coordinates from FILE", "FILE"},
	{"threads", 'T', 0, G_OPTION_ARG_INT, &thread_count, "Number of rendering threads"},
	{NULL}
};


void
parse_command_line (int *argc, char ***argv)
{
	GOptionContext *option_context = g_option_context_new (NULL);
	g_option_context_add_main_entries (option_context, option_entries, "mandelbrot");
	g_option_context_add_group (option_context, gtk_get_option_group (true));
	g_option_context_parse (option_context, argc, argv, NULL);
}
