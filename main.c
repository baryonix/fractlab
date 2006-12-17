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


#define DEFAULT_RENDER_METHOD RM_SUCCESSIVE_REFINE


void
my_mpz_to_mpf (mpf_t rop, mpz_t op, unsigned frac_limbs)
{
	mpf_set_z (rop, op);
	mpf_div_2exp (rop, rop, frac_limbs * mp_bits_per_limb);
}


void
new_maxiter (GtkWidget *widget, gpointer *data)
{
	GtkMandel *mandel = GTK_MANDEL (data);
	int i = atoi (gtk_entry_get_text (GTK_ENTRY (widget)));
	if (i > 0)
		gtk_mandel_restart_thread (mandel, mandel->md->xmin_f, mandel->md->xmax_f, mandel->md->ymin_f, mandel->md->ymax_f, i, mandel->md->render_method);
}


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
	gtk_label_set_text (data->xmin, data->xmin_text);
	gmp_sprintf (data->xmax_text, "%.Ff", data->mandel->md->xmax_f);
	gtk_label_set_text (data->xmax, data->xmax_text);
	gmp_sprintf (data->ymin_text, "%.Ff", data->mandel->md->ymin_f);
	gtk_label_set_text (data->ymin, data->ymin_text);
	gmp_sprintf (data->ymax_text, "%.Ff", data->mandel->md->ymax_f);
	gtk_label_set_text (data->ymax, data->ymax_text);
	gtk_label_set_justify (data->xmin, GTK_JUSTIFY_RIGHT);
	gtk_label_set_justify (data->xmax, GTK_JUSTIFY_RIGHT);
	gtk_label_set_justify (data->ymin, GTK_JUSTIFY_RIGHT);
	gtk_label_set_justify (data->ymax, GTK_JUSTIFY_RIGHT);
	gtk_widget_show_all (data->dialog);
	gtk_dialog_run (data->dialog);
	gtk_widget_hide_all (data->dialog);
}


struct rm_update_data {
	GtkMandel *mandel;
	render_method_t method;
};


void
update_render_method (GtkCheckMenuItem *menuitem, struct rm_update_data *data)
{
	if (!menuitem->active)
		return;
	GtkMandel *mandel = data->mandel;
	gtk_mandel_restart_thread (mandel, mandel->md->xmin_f, mandel->md->xmax_f, mandel->md->ymin_f, mandel->md->ymax_f, mandel->md->maxiter, data->method);
}


int
main (int argc, char **argv)
{
	g_thread_init (NULL);
	gdk_threads_init ();
	gdk_threads_enter ();

	parse_command_line (&argc, &argv);

	mpf_set_default_prec (1024);

	int i;
	for (i = 0; i < COLORS; i++) {
		mandelcolors[i].red = (guint16) (sin (2 * M_PI * i / COLORS) * 32767) + 32768;
		mandelcolors[i].green = (guint16) (sin (4 * M_PI * i / COLORS) * 32767) + 32768;
		mandelcolors[i].blue = (guint16) (sin (6 * M_PI * i / COLORS) * 32767) + 32768;
	}

	GtkWidget *win, *img;
	gtk_init (&argc, &argv);

	GtkWidget *menu_items = gtk_menu_item_new_with_label ("Area Info");

	GtkWidget *render_menu = gtk_menu_new ();

	GtkWidget *render_method_item = gtk_menu_item_new_with_label ("Rendering Method");
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (render_method_item), render_menu);

	GtkWidget *menu = gtk_menu_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), render_method_item);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);

	GtkWidget *file_menu = gtk_menu_item_new_with_label ("File");
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (file_menu), menu);

	GtkWidget *menu_bar = gtk_menu_bar_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (menu_bar), file_menu);

	GtkWidget *hbox = gtk_hbox_new (false, 5);
	GtkWidget *maxiter_label = gtk_label_new ("maxiter:");
	gtk_container_add (GTK_CONTAINER (hbox), maxiter_label);
	GtkWidget *maxiter_entry = gtk_entry_new ();
	gtk_container_add (GTK_CONTAINER (hbox), maxiter_entry);

	GtkWidget *vbox = gtk_vbox_new (false, 5);
	gtk_container_add (GTK_CONTAINER (vbox), menu_bar);
	gtk_container_add (GTK_CONTAINER (vbox), hbox);

	win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	img = gtk_mandel_new ();
	gtk_widget_set_size_request (img, PIXELS, PIXELS);
	gtk_container_add (GTK_CONTAINER (vbox), img);
	gtk_container_add (GTK_CONTAINER (win), vbox);

	gtk_signal_connect (GTK_OBJECT (maxiter_entry), "activate", (GtkSignalFunc) new_maxiter, (gpointer) img);

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

	gtk_signal_connect (GTK_OBJECT (menu_items), "activate", (GtkSignalFunc) show_area_info, (gpointer) &area_info_data);

	GSList *render_item_group = NULL;
	for (i = 0; i < RM_MAX; i++) {
		GtkWidget *item = gtk_radio_menu_item_new_with_label (render_item_group, render_method_names[i]);
		render_item_group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (item));
		if (i == DEFAULT_RENDER_METHOD)
			gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), TRUE);
		struct rm_update_data *d = malloc (sizeof (struct rm_update_data));
		d->mandel = img;
		d->method = i;
		gtk_signal_connect (GTK_OBJECT (item), "toggled", (GtkSignalFunc) update_render_method, (gpointer) d);
		gtk_menu_shell_append (GTK_MENU_SHELL (render_menu), item);
	}

	gtk_widget_show_all (win);

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

	gtk_mandel_restart_thread (GTK_MANDEL (img), xmin, xmax, ymin, ymax, 1000, DEFAULT_RENDER_METHOD);
	gtk_main ();
	gdk_threads_leave ();

	return 0;
}
