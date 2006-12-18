#include <stdlib.h>

#include <gtk/gtk.h>

#include "defs.h"
#include "mandelbrot.h"
#include "gtkmandel.h"
#include "gui.h"

#define GTK_MANDEL_APPLICATION_GET_CLASS(app) G_TYPE_INSTANCE_GET_CLASS ((app), GtkMandelApplication, GtkMandelApplicationClass)

static void gtk_mandel_application_class_init (GtkMandelApplicationClass *class);
static void gtk_mandel_application_init (GtkMandelApplication *app);
static void create_menus (GtkMandelApplication *app);
static void create_mainwin (GtkMandelApplication *app);
static void connect_signals (GtkMandelApplication *app);
static void area_selected (GtkMandelApplication *app, GtkMandelArea *area, gpointer data);
static void maxiter_updated (GtkMandelApplication *app, gpointer data);
static void render_method_updated (GtkMandelApplication *app, gpointer data);

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
}


static void
gtk_mandel_application_init (GtkMandelApplication *app)
{
	create_menus (app);
	create_mainwin (app);
	connect_signals (app);
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


static void create_mainwin (GtkMandelApplication *app)
{
	app->mainwin.maxiter_label = gtk_label_new ("maxiter:");

	app->mainwin.maxiter_input = gtk_entry_new ();

	app->mainwin.maxiter_hbox = gtk_hbox_new (false, 5);
	gtk_container_add (GTK_CONTAINER (app->mainwin.maxiter_hbox), app->mainwin.maxiter_label);
	gtk_container_add (GTK_CONTAINER (app->mainwin.maxiter_hbox), app->mainwin.maxiter_input);

	app->mainwin.mandel = gtk_mandel_new ();
	gtk_widget_set_size_request (app->mainwin.mandel, PIXELS, PIXELS); // FIXME

	app->mainwin.main_vbox = gtk_vbox_new (false, 5);
	gtk_container_add (GTK_CONTAINER (app->mainwin.main_vbox), app->menu.bar);
	gtk_container_add (GTK_CONTAINER (app->mainwin.main_vbox), app->mainwin.maxiter_hbox);
	gtk_container_add (GTK_CONTAINER (app->mainwin.main_vbox), app->mainwin.mandel);

	app->mainwin.win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_container_add (GTK_CONTAINER (app->mainwin.win), app->mainwin.main_vbox);
}


void
gtk_mandel_application_start (GtkMandelApplication *app, mpf_t xmin, mpf_t xmax, mpf_t ymin, mpf_t ymax)
{
	gtk_widget_show_all (app->mainwin.win);
	gtk_mandel_restart_thread (GTK_MANDEL (app->mainwin.mandel), xmin, xmax, ymin, ymax, 1000, DEFAULT_RENDER_METHOD);
}


GtkMandelApplication *gtk_mandel_application_new ()
{
	GtkMandelApplication *app = g_object_new (gtk_mandel_application_get_type (), NULL);
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
}


static void area_selected (GtkMandelApplication *app, GtkMandelArea *area, gpointer data)
{
	GtkMandel *mandel = GTK_MANDEL (app->mainwin.mandel);
	mpf_t d;
	mpf_init (d);
	mpf_sub (d, area->xmin, area->xmax);
	mpf_abs (d, d);
	long xprec;
	mpf_get_d_2exp (&xprec, d);
	mpf_clear (d);
	/* We are using %f format, so the absolute difference between
	 * the min and max values dictates the required precision. */
	gmp_printf ("* xmin = %.*Ff\n", (int) (-xprec / 3.3219 + 5), area->xmin);
	gmp_printf ("* xmax = %.*Ff\n", (int) (-xprec / 3.3219 + 5), area->xmax);
	gmp_printf ("* ymin = %.*Ff\n", (int) (-xprec / 3.3219 + 5), area->ymin);
	gmp_printf ("* ymax = %.*Ff\n", (int) (-xprec / 3.3219 + 5), area->ymax);
	gtk_mandel_restart_thread (mandel, area->xmin, area->xmax, area->ymin, area->ymax, mandel->md->maxiter, mandel->md->render_method);
}


static void maxiter_updated (GtkMandelApplication *app, gpointer data)
{
	GtkMandel *mandel = GTK_MANDEL (app->mainwin.mandel);
	int i = atoi (gtk_entry_get_text (GTK_ENTRY (app->mainwin.maxiter_input)));
	if (i > 0)
		gtk_mandel_restart_thread (mandel, mandel->md->xmin_f, mandel->md->xmax_f, mandel->md->ymin_f, mandel->md->ymax_f, i, mandel->md->render_method);
}


static void render_method_updated (GtkMandelApplication *app, gpointer data)
{
	GtkMandel *mandel = GTK_MANDEL (app->mainwin.mandel);
	GtkCheckMenuItem *item = GTK_CHECK_MENU_ITEM (data);
	if (!item->active)
		return;
	render_method_t *method = (render_method_t *) g_object_get_data (G_OBJECT (item), "render_method");
	gtk_mandel_restart_thread (mandel, mandel->md->xmin_f, mandel->md->xmax_f, mandel->md->ymin_f, mandel->md->ymax_f, mandel->md->maxiter, *method);
}
