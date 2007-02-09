#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <errno.h>

#include <gtk/gtk.h>

#include "defs.h"
#include "mandelbrot.h"
#include "gtkmandel.h"
#include "gui-util.h"
#include "gui-typedlg.h"
#include "gui-infodlg.h"
#include "gui.h"
#include "util.h"
#include "file.h"

#define GTK_MANDEL_APPLICATION_GET_CLASS(app) G_TYPE_INSTANCE_GET_CLASS ((app), GtkMandelApplication, GtkMandelApplicationClass)

static void gtk_mandel_application_class_init (GtkMandelApplicationClass *class);
static void gtk_mandel_application_init (GtkMandelApplication *app);
static void create_dialogs (GtkMandelApplication *app);
static void connect_signals (GtkMandelApplication *app);
static void open_coord_dlg_response (GtkMandelApplication *app, gint response, gpointer data);
static void save_coord_dlg_response (GtkMandelApplication *app, gint response, gpointer data);
static void mandeldata_updated (GtkMandelApplication *app, gpointer data);
static void area_info_dlg_response (GtkMandelApplication *app, gpointer data);
static void type_dlg_response (GtkMandelApplication *app, gint response, gpointer data);
static GtkAboutDialog *create_about_dlg (GtkWindow *parent);
static void restart_thread (GtkMandelApplication *app);


GType
gtk_mandel_application_get_type ()
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GtkMandelApplicationClass),
			NULL, NULL,
			(GClassInitFunc) gtk_mandel_application_class_init,
			NULL, NULL,
			sizeof (GtkMandelApplication),
			0,
			(GInstanceInitFunc) gtk_mandel_application_init
		};
		type = g_type_register_static (G_TYPE_OBJECT, "GtkMandelApplication", &info, 0);
	}

	return type;
}


static void
gtk_mandel_application_class_init (GtkMandelApplicationClass *class)
{
	//class->icon_factory = gtk_icon_factory_new ();
}


static void
gtk_mandel_application_init (GtkMandelApplication *app)
{
	app->main_window = fractal_main_window_new ();
	g_object_ref_sink (app->main_window);
	create_dialogs (app);
	connect_signals (app);
}


static void
create_dialogs (GtkMandelApplication *app)
{
	app->open_coord_chooser = gtk_file_chooser_dialog_new ("Open coordinate file", GTK_WINDOW (app->main_window), GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
	g_object_ref_sink (G_OBJECT (app->open_coord_chooser));

	app->save_coord_chooser = gtk_file_chooser_dialog_new ("Save coordinate file", GTK_WINDOW (app->main_window), GTK_FILE_CHOOSER_ACTION_SAVE, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL);
	g_object_ref_sink (G_OBJECT (app->save_coord_chooser));
	gtk_window_set_modal (GTK_WINDOW (app->save_coord_chooser), TRUE);
	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (app->save_coord_chooser), TRUE);

	app->fractal_info_dlg = fractal_info_dialog_new (GTK_WINDOW (app->main_window));
	g_object_ref_sink (G_OBJECT (app->fractal_info_dlg));
	app->fractal_type_dlg = fractal_type_dialog_new (GTK_WINDOW (app->main_window));
	g_object_ref_sink (G_OBJECT (app->fractal_type_dlg));

	app->about_dlg = create_about_dlg (GTK_WINDOW (app->main_window));
	g_object_ref_sink (G_OBJECT (app->about_dlg));
}


void
gtk_mandel_application_start (GtkMandelApplication *app)
{
	gtk_widget_show (GTK_WIDGET (app->main_window));
	//restart_thread (app);
}


GtkMandelApplication *
gtk_mandel_application_new (const struct mandeldata *initmd)
{
	GtkMandelApplication *app = g_object_new (gtk_mandel_application_get_type (), NULL);
	if (initmd != NULL) {
		struct mandeldata *md = malloc (sizeof (*md));
		mandeldata_clone (md, initmd);
		fractal_main_window_set_mandeldata (app->main_window, md);
	}
	return app;
}


static void
connect_signals (GtkMandelApplication *app)
{
	g_signal_connect_object (G_OBJECT (app->main_window), "load-coords-requested", (GCallback) gtk_widget_show, app->open_coord_chooser, G_CONNECT_SWAPPED);

	g_signal_connect_object (G_OBJECT (app->main_window), "save-coords-requested", (GCallback) gtk_widget_show, app->save_coord_chooser, G_CONNECT_SWAPPED);

	g_signal_connect_object (G_OBJECT (app->main_window), "info-dialog-requested", (GCallback) gtk_widget_show, app->fractal_info_dlg, G_CONNECT_SWAPPED);

	g_signal_connect_object (G_OBJECT (app->main_window), "type-dialog-requested", (GCallback) gtk_widget_show, app->fractal_type_dlg, G_CONNECT_SWAPPED);

	g_signal_connect_object (G_OBJECT (app->main_window), "about-dialog-requested", (GCallback) gtk_widget_show, app->about_dlg, G_CONNECT_SWAPPED);

	g_signal_connect_object (G_OBJECT (app->main_window), "mandeldata-updated", (GCallback) mandeldata_updated, app, G_CONNECT_SWAPPED);

	/*
	 * This prevents the window from being destroyed
	 * when the close button is clicked.
	 */
	g_signal_connect (G_OBJECT (app->open_coord_chooser), "delete-event", (GCallback) gtk_widget_hide_on_delete, NULL);
	g_signal_connect_object (G_OBJECT (app->open_coord_chooser), "response", (GCallback) open_coord_dlg_response, app, G_CONNECT_SWAPPED);

	g_signal_connect (G_OBJECT (app->save_coord_chooser), "delete-event", (GCallback) gtk_widget_hide_on_delete, NULL);
	g_signal_connect_object (G_OBJECT (app->save_coord_chooser), "response", (GCallback) save_coord_dlg_response, app, G_CONNECT_SWAPPED);

	g_signal_connect (G_OBJECT (app->fractal_info_dlg), "delete-event", (GCallback) gtk_widget_hide_on_delete, NULL);
	g_signal_connect_object (G_OBJECT (app->fractal_info_dlg), "response", (GCallback) area_info_dlg_response, app, G_CONNECT_SWAPPED);

	g_signal_connect (G_OBJECT (app->fractal_type_dlg), "delete-event", (GCallback) gtk_widget_hide_on_delete, NULL);
	g_signal_connect_object (G_OBJECT (app->fractal_type_dlg), "response", (GCallback) type_dlg_response, app, G_CONNECT_SWAPPED);

	g_signal_connect (G_OBJECT (app->about_dlg), "delete-event", (GCallback) gtk_widget_hide_on_delete, NULL);
	g_signal_connect_object (G_OBJECT (app->about_dlg), "response", (GCallback) gtk_widget_hide, app->about_dlg, G_CONNECT_SWAPPED);
}


/* XXX */
static void
open_coord_dlg_response (GtkMandelApplication *app, gint response, gpointer data)
{
	char errbuf[1024];
	gtk_widget_hide (app->open_coord_chooser);

	if (response != GTK_RESPONSE_ACCEPT)
		return;

	const char *filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (app->open_coord_chooser));
	struct mandeldata *md = malloc (sizeof (*md));
	bool ok = read_mandeldata (filename, md, errbuf, sizeof (errbuf));
	if (!ok) {
		fprintf (stderr, "%s: cannot read: %s\n", filename, errbuf);
		free (md);
		return;
	}
	fractal_main_window_set_mandeldata (app->main_window, md);
	restart_thread (app);
}


static void
mandeldata_updated (GtkMandelApplication *app, gpointer data)
{
	const struct mandeldata *const md = fractal_main_window_get_mandeldata (app->main_window);
	// XXX fractal_info_dialog_set_mandeldata (app->fractal_info_dlg, md, GTK_MANDEL (app->mainwin.mandel)->aspect);
	fractal_info_dialog_set_mandeldata (app->fractal_info_dlg, md, 1.0 /* aspect */);
	fractal_type_dialog_set_mandeldata (app->fractal_type_dlg, md);
}


static void
area_info_dlg_response (GtkMandelApplication *app, gpointer data)
{
	gtk_widget_hide (GTK_WIDGET (app->fractal_info_dlg));
}


static void
save_coord_dlg_response (GtkMandelApplication *app, gint response, gpointer data)
{
	char errbuf[1024];

	gtk_widget_hide (app->save_coord_chooser);
	if (response != GTK_RESPONSE_ACCEPT)
		return;

	const char *filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (app->save_coord_chooser));
	if (!write_mandeldata (filename, fractal_main_window_get_mandeldata (app->main_window), errbuf, sizeof (errbuf)))
		fprintf (stderr, "%s: cannot write: %s\n", filename, errbuf);
}


static void
type_dlg_response (GtkMandelApplication *app, gint response, gpointer data)
{
	if (response == GTK_RESPONSE_APPLY || response == GTK_RESPONSE_ACCEPT) {
		struct mandeldata *md = malloc (sizeof (*md));
		fractal_type_dialog_get_mandeldata (app->fractal_type_dlg, md);
		fractal_main_window_set_mandeldata (app->main_window, md);
		restart_thread (app);
	}
	if (response == GTK_RESPONSE_ACCEPT || response == GTK_RESPONSE_CANCEL)
		gtk_widget_hide (GTK_WIDGET (app->fractal_type_dlg));
}


static GtkAboutDialog *
create_about_dlg (GtkWindow *parent)
{
	static const char *authors[] = {"Jan Andres", NULL};
	static const char *documenters[] = {"nobody", NULL};
	GtkAboutDialog *dlg = GTK_ABOUT_DIALOG (gtk_about_dialog_new ());
	gtk_about_dialog_set_name (dlg, "mandel-gtk");
	gtk_about_dialog_set_comments (dlg, "Waste your time and computing power on fractal graphics \xe2\x80\x94 with high performance!\n\nThanks to Robert Munafo (http://www.mrob.com/) for many inspirations on the algorithms used in this program.");
	gtk_about_dialog_set_license (dlg, "This program is free software. License terms are described in the file LICENSE which is included in the software distribution.\n\nWARNING: Rendering copyrighted areas of the Mandelbrot\xe2\x84\xa2 set or other fractals on a high definition capable display may break your Microsoft\xc2\xae Windows\xc2\xae Vista\xc2\xae End-User License Agreement\xc2\xae!");
	gtk_about_dialog_set_wrap_license (dlg, TRUE);
	gtk_about_dialog_set_authors (dlg, authors);
	gtk_about_dialog_set_documenters (dlg, documenters);
	if (parent != NULL)
		gtk_window_set_transient_for (GTK_WINDOW (dlg), parent);
	return dlg;
}


static void
restart_thread (GtkMandelApplication *app)
{
	fractal_main_window_restart (app->main_window);
}
