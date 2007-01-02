#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <errno.h>

#include <gtk/gtk.h>

#include "defs.h"
#include "mandelbrot.h"
#include "gtkmandel.h"
#include "gui.h"
#include "util.h"
#include "file.h"
#include "cmdline.h"

#define GTK_MANDEL_APPLICATION_GET_CLASS(app) G_TYPE_INSTANCE_GET_CLASS ((app), GtkMandelApplication, GtkMandelApplicationClass)

static void gtk_mandel_application_class_init (GtkMandelApplicationClass *class);
static void gtk_mandel_application_init (GtkMandelApplication *app);
static void create_menus (GtkMandelApplication *app);
static void create_mainwin (GtkMandelApplication *app);
static void create_dialogs (GtkMandelApplication *app);
static void create_area_info (GtkMandelApplication *app);
static void connect_signals (GtkMandelApplication *app);
static void area_selected (GtkMandelApplication *app, GtkMandelArea *area, gpointer data);
static void maxiter_updated (GtkMandelApplication *app, gpointer data);
static void render_method_updated (GtkMandelApplication *app, gpointer data);
static void log_colors_updated (GtkMandelApplication *app, gpointer data);
static void undo_pressed (GtkMandelApplication *app, gpointer data);
static void redo_pressed (GtkMandelApplication *app, gpointer data);
static void restart_thread (GtkMandelApplication *app);
static void rendering_started (GtkMandelApplication *app, gulong bits, gpointer data);
static void open_coord_file (GtkMandelApplication *app, gpointer data);
static void open_coord_dlg_response (GtkMandelApplication *app, gint response, gpointer data);
static void save_coord_file (GtkMandelApplication *app, gpointer data);
static void save_coord_dlg_response (GtkMandelApplication *app, gint response, gpointer data);
static void quit_selected (GtkMandelApplication *app, gpointer data);
static void update_area_info (GtkMandelApplication *app);
static void area_info_selected (GtkMandelApplication *app, gpointer data);
static void create_area_info_item (GtkMandelApplication *app, GtkWidget *table, struct area_info_item *item, int i, const char *label);
static void area_info_dlg_response (GtkMandelApplication *app, gpointer data);
static void set_entry_from_long (GtkEntry *entry, long value);
static void rendering_stopped (GtkMandelApplication *app, gboolean completed, gpointer data);
static void restart_pressed (GtkMandelApplication *app, gpointer data);
static void stop_pressed (GtkMandelApplication *app, gpointer data);
static void zoom_2exp (GtkMandelApplication *app, long exponent);
static void zoomed_in (GtkMandelApplication *app, gpointer data);
static void zoomed_out (GtkMandelApplication *app, gpointer data);
static void zpower_updated (GtkMandelApplication *app, gpointer data);


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
	create_dialogs (app);
	connect_signals (app);
	app->undo = NULL;
	app->redo = NULL;
	app->area = NULL;
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

	app->menu.open_coord_item = gtk_menu_item_new_with_label ("Open coordinate file...");
	gtk_menu_shell_append (GTK_MENU_SHELL (app->menu.file_menu), app->menu.open_coord_item);

	app->menu.save_coord_item = gtk_menu_item_new_with_label ("Save coordinate file...");
	gtk_menu_shell_append (GTK_MENU_SHELL (app->menu.file_menu), app->menu.save_coord_item);

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
	app->mainwin.undo = GTK_WIDGET (gtk_tool_button_new_from_stock (GTK_STOCK_GO_BACK));
	gtk_widget_set_sensitive (app->mainwin.undo, FALSE);

	app->mainwin.redo = GTK_WIDGET (gtk_tool_button_new_from_stock (GTK_STOCK_GO_FORWARD));
	gtk_widget_set_sensitive (app->mainwin.redo, FALSE);

	app->mainwin.toolbar_sep1 = GTK_WIDGET (gtk_separator_tool_item_new ());

	app->mainwin.restart = GTK_WIDGET (gtk_tool_button_new_from_stock (GTK_STOCK_REFRESH));

	app->mainwin.stop = GTK_WIDGET (gtk_tool_button_new_from_stock (GTK_STOCK_STOP));
	gtk_widget_set_sensitive (app->mainwin.stop, FALSE);

	app->mainwin.toolbar_sep2 = GTK_WIDGET (gtk_separator_tool_item_new ());

	app->mainwin.zoom_in = GTK_WIDGET (gtk_tool_button_new_from_stock (GTK_STOCK_ZOOM_IN));

	app->mainwin.zoom_out = GTK_WIDGET (gtk_tool_button_new_from_stock (GTK_STOCK_ZOOM_OUT));

	app->mainwin.tool_bar = gtk_toolbar_new ();
	gtk_toolbar_set_style (GTK_TOOLBAR (app->mainwin.tool_bar), GTK_TOOLBAR_ICONS);
	gtk_container_add (GTK_CONTAINER (app->mainwin.tool_bar), app->mainwin.undo);
	gtk_container_add (GTK_CONTAINER (app->mainwin.tool_bar), app->mainwin.redo);
	gtk_container_add (GTK_CONTAINER (app->mainwin.tool_bar), app->mainwin.toolbar_sep1);
	gtk_container_add (GTK_CONTAINER (app->mainwin.tool_bar), app->mainwin.restart);
	gtk_container_add (GTK_CONTAINER (app->mainwin.tool_bar), app->mainwin.stop);
	gtk_container_add (GTK_CONTAINER (app->mainwin.tool_bar), app->mainwin.toolbar_sep2);
	gtk_container_add (GTK_CONTAINER (app->mainwin.tool_bar), app->mainwin.zoom_in);
	gtk_container_add (GTK_CONTAINER (app->mainwin.tool_bar), app->mainwin.zoom_out);

	app->mainwin.maxiter_label = gtk_label_new ("Max Iterations");
	gtk_misc_set_alignment (GTK_MISC (app->mainwin.maxiter_label), 0.0, 0.5);

	app->mainwin.maxiter_input = gtk_entry_new ();
	gtk_entry_set_alignment (GTK_ENTRY (app->mainwin.maxiter_input), 1.0);
	gtk_entry_set_width_chars (GTK_ENTRY (app->mainwin.maxiter_input), 10);

	app->mainwin.log_colors_checkbox = gtk_check_button_new_with_label ("Logarithmic Colors");

	app->mainwin.log_colors_input = gtk_entry_new ();
	gtk_entry_set_alignment (GTK_ENTRY (app->mainwin.log_colors_input), 1.0);
	gtk_entry_set_width_chars (GTK_ENTRY (app->mainwin.log_colors_input), 10);
	gtk_entry_set_text (GTK_ENTRY (app->mainwin.log_colors_input), "100"); /* FIXME get default value in a sensible way */
	gtk_widget_set_sensitive (app->mainwin.log_colors_input, FALSE);

	app->mainwin.zpower_label = gtk_label_new ("Power of Z");
	gtk_misc_set_alignment (GTK_MISC (app->mainwin.zpower_label), 0.0, 0.5);

	app->mainwin.zpower_input = gtk_spin_button_new_with_range (2.0, 1000000.0, 1.0);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (app->mainwin.zpower_input), TRUE);
	gtk_entry_set_alignment (GTK_ENTRY (app->mainwin.zpower_input), 1.0);
	gtk_entry_set_width_chars (GTK_ENTRY (app->mainwin.zpower_input), 5);

	app->mainwin.controls_table = gtk_table_new (2, 3, FALSE);
	gtk_table_set_homogeneous (GTK_TABLE (app->mainwin.controls_table), FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (app->mainwin.controls_table), 2);
	gtk_table_set_col_spacings (GTK_TABLE (app->mainwin.controls_table), 2);

	gtk_table_attach (GTK_TABLE (app->mainwin.controls_table), app->mainwin.maxiter_label, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (app->mainwin.controls_table), app->mainwin.maxiter_input, 1, 2, 0, 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (app->mainwin.controls_table), app->mainwin.log_colors_checkbox, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (app->mainwin.controls_table), app->mainwin.log_colors_input, 1, 2, 1, 2, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (app->mainwin.controls_table), app->mainwin.zpower_label, 0, 1, 2, 3, GTK_FILL, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (app->mainwin.controls_table), app->mainwin.zpower_input, 1, 2, 2, 3, GTK_FILL | GTK_EXPAND, 0, 0, 0);

	app->mainwin.mandel = gtk_mandel_new ();
	gtk_widget_set_size_request (app->mainwin.mandel, 50, 50);
	/* FIXME how to set initial widget size? */

	app->mainwin.status_info = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (app->mainwin.status_info), 0.0, 0.5);

	app->mainwin.status_info_frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (app->mainwin.status_info_frame), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (app->mainwin.status_info_frame), app->mainwin.status_info);

	app->mainwin.math_info = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (app->mainwin.math_info), 0.0, 0.5);

	app->mainwin.math_info_frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (app->mainwin.math_info_frame), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (app->mainwin.math_info_frame), app->mainwin.math_info);

	app->mainwin.status_hbox = gtk_hbox_new (false, 2);
	gtk_box_pack_start (GTK_BOX (app->mainwin.status_hbox), app->mainwin.status_info_frame, TRUE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (app->mainwin.status_hbox), app->mainwin.math_info_frame, FALSE, FALSE, 0);

	app->mainwin.main_vbox = gtk_vbox_new (false, 2);
	gtk_box_pack_start (GTK_BOX (app->mainwin.main_vbox), app->menu.bar, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (app->mainwin.main_vbox), app->mainwin.tool_bar, FALSE, FALSE, 0);
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

	create_area_info (app);
}


static void
create_area_info (GtkMandelApplication *app)
{
	app->area_info.center.table = gtk_table_new (2, 4, false);
	gtk_table_set_homogeneous (GTK_TABLE (app->area_info.center.table), FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (app->area_info.center.table), 2);
	gtk_table_set_col_spacings (GTK_TABLE (app->area_info.center.table), 2);
	gtk_container_set_border_width (GTK_CONTAINER (app->area_info.center.table), 2);
	create_area_info_item (app, app->area_info.center.table, app->area_info.center.items + 0, 0, "cx");
	create_area_info_item (app, app->area_info.center.table, app->area_info.center.items + 1, 1, "cy");
	create_area_info_item (app, app->area_info.center.table, app->area_info.center.items + 2, 2, "magf");

	app->area_info.corners.table = gtk_table_new (2, 4, false);
	gtk_table_set_homogeneous (GTK_TABLE (app->area_info.corners.table), FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (app->area_info.corners.table), 2);
	gtk_table_set_col_spacings (GTK_TABLE (app->area_info.corners.table), 2);
	gtk_container_set_border_width (GTK_CONTAINER (app->area_info.corners.table), 2);
	create_area_info_item (app, app->area_info.corners.table, app->area_info.corners.items + 0, 0, "xmin");
	create_area_info_item (app, app->area_info.corners.table, app->area_info.corners.items + 1, 1, "xmax");
	create_area_info_item (app, app->area_info.corners.table, app->area_info.corners.items + 2, 2, "ymin");
	create_area_info_item (app, app->area_info.corners.table, app->area_info.corners.items + 3, 3, "ymax");

	app->area_info.center_label = gtk_label_new ("Center");
	app->area_info.corners_label = gtk_label_new ("Corners");

	app->area_info.notebook = gtk_notebook_new ();
	gtk_notebook_append_page (GTK_NOTEBOOK (app->area_info.notebook), app->area_info.center.table, app->area_info.center_label);
	gtk_notebook_append_page (GTK_NOTEBOOK (app->area_info.notebook), app->area_info.corners.table, app->area_info.corners_label);

	app->area_info.dialog = gtk_dialog_new_with_buttons ("Area Info", GTK_WINDOW (app->mainwin.win), GTK_DIALOG_DESTROY_WITH_PARENT, GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);
	gtk_dialog_set_has_separator (GTK_DIALOG (app->area_info.dialog), FALSE);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (app->area_info.dialog)->vbox), app->area_info.notebook, FALSE, FALSE, 0);

	GdkGeometry geom;
	geom.max_width = 1000000; /* FIXME how to set max_width = unlimited? */
	geom.max_height = -1;
	gtk_window_set_geometry_hints (GTK_WINDOW (app->area_info.dialog), NULL, &geom, GDK_HINT_MAX_SIZE);
}


static void
create_area_info_item (GtkMandelApplication *app, GtkWidget *table, struct area_info_item *item, int i, const char *label)
{
	item->label = gtk_label_new (label);
	gtk_misc_set_alignment (GTK_MISC (item->label), 0.0, 0.5);
	item->buffer = gtk_text_buffer_new (NULL);
	item->view = gtk_text_view_new_with_buffer (item->buffer);
	gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (item->view), GTK_WRAP_CHAR);
	gtk_text_view_set_editable (GTK_TEXT_VIEW (item->view), FALSE);
	gtk_table_attach (GTK_TABLE (table), item->label, 0, 1, i, i + 1, GTK_FILL, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (table), item->view, 1, 2, i, i + 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
}


void
gtk_mandel_application_start (GtkMandelApplication *app)
{
	gtk_widget_show_all (app->mainwin.win);
	restart_thread (app);
}


GtkMandelApplication *
gtk_mandel_application_new (GtkMandelArea *area, unsigned maxiter, render_method_t render_method, double log_factor, unsigned zpower)
{
	GtkMandelApplication *app = g_object_new (gtk_mandel_application_get_type (), NULL);
	gtk_mandel_application_set_area (app, area);
	gtk_mandel_application_set_maxiter (app, maxiter);
	gtk_mandel_application_set_zpower (app, zpower);
	app->render_method = render_method;
	app->log_factor = log_factor;
	return app;
}


static void
connect_signals (GtkMandelApplication *app)
{
	int i;

	g_signal_connect_swapped (G_OBJECT (app->mainwin.mandel), "selection", (GCallback) area_selected, app);
	g_signal_connect_swapped (G_OBJECT (app->mainwin.mandel), "rendering-started", (GCallback) rendering_started, app);
	g_signal_connect_swapped (G_OBJECT (app->mainwin.mandel), "rendering-stopped", (GCallback) rendering_stopped, app);

	g_signal_connect_swapped (G_OBJECT (app->mainwin.maxiter_input), "activate", (GCallback) maxiter_updated, app);

	g_signal_connect_swapped (G_OBJECT (app->menu.area_info_item), "activate", (GCallback) area_info_selected, app);

	g_signal_connect_swapped (G_OBJECT (app->menu.open_coord_item), "activate", (GCallback) open_coord_file, app);

	g_signal_connect_swapped (G_OBJECT (app->menu.save_coord_item), "activate", (GCallback) save_coord_file, app);

	for (i = 0; i < RM_MAX; i++)
		g_signal_connect_swapped (G_OBJECT (app->menu.render_method_items[i]), "toggled", (GCallback) render_method_updated, app);

	g_signal_connect_swapped (G_OBJECT (app->menu.quit_item), "activate", (GCallback) quit_selected, app);

	g_signal_connect_swapped (G_OBJECT (app->mainwin.log_colors_checkbox), "toggled", (GCallback) log_colors_updated, app);

	g_signal_connect_swapped (G_OBJECT (app->mainwin.log_colors_input), "activate", (GCallback) log_colors_updated, app);

	g_signal_connect_swapped (G_OBJECT (app->mainwin.zpower_input), "value-changed", (GCallback) zpower_updated, app);

	g_signal_connect_swapped (G_OBJECT (app->mainwin.undo), "clicked", (GCallback) undo_pressed, app);

	g_signal_connect_swapped (G_OBJECT (app->mainwin.redo), "clicked", (GCallback) redo_pressed, app);

	g_signal_connect_swapped (G_OBJECT (app->mainwin.zoom_in), "clicked", (GCallback) zoomed_in, app);

	g_signal_connect_swapped (G_OBJECT (app->mainwin.zoom_out), "clicked", (GCallback) zoomed_out, app);

	g_signal_connect_swapped (G_OBJECT (app->mainwin.restart), "clicked", (GCallback) restart_pressed, app);

	g_signal_connect_swapped (G_OBJECT (app->mainwin.stop), "clicked", (GCallback) stop_pressed, app);

	/*
	 * This prevents the window from being destroyed
	 * when the close button is clicked.
	 */
	g_signal_connect (G_OBJECT (app->open_coord_chooser), "delete-event", (GCallback) gtk_widget_hide_on_delete, NULL);
	g_signal_connect_swapped (G_OBJECT (app->open_coord_chooser), "response", (GCallback) open_coord_dlg_response, app);

	g_signal_connect (G_OBJECT (app->save_coord_chooser), "delete-event", (GCallback) gtk_widget_hide_on_delete, NULL);
	g_signal_connect_swapped (G_OBJECT (app->save_coord_chooser), "response", (GCallback) save_coord_dlg_response, app);

	g_signal_connect_swapped (G_OBJECT (app->mainwin.win), "delete-event", (GCallback) quit_selected, app);

	g_signal_connect (G_OBJECT (app->area_info.dialog), "delete-event", (GCallback) gtk_widget_hide_on_delete, NULL);
	g_signal_connect_swapped (G_OBJECT (app->area_info.dialog), "response", (GCallback) area_info_dlg_response, app);
}


static void
area_selected (GtkMandelApplication *app, GtkMandelArea *area, gpointer data)
{
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


static void
render_method_updated (GtkMandelApplication *app, gpointer data)
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
	update_area_info (app);
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
	update_area_info (app);
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
	update_area_info (app);
}


static void
restart_thread (GtkMandelApplication *app)
{
	GtkMandel *mandel = GTK_MANDEL (app->mainwin.mandel);
	GtkMandelArea *area = app->area;
	struct mandeldata *md = malloc (sizeof (*md));
	mandeldata_init (md);
	mpf_set (md->cx, area->cx);
	mpf_set (md->cy, area->cy);
	mpf_set (md->magf, area->magf);
	md->type = FRACTAL_MANDELBROT;
	md->maxiter = app->maxiter;
	md->render_method = app->render_method;
	md->log_factor = app->log_factor;
	md->zpower = app->zpower;
	md->thread_count = thread_count;
	gtk_mandel_restart_thread (mandel, md);
}


static void
rendering_started (GtkMandelApplication *app, gulong bits, gpointer data)
{
	char buf[64];
	int r;

	gtk_label_set_text (GTK_LABEL (app->mainwin.status_info), "Rendering");
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
	gtk_widget_show_all (app->open_coord_chooser);
}


static void
open_coord_dlg_response (GtkMandelApplication *app, gint response, gpointer data)
{
	gtk_widget_hide (app->open_coord_chooser);

	if (response != GTK_RESPONSE_ACCEPT)
		return;

	const char *filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (app->open_coord_chooser));
	GtkMandelArea *area = gtk_mandel_area_new_from_file (filename);
	if (area != NULL) {
		gtk_mandel_application_set_area (app, area);
		g_object_unref (G_OBJECT (area));
		restart_thread (app);
	} else
		fprintf (stderr, "%s: Something went wrong reading the file.\n", filename);
}


static void
quit_selected (GtkMandelApplication *app, gpointer data)
{
	gtk_main_quit ();
}


static void
update_area_info (GtkMandelApplication *app)
{
	char b0[1024], b1[1024], b2[1024];
	mpf_t xmin, xmax, ymin, ymax;

	if (center_coords_to_string (app->area->cx, app->area->cy, app->area->magf, b0, b1, b2, 1024) >= 0) {
		gtk_text_buffer_set_text (app->area_info.center.items[0].buffer, b0, strlen (b0));
		gtk_text_buffer_set_text (app->area_info.center.items[1].buffer, b1, strlen (b1));
		gtk_text_buffer_set_text (app->area_info.center.items[2].buffer, b2, strlen (b2));
	}

	mpf_init (xmin);
	mpf_init (xmax);
	mpf_init (ymin);
	mpf_init (ymax);

	center_to_corners (xmin, xmax, ymin, ymax, app->area->cx, app->area->cy, app->area->magf, (double) app->mainwin.mandel->allocation.width / app->mainwin.mandel->allocation.height);

	if (coord_pair_to_string (xmin, xmax, b0, b1, 1024) >= 0) {
		gtk_text_buffer_set_text (app->area_info.corners.items[0].buffer, b0, strlen (b0));
		gtk_text_buffer_set_text (app->area_info.corners.items[1].buffer, b1, strlen (b1));
	}
	if (coord_pair_to_string (ymin, ymax, b0, b1, 1024) >= 0) {
		gtk_text_buffer_set_text (app->area_info.corners.items[2].buffer, b0, strlen (b0));
		gtk_text_buffer_set_text (app->area_info.corners.items[3].buffer, b1, strlen (b1));
	}

	mpf_clear (xmin);
	mpf_clear (xmax);
	mpf_clear (ymin);
	mpf_clear (ymax);
}


static void
area_info_dlg_response (GtkMandelApplication *app, gpointer data)
{
	gtk_widget_hide (app->area_info.dialog);
}


static void
area_info_selected (GtkMandelApplication *app, gpointer data)
{
	gtk_widget_show_all (app->area_info.dialog);
}


static void
save_coord_file (GtkMandelApplication *app, gpointer data)
{
	gtk_widget_show_all (app->save_coord_chooser);
}


static void
save_coord_dlg_response (GtkMandelApplication *app, gint response, gpointer data)
{
	gtk_widget_hide (app->save_coord_chooser);

	if (response != GTK_RESPONSE_ACCEPT)
		return;

	/* FIXME check for existing file */

	const char *filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (app->save_coord_chooser));
	FILE *f = fopen (filename, "w");
	if (f == NULL) {
		fprintf (stderr, "%s: %s\n", filename, strerror (errno));
		return;
	}

	fwrite_center_coords (f, app->area->cx, app->area->cy, app->area->magf);
	fclose (f);
}


void
gtk_mandel_application_set_maxiter (GtkMandelApplication *app, unsigned long maxiter)
{
	app->maxiter = maxiter;
	set_entry_from_long (GTK_ENTRY (app->mainwin.maxiter_input), app->maxiter);
}


void
gtk_mandel_application_set_zpower (GtkMandelApplication *app, unsigned zpower)
{
	app->zpower = zpower;
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (app->mainwin.zpower_input), app->zpower);
}


static void
set_entry_from_long (GtkEntry *entry, long value)
{
	char buf[64];
	int r;
	r = snprintf (buf, sizeof (buf), "%ld", value);
	if (r < 0 || r >= sizeof (buf))
		return;
	gtk_entry_set_text (entry, buf);
}


static void
rendering_stopped (GtkMandelApplication *app, gboolean completed, gpointer data)
{
	const char *msg;
	if (completed)
		msg = "Finished";
	else
		msg = "Stopped";

	gtk_widget_set_sensitive (app->mainwin.stop, FALSE);
	gtk_label_set_text (GTK_LABEL (app->mainwin.status_info), msg);
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
	gtk_label_set_text (GTK_LABEL (app->mainwin.status_info), "Stopping...");
	mandel->md->terminate = TRUE;
}


static void
zoom_2exp (GtkMandelApplication *app, long exponent)
{
	GtkMandel *mandel = GTK_MANDEL (app->mainwin.mandel);
	mpf_t magf;
	mpf_init (magf);
	if (exponent >= 0)
		mpf_mul_2exp (magf, mandel->md->magf, exponent);
	else
		mpf_div_2exp (magf, mandel->md->magf, -exponent);
	GtkMandelArea *area = gtk_mandel_area_new (mandel->md->cx, mandel->md->cy, magf);
	mpf_clear (magf);
	gtk_mandel_application_set_area (app, area);
	restart_thread (app);
}


static void
zoomed_in (GtkMandelApplication *app, gpointer data)
{
	zoom_2exp (app, 1);
}


static void
zoomed_out (GtkMandelApplication *app, gpointer data)
{
	zoom_2exp (app, -1);
}


static void
zpower_updated (GtkMandelApplication *app, gpointer data)
{
	app->zpower = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (app->mainwin.zpower_input));
	restart_thread (app);
}
