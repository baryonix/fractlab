#include <stdlib.h>
#include <stdbool.h>

#include <glib.h>
#include <gtk/gtk.h>

#include "cmdline.h"


static gboolean
option_log_factor (const gchar *option_name, const gchar *value, gpointer data, GError **error)
{
	log_factor = strtod (value, NULL);
	return true;
}


gchar *option_center_coords, *option_corner_coords;
gboolean option_mariani_silver, option_successive_refine;
double log_factor = 0.0;


static GOptionEntry option_entries[] = {
	{"center-coords", 'c', 0, G_OPTION_ARG_FILENAME, &option_center_coords, "Read center/magf coordinates from FILE", "FILE"},
	{"corner-coords", 'C', 0, G_OPTION_ARG_FILENAME, &option_corner_coords, "Read corner coordinates from FILE", "FILE"},
	{"log-factor", 'l', 0, G_OPTION_ARG_CALLBACK, option_log_factor, "Factor for logarithmic palette", "X"},
	{"successive-refine", 's', 0, G_OPTION_ARG_NONE, &option_successive_refine, "Use successive refinement algorithm (default)", NULL},
	{"mariani-silver", 'm', 0, G_OPTION_ARG_NONE, &option_mariani_silver, "Use Mariani-Silver algorithm", NULL},
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
