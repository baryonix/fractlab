#include <stdlib.h>
#include <math.h>

#include <gtk/gtk.h>

#include "defs.h"
#include "mandelbrot.h"
#include "gtkmandel.h"
#include "gui.h"
#include "util.h"

#define GTK_MANDEL_APPLICATION_GET_CLASS(app) G_TYPE_INSTANCE_GET_CLASS ((app), GtkMandelApplication, GtkMandelApplicationClass)

static void gtk_mandel_application_class_init (GtkMandelApplicationClass *class);
static void gtk_mandel_application_init (GtkMandelApplication *app);
static void create_menus (GtkMandelApplication *app);
static void create_mainwin (GtkMandelApplication *app);
static void connect_signals (GtkMandelApplication *app);
static void area_selected (GtkMandelApplication *app, GtkMandelArea *area, gpointer data);
static void maxiter_updated (GtkMandelApplication *app, gpointer data);
static void render_method_updated (GtkMandelApplication *app, gpointer data);
static void log_colors_updated (GtkMandelApplication *app, gpointer data);
static void undo_pressed (GtkMandelApplication *app, gpointer data);
static void redo_pressed (GtkMandelApplication *app, gpointer data);
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
	render_method_t i;
	for (i = 0; i < RM_MAX; i++)
		class->render_methods[i] = i;
	//class->icon_factory = gtk_icon_factory_new ();
}


static void
gtk_mandel_application_init (GtkMandelApplication *app)
{
	create_menus (app);
	create_mainwin (app);
	connect_signals (app);
	app->undo = NULL;
	app->redo = NULL;
	app->area = NULL;
}


static void create_menus (GtkMandelApplication *app)
{
	render_method_t i;

	app->menu.bar = gtk_menu_bar_new ();

	app->menu.file_item = gtk_menu_item_new_with_label ("File");
	gtk_menu_shell_append (GTK_MENU_SHELL (app->menu.bar), app->menu.file_item);

	app->menu.file_menu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (app->menu.file_item), app->menu.file_menu);

	app->menu.area_info_item = gtk_menu_item_new_with_label ("Area Info");
	gtk_menu_shell_append (GTK_MENU_SHELL (app->menu.file_menu), app->menu.area_info_item);

	app->menu.render_item = gtk_menu_item_new_with_label ("Rendering Method");
	gtk_menu_shell_append (GTK_MENU_SHELL (app->menu.file_menu), app->menu.render_item);

	app->menu.render_menu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (app->menu.render_item), app->menu.render_menu);

	app->menu.render_item_group = NULL;
	for (i = 0; i < RM_MAX; i++) {
		GtkWidget *item = gtk_radio_menu_item_new_with_label (app->menu.render_item_group, render_method_names[i]);
		app->menu.render_method_items[i] = item;
		app->menu.render_item_group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (item));
		gtk_menu_shell_append (GTK_MENU_SHELL (app->menu.render_menu), item);
		g_object_set_data (G_OBJECT (item), "render_method", GTK_MANDEL_APPLICATION_GET_CLASS (app)->render_methods + i);
		if (i == DEFAULT_RENDER_METHOD)
			gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), TRUE);
	}

	app->menu.quit_item = gtk_menu_item_new_with_label ("Quit");
	gtk_menu_shell_append (GTK_MENU_SHELL (app->menu.file_menu), app->menu.quit_item);
}


static void
create_mainwin (GtkMandelApplication *app)
{
	app->mainwin.undo = gtk_button_new_from_stock (GTK_STOCK_GO_BACK);
	gtk_widget_set_sensitive (app->mainwin.undo, FALSE);

	app->mainwin.redo = gtk_button_new_from_stock (GTK_STOCK_GO_FORWARD);
	gtk_widget_set_sensitive (app->mainwin.redo, FALSE);

	app->mainwin.undo_hbox = gtk_hbox_new (5, false);
	gtk_container_add (GTK_CONTAINER (app->mainwin.undo_hbox), app->mainwin.undo);
	gtk_container_add (GTK_CONTAINER (app->mainwin.undo_hbox), app->mainwin.redo);

	app->mainwin.maxiter_label = gtk_label_new ("maxiter:");

	app->mainwin.maxiter_input = gtk_entry_new ();

	app->mainwin.maxiter_hbox = gtk_hbox_new (false, 5);
	gtk_container_add (GTK_CONTAINER (app->mainwin.maxiter_hbox), app->mainwin.maxiter_label);
	gtk_container_add (GTK_CONTAINER (app->mainwin.maxiter_hbox), app->mainwin.maxiter_input);

	app->mainwin.log_colors_checkbox = gtk_check_button_new_with_label ("Logarithmic Colors");

	app->mainwin.log_colors_input = gtk_entry_new ();
	//gtk_entry_set_text (GTK_ENTRY (app->mainwin.log_colors_input), "foobar");
	gtk_widget_set_sensitive (app->mainwin.log_colors_input, FALSE);

	app->mainwin.log_colors_hbox = gtk_hbox_new (false, 5);
	gtk_container_add (GTK_CONTAINER (app->mainwin.log_colors_hbox), app->mainwin.log_colors_checkbox);
	gtk_container_add (GTK_CONTAINER (app->mainwin.log_colors_hbox), app->mainwin.log_colors_input);

	app->mainwin.mandel = gtk_mandel_new ();
	gtk_widget_set_size_request (app->mainwin.mandel, PIXELS, PIXELS); // FIXME

	app->mainwin.main_vbox = gtk_vbox_new (false, 5);
	gtk_container_add (GTK_CONTAINER (app->mainwin.main_vbox), app->menu.bar);
	gtk_container_add (GTK_CONTAINER (app->mainwin.main_vbox), app->mainwin.undo_hbox);
	gtk_container_add (GTK_CONTAINER (app->mainwin.main_vbox), app->mainwin.maxiter_hbox);
	gtk_container_add (GTK_CONTAINER (app->mainwin.main_vbox), app->mainwin.log_colors_hbox);
	gtk_container_add (GTK_CONTAINER (app->mainwin.main_vbox), app->mainwin.mandel);

	app->mainwin.win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_container_add (GTK_CONTAINER (app->mainwin.win), app->mainwin.main_vbox);
}


void
gtk_mandel_application_start (GtkMandelApplication *app)
{
	gtk_widget_show_all (app->mainwin.win);
	restart_thread (app);
}


GtkMandelApplication *
gtk_mandel_application_new (GtkMandelArea *area, unsigned maxiter, render_method_t render_method, double log_factor)
{
	GtkMandelApplication *app = g_object_new (gtk_mandel_application_get_type (), NULL);
	gtk_mandel_application_set_area (app, area);
	app->maxiter = maxiter;
	app->render_method = render_method;
	app->log_factor = log_factor;
	return app;
}


static void
connect_signals (GtkMandelApplication *app)
{
	int i;
	g_signal_connect_swapped (G_OBJECT (app->mainwin.mandel), "selection", (GCallback) area_selected, app);
	g_signal_connect_swapped (G_OBJECT (app->mainwin.maxiter_input), "activate", (GCallback) maxiter_updated, app);
	for (i = 0; i < RM_MAX; i++)
		g_signal_connect_swapped (G_OBJECT (app->menu.render_method_items[i]), "toggled", (GCallback) render_method_updated, app);
	g_signal_connect_swapped (G_OBJECT (app->mainwin.log_colors_checkbox), "toggled", (GCallback) log_colors_updated, app);
	g_signal_connect_swapped (G_OBJECT (app->mainwin.log_colors_input), "activate", (GCallback) log_colors_updated, app);
	g_signal_connect_swapped (G_OBJECT (app->mainwin.undo), "clicked", (GCallback) undo_pressed, app);
	g_signal_connect_swapped (G_OBJECT (app->mainwin.redo), "clicked", (GCallback) redo_pressed, app);
}


static void area_selected (GtkMandelApplication *app, GtkMandelArea *area, gpointer data)
{
	char xmin_buf[1024], xmax_buf[1024], ymin_buf[1024], ymax_buf[1024];
	coords_to_string (area->xmin, area->xmax, area->ymin, area->ymax, xmin_buf, xmax_buf, ymin_buf, ymax_buf, 1024);
	printf ("* xmin = %s\n* xmax = %s\n* ymin = %s\n* ymax = %s\n", xmin_buf, xmax_buf, ymin_buf, ymax_buf);
	gtk_mandel_application_set_area (app, area);
	restart_thread (app);
}


static void maxiter_updated (GtkMandelApplication *app, gpointer data)
{
	int i = atoi (gtk_entry_get_text (GTK_ENTRY (app->mainwin.maxiter_input)));
	if (i > 0) {
		app->maxiter = i;
		restart_thread (app);
	}
}


static void render_method_updated (GtkMandelApplication *app, gpointer data)
{
	GtkCheckMenuItem *item = GTK_CHECK_MENU_ITEM (data);
	if (!item->active)
		return;
	render_method_t *method = (render_method_t *) g_object_get_data (G_OBJECT (item), "render_method");
	app->render_method = *method;
	restart_thread (app);
}


static void
log_colors_updated (GtkMandelApplication *app, gpointer data)
{
	gboolean active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (app->mainwin.log_colors_checkbox));
	gtk_widget_set_sensitive (app->mainwin.log_colors_input, active);
	double lf = 0.0;
	if (active)
		lf = strtod (gtk_entry_get_text (GTK_ENTRY (app->mainwin.log_colors_input)), NULL);
	if (isfinite (lf)) {
		app->log_factor = lf;
		restart_thread (app);
	}
}


void
gtk_mandel_application_set_area (GtkMandelApplication *app, GtkMandelArea *area)
{
	GSList *l;
	if (app->area != NULL) {
		app->undo = g_slist_prepend (app->undo, (gpointer) app->area);
		gtk_widget_set_sensitive (app->mainwin.undo, TRUE);
	}
	app->area = area;
	g_object_ref (app->area);
	l = app->redo;
	while (l != NULL) {
		GSList *next_l = g_slist_next (l);
		g_object_unref (G_OBJECT (l->data));
		g_slist_free_1 (l);
		l = next_l;
	}
	app->redo = NULL;
	gtk_widget_set_sensitive (app->mainwin.redo, FALSE);
}


static void
undo_pressed (GtkMandelApplication *app, gpointer data)
{
	if (app->undo == NULL) {
		fprintf (stderr, "! Undo called with empty history.\n");
		return;
	}
	app->redo = g_slist_prepend (app->redo, (gpointer) app->area);
	app->area = (GtkMandelArea *) app->undo->data;
	GSList *old = app->undo;
	app->undo = g_slist_next (app->undo);
	g_slist_free_1 (old);
	if (app->undo == NULL)
		gtk_widget_set_sensitive (app->mainwin.undo, FALSE);
	gtk_widget_set_sensitive (app->mainwin.redo, TRUE);
	restart_thread (app);
}


/* FIXME: This is an exact "mirror" of undo_pressed(). */
static void
redo_pressed (GtkMandelApplication *app, gpointer data)
{
	if (app->redo == NULL) {
		fprintf (stderr, "! Redo called with empty history.\n");
		return;
	}
	app->undo = g_slist_prepend (app->undo, (gpointer) app->area);
	app->area = (GtkMandelArea *) app->redo->data;
	GSList *old = app->redo;
	app->redo = g_slist_next (app->redo);
	g_slist_free_1 (old);
	if (app->redo == NULL)
		gtk_widget_set_sensitive (app->mainwin.redo, FALSE);
	gtk_widget_set_sensitive (app->mainwin.undo, TRUE);
	restart_thread (app);
}


static void
restart_thread (GtkMandelApplication *app)
{
	GtkMandel *mandel = GTK_MANDEL (app->mainwin.mandel);
	GtkMandelArea *area = app->area;
	gtk_mandel_restart_thread (mandel, area->xmin, area->xmax, area->ymin, area->ymax, app->maxiter, app->render_method, app->log_factor);
}
