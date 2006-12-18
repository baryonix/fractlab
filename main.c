// ANSI C
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

// POSIX
#include <unistd.h>

// GTK
#include <gtk/gtk.h>
#include <gdk/gdk.h>

// GMP
#include <gmp.h>


#include "cmdline.h"
#include "file.h"
#include "fpdefs.h"
#include "mandelbrot.h"
#include "gtkmandel.h"
#include "defs.h"
#include "gui.h"


struct area_info_data {
	GtkMandel *mandel;
	GtkWidget *dialog, *xmin, *xmax, *ymin, *ymax;
	char xmin_text[1024];
	char xmax_text[1024];
	char ymin_text[1024];
	char ymax_text[1024];
};


void
show_area_info (GtkMenuItem *menuitem, struct area_info_data *data)
{
	gmp_sprintf (data->xmin_text, "%.Ff", data->mandel->md->xmin_f);
	gtk_label_set_text (GTK_LABEL (data->xmin), data->xmin_text);
	gmp_sprintf (data->xmax_text, "%.Ff", data->mandel->md->xmax_f);
	gtk_label_set_text (GTK_LABEL (data->xmax), data->xmax_text);
	gmp_sprintf (data->ymin_text, "%.Ff", data->mandel->md->ymin_f);
	gtk_label_set_text (GTK_LABEL (data->ymin), data->ymin_text);
	gmp_sprintf (data->ymax_text, "%.Ff", data->mandel->md->ymax_f);
	gtk_label_set_text (GTK_LABEL (data->ymax), data->ymax_text);
	gtk_label_set_justify (GTK_LABEL (data->xmin), GTK_JUSTIFY_RIGHT);
	gtk_label_set_justify (GTK_LABEL (data->xmax), GTK_JUSTIFY_RIGHT);
	gtk_label_set_justify (GTK_LABEL (data->ymin), GTK_JUSTIFY_RIGHT);
	gtk_label_set_justify (GTK_LABEL (data->ymax), GTK_JUSTIFY_RIGHT);
	gtk_widget_show_all (data->dialog);
	gtk_dialog_run (GTK_DIALOG (data->dialog));
	gtk_widget_hide_all (data->dialog);
}


int
main (int argc, char **argv)
{
	g_thread_init (NULL);
	gdk_threads_init ();
	gdk_threads_enter ();

	parse_command_line (&argc, &argv);

	mpf_set_default_prec (1024); /* ? */

	int i;
	for (i = 0; i < COLORS; i++) {
		mandelcolors[i].red = (guint16) (sin (2 * M_PI * i / COLORS) * 32767) + 32768;
		mandelcolors[i].green = (guint16) (sin (4 * M_PI * i / COLORS) * 32767) + 32768;
		mandelcolors[i].blue = (guint16) (sin (6 * M_PI * i / COLORS) * 32767) + 32768;
	}

	gtk_init (&argc, &argv);

#if 0
	GtkWidget *tbl = gtk_table_new (2, 4, false);

	GtkWidget *xmin_label = gtk_label_new ("xmin");
	GtkWidget *xmax_label = gtk_label_new ("xmax");
	GtkWidget *ymin_label = gtk_label_new ("ymin");
	GtkWidget *ymax_label = gtk_label_new ("ymax");

	struct area_info_data area_info_data;

	area_info_data.mandel = GTK_MANDEL (img);

	area_info_data.xmin = gtk_label_new (NULL);
	area_info_data.xmax = gtk_label_new (NULL);
	area_info_data.ymin = gtk_label_new (NULL);
	area_info_data.ymax = gtk_label_new (NULL);

	gtk_table_attach_defaults (GTK_TABLE (tbl), xmin_label, 0, 1, 0, 1);
	gtk_table_attach_defaults (GTK_TABLE (tbl), area_info_data.xmin, 1, 2, 0, 1);
	gtk_table_attach_defaults (GTK_TABLE (tbl), xmax_label, 0, 1, 1, 2);
	gtk_table_attach_defaults (GTK_TABLE (tbl), area_info_data.xmax, 1, 2, 1, 2);
	gtk_table_attach_defaults (GTK_TABLE (tbl), ymin_label, 0, 1, 2, 3);
	gtk_table_attach_defaults (GTK_TABLE (tbl), area_info_data.ymin, 1, 2, 2, 3);
	gtk_table_attach_defaults (GTK_TABLE (tbl), ymax_label, 0, 1, 3, 4);
	gtk_table_attach_defaults (GTK_TABLE (tbl), area_info_data.ymax, 1, 2, 3, 4);

	area_info_data.dialog = gtk_dialog_new_with_buttons ("Area Info", GTK_WINDOW (win), GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, GTK_STOCK_OK, GTK_RESPONSE_NONE, NULL);

	GtkWidget *scrolled_win = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_win), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
	gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolled_win), tbl);

	gtk_container_add (GTK_CONTAINER (GTK_DIALOG (area_info_data.dialog)->vbox), scrolled_win);
#endif

	mpf_t xmin, xmax, ymin, ymax;
	mpf_init (xmin);
	mpf_init (xmax);
	mpf_init (ymin);
	mpf_init (ymax);

	bool coords_ok;

	if (option_center_coords != NULL)
		coords_ok = read_cmag_coords_from_file (option_center_coords, xmin, xmax, ymin, ymax);
	else if (option_corner_coords != NULL)
		coords_ok = read_corner_coords_from_file (option_corner_coords, xmin, xmax, ymin, ymax);
	else {
		fprintf (stderr, "No start coordinates specified.\n");
		exit (2);
	}

	if (!coords_ok) {
		perror ("reading coordinates file");
		exit (3);
	}

	GtkMandelApplication *app = gtk_mandel_application_new ();
	gtk_mandel_application_start (app, xmin, xmax, ymin, ymax);
	gtk_main ();
	gdk_threads_leave ();

	return 0;
}
