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
static void create_menus (GtkMandelApplication *app);
static void create_mainwin (GtkMandelApplication *app);
static void create_dialogs (GtkMandelApplication *app);
static void connect_signals (GtkMandelApplication *app);
static void area_selected (GtkMandelApplication *app, struct mandel_area *area, gpointer data);
static void point_for_julia_selected (GtkMandelApplication *app, struct mandel_point *point, gpointer data);
static void render_method_updated (GtkMandelApplication *app, gpointer data);
static void undo_pressed (GtkMandelApplication *app, gpointer data);
static void redo_pressed (GtkMandelApplication *app, gpointer data);
static void restart_thread (GtkMandelApplication *app);
static void rendering_started (GtkMandelApplication *app, gulong bits, gpointer data);
static void rendering_progress (GtkMandelApplication *app, gdouble progress, gpointer data);
static void open_coord_file (GtkMandelApplication *app, gpointer data);
static void open_coord_dlg_response (GtkMandelApplication *app, gint response, gpointer data);
static void save_coord_file (GtkMandelApplication *app, gpointer data);
static void save_coord_dlg_response (GtkMandelApplication *app, gint response, gpointer data);
static void quit_selected (GtkMandelApplication *app, gpointer data);
static void update_gui_from_mandeldata (GtkMandelApplication *app);
static void area_info_selected (GtkMandelApplication *app, gpointer data);
static void fractal_type_clicked (GtkMandelApplication *app, gpointer data);
static void area_info_dlg_response (GtkMandelApplication *app, gpointer data);
static void rendering_stopped (GtkMandelApplication *app, gboolean completed, gpointer data);
static void restart_pressed (GtkMandelApplication *app, gpointer data);
static void stop_pressed (GtkMandelApplication *app, gpointer data);
static void zoom_2exp (GtkMandelApplication *app, long exponent);
static void zoomed_out (GtkMandelApplication *app, gpointer data);
static void threads_updated (GtkMandelApplication *app, gpointer data);
static void update_mandeldata (GtkMandelApplication *app, struct mandeldata *md);
static void gtk_mandel_application_set_area (GtkMandelApplication *app, struct mandel_area *area);
static void zoom_mode_selected (GtkMandelApplication *app, gpointer data);
static void to_julia_mode_selected (GtkMandelApplication *app, gpointer data);
static void type_dlg_response (GtkMandelApplication *app, gint response, gpointer data);
static GtkAboutDialog *create_about_dlg (GtkWindow *parent);


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
	app->updating_gui = true;
	create_menus (app);
	create_mainwin (app);
	create_dialogs (app);
	connect_signals (app);
	app->undo = NULL;
	app->redo = NULL;
	app->md = NULL;
	app->updating_gui = false;
}


static void
create_menus (GtkMandelApplication *app)
{
	render_method_t i;

	app->menu.bar = gtk_menu_bar_new ();

	app->menu.file_item = gtk_menu_item_new_with_label ("File");
	gtk_menu_shell_append (GTK_MENU_SHELL (app->menu.bar), app->menu.file_item);

	app->menu.file_menu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (app->menu.file_item), app->menu.file_menu);

	app->menu.open_coord_item = my_gtk_stock_menu_item_with_label (GTK_STOCK_OPEN, "Open coordinate file...");
	gtk_menu_shell_append (GTK_MENU_SHELL (app->menu.file_menu), app->menu.open_coord_item);

	app->menu.save_coord_item = my_gtk_stock_menu_item_with_label (GTK_STOCK_SAVE_AS, "Save coordinate file...");
	gtk_menu_shell_append (GTK_MENU_SHELL (app->menu.file_menu), app->menu.save_coord_item);

	app->menu.sep1 = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (app->menu.file_menu), app->menu.sep1);

	app->menu.fractal_type_item = my_gtk_stock_menu_item_with_label (GTK_STOCK_PROPERTIES, "Fractal Type and Parameters...");
	gtk_menu_shell_append (GTK_MENU_SHELL (app->menu.file_menu), app->menu.fractal_type_item);

	app->menu.area_info_item = my_gtk_stock_menu_item_with_label (GTK_STOCK_INFO, "Area Info");
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

	app->menu.sep2 = gtk_separator_menu_item_new ();
	gtk_menu_shell_append (GTK_MENU_SHELL (app->menu.file_menu), app->menu.sep2);

	app->menu.quit_item = gtk_image_menu_item_new_from_stock (GTK_STOCK_QUIT, NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (app->menu.file_menu), app->menu.quit_item);

	app->menu.help_item = gtk_menu_item_new_with_label ("Help");
	gtk_menu_shell_append (GTK_MENU_SHELL (app->menu.bar), app->menu.help_item);

	app->menu.help_menu = gtk_menu_new ();
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (app->menu.help_item), app->menu.help_menu);

	app->menu.about_item = gtk_image_menu_item_new_from_stock (GTK_STOCK_ABOUT, NULL);
	gtk_menu_shell_append (GTK_MENU_SHELL (app->menu.help_menu), app->menu.about_item);
}


static void
create_mainwin (GtkMandelApplication *app)
{
	gint ww, wh;

	app->mainwin.undo = GTK_WIDGET (gtk_tool_button_new_from_stock (GTK_STOCK_GO_BACK));
	gtk_widget_set_sensitive (app->mainwin.undo, FALSE);

	app->mainwin.redo = GTK_WIDGET (gtk_tool_button_new_from_stock (GTK_STOCK_GO_FORWARD));
	gtk_widget_set_sensitive (app->mainwin.redo, FALSE);

	app->mainwin.toolbar1_sep1 = GTK_WIDGET (gtk_separator_tool_item_new ());

	app->mainwin.restart = GTK_WIDGET (gtk_tool_button_new_from_stock (GTK_STOCK_REFRESH));

	app->mainwin.stop = GTK_WIDGET (gtk_tool_button_new_from_stock (GTK_STOCK_STOP));
	gtk_widget_set_sensitive (app->mainwin.stop, FALSE);

	app->mainwin.toolbar1_sep2 = GTK_WIDGET (gtk_separator_tool_item_new ());

	app->mainwin.fractal_type = GTK_WIDGET (gtk_tool_button_new_from_stock (GTK_STOCK_PROPERTIES));
	gtk_tool_button_set_label (GTK_TOOL_BUTTON (app->mainwin.fractal_type), "Fractal Type and Parameters...");

	app->mainwin.zoom_out = GTK_WIDGET (gtk_tool_button_new_from_stock (GTK_STOCK_ZOOM_OUT));

	app->mainwin.toolbar1 = gtk_toolbar_new ();
	gtk_toolbar_set_style (GTK_TOOLBAR (app->mainwin.toolbar1), GTK_TOOLBAR_ICONS);
	//gtk_toolbar_set_show_arrow (GTK_TOOLBAR (app->mainwin.toolbar1), FALSE);
	gtk_container_add (GTK_CONTAINER (app->mainwin.toolbar1), app->mainwin.undo);
	gtk_container_add (GTK_CONTAINER (app->mainwin.toolbar1), app->mainwin.redo);
	gtk_container_add (GTK_CONTAINER (app->mainwin.toolbar1), app->mainwin.toolbar1_sep1);
	gtk_container_add (GTK_CONTAINER (app->mainwin.toolbar1), app->mainwin.restart);
	gtk_container_add (GTK_CONTAINER (app->mainwin.toolbar1), app->mainwin.stop);
	gtk_container_add (GTK_CONTAINER (app->mainwin.toolbar1), app->mainwin.toolbar1_sep2);
	gtk_container_add (GTK_CONTAINER (app->mainwin.toolbar1), app->mainwin.fractal_type);
	gtk_container_add (GTK_CONTAINER (app->mainwin.toolbar1), app->mainwin.zoom_out);

	app->mainwin.zoom_mode = GTK_WIDGET (gtk_radio_tool_button_new_from_stock (NULL, GTK_STOCK_ZOOM_IN));
	app->mainwin.mode_group = gtk_radio_tool_button_get_group (GTK_RADIO_TOOL_BUTTON (app->mainwin.zoom_mode));
	app->mainwin.to_julia_mode = GTK_WIDGET (gtk_radio_tool_button_new (app->mainwin.mode_group));
	gtk_tool_button_set_label (GTK_TOOL_BUTTON (app->mainwin.to_julia_mode), "-> Julia");
	gtk_tool_item_set_homogeneous (GTK_TOOL_ITEM (app->mainwin.to_julia_mode), FALSE);

	app->mainwin.toolbar2 = gtk_toolbar_new ();
	gtk_toolbar_set_style (GTK_TOOLBAR (app->mainwin.toolbar2), GTK_TOOLBAR_ICONS);
	//gtk_toolbar_set_show_arrow (GTK_TOOLBAR (app->mainwin.toolbar2), FALSE);
	gtk_container_add (GTK_CONTAINER (app->mainwin.toolbar2), app->mainwin.zoom_mode);
	gtk_container_add (GTK_CONTAINER (app->mainwin.toolbar2), app->mainwin.to_julia_mode);

	app->mainwin.threads_label = gtk_label_new ("Threads");
	gtk_misc_set_alignment (GTK_MISC (app->mainwin.threads_label), 0.0, 0.5);

	app->mainwin.threads_input = gtk_spin_button_new_with_range (1.0, 1024.0, 1.0);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (app->mainwin.threads_input), TRUE);
	gtk_entry_set_alignment (GTK_ENTRY (app->mainwin.threads_input), 1.0);
	gtk_entry_set_width_chars (GTK_ENTRY (app->mainwin.threads_input), 5);

	app->mainwin.controls_table = gtk_table_new (2, 1, FALSE);
	gtk_table_set_homogeneous (GTK_TABLE (app->mainwin.controls_table), FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (app->mainwin.controls_table), 2);
	gtk_table_set_col_spacings (GTK_TABLE (app->mainwin.controls_table), 2);

	gtk_table_attach (GTK_TABLE (app->mainwin.controls_table), app->mainwin.threads_label, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (app->mainwin.controls_table), app->mainwin.threads_input, 1, 2, 0, 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);

	app->mainwin.mandel = gtk_mandel_new ();
	gtk_mandel_set_selection_type (GTK_MANDEL (app->mainwin.mandel), GTK_MANDEL_SELECT_AREA);
	gtk_widget_set_size_request (app->mainwin.mandel, 50, 50);
	/* FIXME how to set initial widget size? */

	app->mainwin.status_info = gtk_progress_bar_new ();
	gtk_progress_set_text_alignment (GTK_PROGRESS (app->mainwin.status_info), 0.0, 0.5);
	/* The default size request of GtkProgressBar is too large here */
	gtk_widget_get_size_request (app->mainwin.status_info, &ww, &wh);
	gtk_widget_set_size_request (app->mainwin.status_info, 10, wh);

	app->mainwin.math_info = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (app->mainwin.math_info), 0.0, 0.5);

	app->mainwin.math_info_frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (app->mainwin.math_info_frame), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (app->mainwin.math_info_frame), app->mainwin.math_info);

	app->mainwin.status_hbox = gtk_hbox_new (false, 2);
	gtk_box_pack_start (GTK_BOX (app->mainwin.status_hbox), app->mainwin.status_info, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (app->mainwin.status_hbox), app->mainwin.math_info_frame, FALSE, FALSE, 0);

	app->mainwin.main_vbox = gtk_vbox_new (false, 2);
	gtk_box_pack_start (GTK_BOX (app->mainwin.main_vbox), app->menu.bar, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (app->mainwin.main_vbox), app->mainwin.toolbar1, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (app->mainwin.main_vbox), app->mainwin.toolbar2, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (app->mainwin.main_vbox), app->mainwin.controls_table, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (app->mainwin.main_vbox), app->mainwin.mandel, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (app->mainwin.main_vbox), app->mainwin.status_hbox, FALSE, FALSE, 0);

	app->mainwin.win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_container_add (GTK_CONTAINER (app->mainwin.win), app->mainwin.main_vbox);
}


static void
create_dialogs (GtkMandelApplication *app)
{
	app->open_coord_chooser = gtk_file_chooser_dialog_new ("Open coordinate file", GTK_WINDOW (app->mainwin.win), GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);

	app->save_coord_chooser = gtk_file_chooser_dialog_new ("Save coordinate file", GTK_WINDOW (app->mainwin.win), GTK_FILE_CHOOSER_ACTION_SAVE, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT, NULL);
	gtk_window_set_modal (GTK_WINDOW (app->save_coord_chooser), TRUE);
	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (app->save_coord_chooser), TRUE);

	app->fractal_info_dlg = fractal_info_dialog_new (GTK_WINDOW (app->mainwin.win));
	app->fractal_type_dlg = fractal_type_dialog_new (GTK_WINDOW (app->mainwin.win));

	app->about_dlg = create_about_dlg (GTK_WINDOW (app->mainwin.win));
}


void
gtk_mandel_application_start (GtkMandelApplication *app)
{
	gtk_widget_show_all (app->mainwin.win);
	restart_thread (app);
}


GtkMandelApplication *
gtk_mandel_application_new (const struct mandeldata *initmd)
{
	GtkMandelApplication *app = g_object_new (gtk_mandel_application_get_type (), NULL);
	if (initmd != NULL) {
		struct mandeldata *md = malloc (sizeof (*md));
		mandeldata_clone (md, initmd);
		gtk_mandel_application_set_mandeldata (app, md);
	}
	return app;
}


static void
connect_signals (GtkMandelApplication *app)
{
	int i;

	g_signal_connect_swapped (G_OBJECT (app->mainwin.mandel), "area-selected", (GCallback) area_selected, app);
	g_signal_connect_swapped (G_OBJECT (app->mainwin.mandel), "point-selected", (GCallback) point_for_julia_selected, app); /* XXX */
	g_signal_connect_swapped (G_OBJECT (app->mainwin.mandel), "rendering-started", (GCallback) rendering_started, app);
	g_signal_connect_swapped (G_OBJECT (app->mainwin.mandel), "rendering-progress", (GCallback) rendering_progress, app);
	g_signal_connect_swapped (G_OBJECT (app->mainwin.mandel), "rendering-stopped", (GCallback) rendering_stopped, app);

	g_signal_connect_swapped (G_OBJECT (app->menu.fractal_type_item), "activate", (GCallback) fractal_type_clicked, app);

	g_signal_connect_swapped (G_OBJECT (app->menu.area_info_item), "activate", (GCallback) area_info_selected, app);

	g_signal_connect_swapped (G_OBJECT (app->menu.open_coord_item), "activate", (GCallback) open_coord_file, app);

	g_signal_connect_swapped (G_OBJECT (app->menu.save_coord_item), "activate", (GCallback) save_coord_file, app);

	for (i = 0; i < RM_MAX; i++)
		g_signal_connect_swapped (G_OBJECT (app->menu.render_method_items[i]), "toggled", (GCallback) render_method_updated, app);

	g_signal_connect_swapped (G_OBJECT (app->menu.quit_item), "activate", (GCallback) quit_selected, app);

	g_signal_connect_object (app->menu.about_item, "activate", (GCallback) gtk_widget_show, app->about_dlg, G_CONNECT_SWAPPED);

	g_signal_connect_swapped (G_OBJECT (app->mainwin.threads_input), "value-changed", (GCallback) threads_updated, app);

	g_signal_connect_swapped (G_OBJECT (app->mainwin.undo), "clicked", (GCallback) undo_pressed, app);

	g_signal_connect_swapped (G_OBJECT (app->mainwin.redo), "clicked", (GCallback) redo_pressed, app);

	g_signal_connect_swapped (G_OBJECT (app->mainwin.restart), "clicked", (GCallback) restart_pressed, app);

	g_signal_connect_swapped (G_OBJECT (app->mainwin.stop), "clicked", (GCallback) stop_pressed, app);

	g_signal_connect_swapped (G_OBJECT (app->mainwin.fractal_type), "clicked", (GCallback) fractal_type_clicked, app);

	g_signal_connect_swapped (G_OBJECT (app->mainwin.zoom_out), "clicked", (GCallback) zoomed_out, app);

	g_signal_connect_swapped (G_OBJECT (app->mainwin.zoom_mode), "toggled", (GCallback) zoom_mode_selected, app);

	g_signal_connect_swapped (G_OBJECT (app->mainwin.to_julia_mode), "toggled", (GCallback) to_julia_mode_selected, app);

	/*
	 * This prevents the window from being destroyed
	 * when the close button is clicked.
	 */
	g_signal_connect (G_OBJECT (app->open_coord_chooser), "delete-event", (GCallback) gtk_widget_hide_on_delete, NULL);
	g_signal_connect_swapped (G_OBJECT (app->open_coord_chooser), "response", (GCallback) open_coord_dlg_response, app);

	g_signal_connect (G_OBJECT (app->save_coord_chooser), "delete-event", (GCallback) gtk_widget_hide_on_delete, NULL);
	g_signal_connect_swapped (G_OBJECT (app->save_coord_chooser), "response", (GCallback) save_coord_dlg_response, app);

	g_signal_connect_swapped (G_OBJECT (app->mainwin.win), "delete-event", (GCallback) quit_selected, app);

	g_signal_connect (G_OBJECT (app->fractal_info_dlg), "delete-event", (GCallback) gtk_widget_hide_on_delete, NULL);
	g_signal_connect_swapped (G_OBJECT (app->fractal_info_dlg), "response", (GCallback) area_info_dlg_response, app);

	g_signal_connect (app->fractal_type_dlg, "delete-event", (GCallback) gtk_widget_hide_on_delete, NULL);
	g_signal_connect_swapped (app->fractal_type_dlg, "response", (GCallback) type_dlg_response, app);

	g_signal_connect (GTK_DIALOG (app->about_dlg), "delete-event", (GCallback) gtk_widget_hide_on_delete, NULL);
	g_signal_connect_object (app->about_dlg, "response", (GCallback) gtk_widget_hide, app->about_dlg, G_CONNECT_SWAPPED);
}


static void
area_selected (GtkMandelApplication *app, struct mandel_area *area, gpointer data)
{
	gtk_mandel_application_set_area (app, area);
	restart_thread (app);
}


static void
point_for_julia_selected (GtkMandelApplication *app, struct mandel_point *point, gpointer data)
{
	struct mandeldata *md = malloc (sizeof (*md));
	mandeldata_init (md, fractal_type_by_id (FRACTAL_JULIA));
	mandeldata_set_defaults (md);
	struct mandelbrot_param *oldmparam = (struct mandelbrot_param *) app->md->type_param;
	struct julia_param *jparam = (struct julia_param *) md->type_param;
	jparam->mjparam.zpower = oldmparam->mjparam.zpower;
	mpf_set (jparam->param.real, point->real);
	mpf_set (jparam->param.imag, point->imag);
	gtk_mandel_application_set_mandeldata (app, md);
	restart_thread (app);
}


static void
render_method_updated (GtkMandelApplication *app, gpointer data)
{
	if (app->updating_gui)
		return;
	GtkCheckMenuItem *item = GTK_CHECK_MENU_ITEM (data);
	if (!item->active)
		return;
	render_method_t *method = (render_method_t *) g_object_get_data (G_OBJECT (item), "render_method");
	gtk_mandel_set_render_method (GTK_MANDEL (app->mainwin.mandel), *method);
	restart_thread (app);
}


void
gtk_mandel_application_set_mandeldata (GtkMandelApplication *app, struct mandeldata *md)
{
	GSList *l;
	if (app->md != NULL) {
		app->undo = g_slist_prepend (app->undo, (gpointer) app->md);
		gtk_widget_set_sensitive (app->mainwin.undo, TRUE);
	}
	update_mandeldata (app, md);
	l = app->redo;
	while (l != NULL) {
		GSList *next_l = g_slist_next (l);
		mandeldata_clear (l->data);
		free (l->data);
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
	app->redo = g_slist_prepend (app->redo, (gpointer) app->md);
	update_mandeldata (app, (struct mandeldata *) app->undo->data);
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
	app->undo = g_slist_prepend (app->undo, (gpointer) app->md);
	update_mandeldata (app, (struct mandeldata *) app->redo->data);
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
	gtk_mandel_start (GTK_MANDEL (app->mainwin.mandel));
}


static void
rendering_started (GtkMandelApplication *app, gulong bits, gpointer data)
{
	char buf[64];
	int r;

	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (app->mainwin.status_info), 0.0);
	gtk_progress_bar_set_text (GTK_PROGRESS_BAR (app->mainwin.status_info), "Rendering");
	gtk_widget_set_sensitive (app->mainwin.stop, TRUE);
	if (bits == 0)
		gtk_label_set_text (GTK_LABEL (app->mainwin.math_info), "FP");
	else {
		r = snprintf (buf, sizeof (buf), "MP (%lu bits)", bits);
		if (r < 0 || r >= sizeof (buf))
			return;
		gtk_label_set_text (GTK_LABEL (app->mainwin.math_info), buf);
	}
}


static void
open_coord_file (GtkMandelApplication *app, gpointer data)
{
	gtk_widget_show (app->open_coord_chooser);
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
	gtk_mandel_application_set_mandeldata (app, md);
	restart_thread (app);
}


static void
quit_selected (GtkMandelApplication *app, gpointer data)
{
	gtk_main_quit ();
}



static void
update_gui_from_mandeldata (GtkMandelApplication *app)
{
	app->updating_gui = true;
	gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (app->mainwin.zoom_mode), TRUE);

	app->updating_gui = false;
	fractal_info_dialog_set_mandeldata (app->fractal_info_dlg, app->md, GTK_MANDEL (app->mainwin.mandel)->aspect);
	fractal_type_dialog_set_mandeldata (app->fractal_type_dlg, app->md);
}


static void
area_info_dlg_response (GtkMandelApplication *app, gpointer data)
{
	gtk_widget_hide (GTK_WIDGET (app->fractal_info_dlg));
}


static void
area_info_selected (GtkMandelApplication *app, gpointer data)
{
	gtk_widget_show (GTK_WIDGET (app->fractal_info_dlg));
}


static void
save_coord_file (GtkMandelApplication *app, gpointer data)
{
	gtk_widget_show (app->save_coord_chooser);
}


static void
save_coord_dlg_response (GtkMandelApplication *app, gint response, gpointer data)
{
	char errbuf[1024];

	gtk_widget_hide (app->save_coord_chooser);
	if (response != GTK_RESPONSE_ACCEPT)
		return;

	const char *filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (app->save_coord_chooser));
	if (!write_mandeldata (filename, app->md, errbuf, sizeof (errbuf)))
		fprintf (stderr, "%s: cannot write: %s\n", filename, errbuf);
}


static void
rendering_stopped (GtkMandelApplication *app, gboolean completed, gpointer data)
{
	const char *msg;
	char buf[256];
	double progress;
	if (completed) {
		msg = "Finished";
		progress = 1.0;
	} else {
		msg = "Stopped";
		progress = gtk_mandel_get_progress (GTK_MANDEL (app->mainwin.mandel));
		int r = snprintf (buf, sizeof (buf), "Stopped (%.1f%%)", progress * 100.0);
		if (r > 0 && r < sizeof (buf))
			msg = buf;
	}

	gtk_widget_set_sensitive (app->mainwin.stop, FALSE);
	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (app->mainwin.status_info), progress);
	gtk_progress_bar_set_text (GTK_PROGRESS_BAR (app->mainwin.status_info), msg);
}


static void
restart_pressed (GtkMandelApplication *app, gpointer data)
{
	restart_thread (app);
}


static void
stop_pressed (GtkMandelApplication *app, gpointer data)
{
	GtkMandel *mandel = GTK_MANDEL (app->mainwin.mandel);
	gtk_progress_bar_set_text (GTK_PROGRESS_BAR (app->mainwin.status_info), "Stopping...");
	gtk_mandel_stop (mandel);
}


static void
zoom_2exp (GtkMandelApplication *app, long exponent)
{
	struct mandeldata *md = malloc (sizeof (*md));
	mandeldata_clone (md, app->md);
	if (exponent > 0)
		mpf_mul_2exp (md->area.magf, md->area.magf, exponent);
	else
		mpf_div_2exp (md->area.magf, md->area.magf, -exponent);
	gtk_mandel_application_set_mandeldata (app, md);
	restart_thread (app);
}


static void
zoomed_out (GtkMandelApplication *app, gpointer data)
{
	zoom_2exp (app, -1);
}


static void
threads_updated (GtkMandelApplication *app, gpointer data)
{
	if (app->updating_gui)
		return;
	gtk_mandel_set_thread_count (GTK_MANDEL (app->mainwin.mandel), gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (app->mainwin.threads_input)));
}


void
gtk_mandel_application_set_threads (GtkMandelApplication *app, unsigned threads)
{
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (app->mainwin.threads_input), threads);
}


static void
update_mandeldata (GtkMandelApplication *app, struct mandeldata *md)
{
	app->md = md;
	gtk_mandel_set_mandeldata (GTK_MANDEL (app->mainwin.mandel), md);
	update_gui_from_mandeldata (app);
}


/* XXX this should disappear */
static void
gtk_mandel_application_set_area (GtkMandelApplication *app, struct mandel_area *area)
{
	struct mandeldata *md = malloc (sizeof (*md));
	mandeldata_clone (md, app->md);
	mpf_set (md->area.center.real, area->center.real);
	mpf_set (md->area.center.imag, area->center.imag);
	mpf_set (md->area.magf, area->magf);
	gtk_mandel_application_set_mandeldata (app, md);
}


void
gtk_mandel_app_set_mode (GtkMandelApplication *app, GtkMandelAppMode mode)
{
	app->mode = mode;
	switch (app->mode) {
		case GTK_MANDEL_APP_MODE_ZOOM: {
			gtk_mandel_set_selection_type (GTK_MANDEL (app->mainwin.mandel), GTK_MANDEL_SELECT_AREA);
			break;
		}
		case GTK_MANDEL_APP_MODE_TO_JULIA: {
			gtk_mandel_set_selection_type (GTK_MANDEL (app->mainwin.mandel), GTK_MANDEL_SELECT_POINT);
			break;
		}
		default: {
			fprintf (stderr, "* Invalid GtkMandelApplication mode %d\n", (int) app->mode);
			break;
		}
	}
}


static void
zoom_mode_selected (GtkMandelApplication *app, gpointer data)
{
	gtk_mandel_app_set_mode (app, GTK_MANDEL_APP_MODE_ZOOM);
}


static void
to_julia_mode_selected (GtkMandelApplication *app, gpointer data)
{
	gtk_mandel_app_set_mode (app, GTK_MANDEL_APP_MODE_TO_JULIA);
}


static void
rendering_progress (GtkMandelApplication *app, gdouble progress, gpointer data)
{
	char buf[256];
	int r = snprintf (buf, sizeof (buf), "Rendering (%.1f%%)", (double) progress * 100.0);
	if (r > 0 && r < sizeof (buf))
		gtk_progress_bar_set_text (GTK_PROGRESS_BAR (app->mainwin.status_info), buf);
	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (app->mainwin.status_info), progress);
}


static void
type_dlg_response (GtkMandelApplication *app, gint response, gpointer data)
{
	if (response == GTK_RESPONSE_APPLY || response == GTK_RESPONSE_ACCEPT) {
		struct mandeldata *md = malloc (sizeof (*md));
		fractal_type_dialog_get_mandeldata (app->fractal_type_dlg, md);
		gtk_mandel_application_set_mandeldata (app, md);
		restart_thread (app);
	}
	if (response == GTK_RESPONSE_ACCEPT || response == GTK_RESPONSE_CANCEL)
		gtk_widget_hide (GTK_WIDGET (app->fractal_type_dlg));
}


static void
fractal_type_clicked (GtkMandelApplication *app, gpointer data)
{
	fractal_type_dialog_set_mandeldata (app->fractal_type_dlg, app->md);
	gtk_widget_show (GTK_WIDGET (app->fractal_type_dlg));
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
