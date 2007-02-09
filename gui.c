#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <errno.h>

#include <glib.h>
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
static void connect_signals (GtkMandelApplication *app);
static void open_coord_dlg_response (GtkMandelApplication *app, gint response, gpointer data);
static void save_coord_dlg_response (GtkMandelApplication *app, gint response, gpointer data);
static void mandeldata_updated (GtkMandelApplication *app, gpointer data);
static void area_info_dlg_response (GtkMandelApplication *app, gpointer data);
static void type_dlg_response (GtkMandelApplication *app, gint response, gpointer data);
static GtkAboutDialog *create_about_dlg (GtkWindow *parent);
static void restart_thread (GtkMandelApplication *app);
static void info_dlg_requested (GtkMandelApplication *app, gpointer data);
static void type_dlg_requested (GtkMandelApplication *app, gpointer data);
static void about_dlg_requested (GtkMandelApplication *app, gpointer data);
static void load_coords_requested (GtkMandelApplication *app, gpointer data);
static void save_coords_requested (GtkMandelApplication *app, gpointer data);
static void about_dlg_weak_notify (gpointer data, GObject *object);
static void gtk_mandel_app_dispose (GObject *object);
static void gtk_mandel_app_finalize (GObject *object);


GType
gtk_mandel_application_get_type ()
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			.class_size		= sizeof (GtkMandelApplicationClass),
			.class_init		= (GClassInitFunc) gtk_mandel_application_class_init,
			.instance_size	= sizeof (GtkMandelApplication),
			.instance_init	= (GInstanceInitFunc) gtk_mandel_application_init
		};
		type = g_type_register_static (G_TYPE_OBJECT, "GtkMandelApplication", &info, 0);
	}

	return type;
}


static void
gtk_mandel_application_class_init (GtkMandelApplicationClass *g_class)
{
	G_OBJECT_CLASS (g_class)->dispose = gtk_mandel_app_dispose;
	G_OBJECT_CLASS (g_class)->finalize = gtk_mandel_app_finalize;
}


static void
gtk_mandel_application_init (GtkMandelApplication *app)
{
	app->main_window = fractal_main_window_new ();
	g_object_ref_sink (app->main_window);
	app->disposed = false;
	app->open_coord_chooser = NULL;
	app->save_coord_chooser = NULL;
	app->fractal_info_dlg = NULL;
	app->fractal_type_dlg = NULL;
	app->about_dlg = NULL;
	connect_signals (app);
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
	g_signal_connect_object (G_OBJECT (app->main_window), "load-coords-requested", (GCallback) load_coords_requested, app, G_CONNECT_SWAPPED);

	g_signal_connect_object (G_OBJECT (app->main_window), "save-coords-requested", (GCallback) save_coords_requested, app, G_CONNECT_SWAPPED);

	g_signal_connect_object (G_OBJECT (app->main_window), "info-dialog-requested", (GCallback) info_dlg_requested, app, G_CONNECT_SWAPPED);

	g_signal_connect_object (G_OBJECT (app->main_window), "type-dialog-requested", (GCallback) type_dlg_requested, app, G_CONNECT_SWAPPED);

	g_signal_connect_object (G_OBJECT (app->main_window), "about-dialog-requested", (GCallback) about_dlg_requested, app, G_CONNECT_SWAPPED);

	g_signal_connect_object (G_OBJECT (app->main_window), "mandeldata-updated", (GCallback) mandeldata_updated, app, G_CONNECT_SWAPPED);
}


static void
open_coord_dlg_response (GtkMandelApplication *app, gint response, gpointer data)
{
	char errbuf[1024];
	gtk_widget_hide (app->open_coord_chooser);

	if (response == GTK_RESPONSE_ACCEPT) {
		const char *filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (app->open_coord_chooser));
		struct mandeldata *md = malloc (sizeof (*md));
		bool ok = read_mandeldata (filename, md, errbuf, sizeof (errbuf));
		if (ok) {
			fractal_main_window_set_mandeldata (app->main_window, md);
			restart_thread (app);
		} else {
			fprintf (stderr, "%s: cannot read: %s\n", filename, errbuf);
			free (md);
		}
	}

	gtk_widget_destroy (app->open_coord_chooser);
	g_object_unref (app->open_coord_chooser);
	app->open_coord_chooser = NULL;
}


static void
mandeldata_updated (GtkMandelApplication *app, gpointer data)
{
	const struct mandeldata *const md = fractal_main_window_get_mandeldata (app->main_window);
	// XXX fractal_info_dialog_set_mandeldata (app->fractal_info_dlg, md, GTK_MANDEL (app->mainwin.mandel)->aspect);
	if (app->fractal_info_dlg != NULL)
		fractal_info_dialog_set_mandeldata (app->fractal_info_dlg, md, 1.0 /* aspect */);
	if (app->fractal_type_dlg != NULL)
		fractal_type_dialog_set_mandeldata (app->fractal_type_dlg, md);
}


static void
area_info_dlg_response (GtkMandelApplication *app, gpointer data)
{
	gtk_widget_destroy (GTK_WIDGET (app->fractal_info_dlg));
	g_object_unref (app->fractal_info_dlg);
	app->fractal_info_dlg = NULL;
}


static void
save_coord_dlg_response (GtkMandelApplication *app, gint response, gpointer data)
{
	char errbuf[1024];

	gtk_widget_hide (app->save_coord_chooser);

	if (response == GTK_RESPONSE_ACCEPT) {
	const char *filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (app->save_coord_chooser));
	if (!write_mandeldata (filename, fractal_main_window_get_mandeldata (app->main_window), errbuf, sizeof (errbuf)))
		fprintf (stderr, "%s: cannot write: %s\n", filename, errbuf);
	}

	gtk_widget_destroy (app->save_coord_chooser);
	g_object_unref (app->save_coord_chooser);
	app->save_coord_chooser = NULL;
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
	if (response == GTK_RESPONSE_ACCEPT || response == GTK_RESPONSE_CANCEL || response == GTK_RESPONSE_NONE) {
		gtk_widget_destroy (GTK_WIDGET (app->fractal_type_dlg));
		g_object_unref (app->fractal_type_dlg);
		app->fractal_type_dlg = NULL;
	}
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


static void
info_dlg_requested (GtkMandelApplication *app, gpointer data)
{
	if (app->fractal_info_dlg == NULL) {
		app->fractal_info_dlg = fractal_info_dialog_new (GTK_WINDOW (app->main_window));
		g_object_ref_sink (G_OBJECT (app->fractal_info_dlg));
		g_signal_connect_object (G_OBJECT (app->fractal_info_dlg), "response", (GCallback) area_info_dlg_response, app, G_CONNECT_SWAPPED);

	}
	fractal_info_dialog_set_mandeldata (app->fractal_info_dlg, fractal_main_window_get_mandeldata (app->main_window), 1.0 /* XXX aspect */);
	gtk_widget_show (GTK_WIDGET (app->fractal_info_dlg));
}


static void
type_dlg_requested (GtkMandelApplication *app, gpointer data)
{
	if (app->fractal_type_dlg == NULL) {
		app->fractal_type_dlg = fractal_type_dialog_new (GTK_WINDOW (app->main_window));
		g_object_ref_sink (G_OBJECT (app->fractal_type_dlg));
		g_signal_connect_object (G_OBJECT (app->fractal_type_dlg), "response", (GCallback) type_dlg_response, app, G_CONNECT_SWAPPED);
	}
	fractal_type_dialog_set_mandeldata (app->fractal_type_dlg, fractal_main_window_get_mandeldata (app->main_window));
	gtk_widget_show (GTK_WIDGET (app->fractal_type_dlg));
}


static void
load_coords_requested (GtkMandelApplication *app, gpointer data)
{
	if (app->open_coord_chooser == NULL) {
		fprintf (stderr, "* DEBUG: creating new open dialog\n");
		app->open_coord_chooser = gtk_file_chooser_dialog_new ("Save coordinate file", GTK_WINDOW (app->main_window), GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
		g_object_ref_sink (G_OBJECT (app->open_coord_chooser));
		gtk_window_set_modal (GTK_WINDOW (app->open_coord_chooser), FALSE);
		g_signal_connect_object (G_OBJECT (app->open_coord_chooser), "response", (GCallback) open_coord_dlg_response, app, G_CONNECT_SWAPPED);
	}

	gtk_widget_show (app->open_coord_chooser);
}


static void
save_coords_requested (GtkMandelApplication *app, gpointer data)
{
	if (app->save_coord_chooser == NULL) {
		fprintf (stderr, "* DEBUG: creating new save dialog\n");
		app->save_coord_chooser = gtk_file_chooser_dialog_new ("Save coordinate file", GTK_WINDOW (app->main_window), GTK_FILE_CHOOSER_ACTION_SAVE, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL);
		g_object_ref_sink (G_OBJECT (app->save_coord_chooser));
		gtk_window_set_modal (GTK_WINDOW (app->save_coord_chooser), TRUE);
		gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (app->save_coord_chooser), TRUE);
		g_signal_connect_object (G_OBJECT (app->save_coord_chooser), "response", (GCallback) save_coord_dlg_response, app, G_CONNECT_SWAPPED);
	}

	gtk_widget_show (app->save_coord_chooser);
}


static void
about_dlg_weak_notify (gpointer data, GObject *object)
{
	GtkMandelApplication *app = GTK_MANDEL_APPLICATION (data);
	app->about_dlg = NULL;
}


static void
about_dlg_requested (GtkMandelApplication *app, gpointer data)
{
	if (app->about_dlg == NULL) {
		fprintf (stderr, "* DEBUG: creating new about dialog\n");
		app->about_dlg = create_about_dlg (GTK_WINDOW (app->main_window));
		g_object_ref_sink (G_OBJECT (app->about_dlg));
		g_object_weak_ref (G_OBJECT (app->about_dlg), about_dlg_weak_notify, app);
		g_signal_connect (G_OBJECT (app->about_dlg), "response", (GCallback) gtk_widget_destroy, NULL);
	}
	gtk_widget_show (GTK_WIDGET (app->about_dlg));
}


static void
gtk_mandel_app_dispose (GObject *object)
{
	fprintf (stderr, "* DEBUG: disposing application\n");
	GtkMandelApplication *app = GTK_MANDEL_APPLICATION (object);
	if (!app->disposed) {
		my_gtk_widget_destroy_unref (app->open_coord_chooser);
		my_gtk_widget_destroy_unref (app->save_coord_chooser);
		my_gtk_widget_destroy_unref (GTK_WIDGET (app->fractal_info_dlg));
		my_gtk_widget_destroy_unref (GTK_WIDGET (app->fractal_type_dlg));
		my_gtk_widget_destroy_unref (GTK_WIDGET (app->about_dlg));
		my_gtk_widget_destroy_unref (GTK_WIDGET (app->main_window));
		app->disposed = true;
	}
	G_OBJECT_CLASS (g_type_class_peek_parent (G_OBJECT_GET_CLASS (object)))->dispose (object);
}


static void
gtk_mandel_app_finalize (GObject *object)
{
	fprintf (stderr, "* DEBUG: finalizing application\n");
	G_OBJECT_CLASS (g_type_class_peek_parent (G_OBJECT_GET_CLASS (object)))->finalize (object);
}
