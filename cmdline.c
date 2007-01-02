#include <stdlib.h>
#include <stdbool.h>

#include <glib.h>
#include <gtk/gtk.h>

#include "cmdline.h"


gchar *option_start_coords;


static GOptionEntry option_entries[] = {
	{"start-coords", 's', 0, G_OPTION_ARG_FILENAME, &option_start_coords, "Read start coordinates from FILE", "FILE"},
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
