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

#define GTK_MANDEL_APPLICATION_GET_CLASS(app) G_TYPE_INSTANCE_GET_CLASS ((app), GtkMandelApplication, GtkMandelApplicationClass)

static void gtk_mandel_application_class_init (GtkMandelApplicationClass *class);
static void gtk_mandel_application_init (GtkMandelApplication *app);
static void create_menus (GtkMandelApplication *app);
static void create_mainwin (GtkMandelApplication *app);
static void create_dialogs (GtkMandelApplication *app);
static void create_area_info (GtkMandelApplication *app);
static void create_mandelbrot_param (struct mandelbrot_param *mp);
static void create_julia_param (struct julia_param *jp);
static void create_type_dlg (struct fractal_type_dlg *dlg, GtkWindow *window);
static void connect_signals (GtkMandelApplication *app);
static void area_selected (GtkMandelApplication *app, struct mandel_area *area, gpointer data);
static void point_for_julia_selected (GtkMandelApplication *app, struct mandel_point *point, gpointer data);
static void maxiter_updated (GtkMandelApplication *app, gpointer data);
static void render_method_updated (GtkMandelApplication *app, gpointer data);
static void log_colors_updated (GtkMandelApplication *app, gpointer data);
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
static void update_area_info (GtkMandelApplication *app);
static void update_gui_from_mandeldata (GtkMandelApplication *app);
static void area_info_selected (GtkMandelApplication *app, gpointer data);
static void create_area_info_item (GtkMandelApplication *app, GtkWidget *table, struct area_info_item *item, int i, const char *label);
static void area_info_dlg_response (GtkMandelApplication *app, gpointer data);
static void set_entry_from_long (GtkEntry *entry, long value);
static void set_entry_from_double (GtkEntry *entry, double value, int prec);
static void rendering_stopped (GtkMandelApplication *app, gboolean completed, gpointer data);
static void restart_pressed (GtkMandelApplication *app, gpointer data);
static void stop_pressed (GtkMandelApplication *app, gpointer data);
static void zoom_2exp (GtkMandelApplication *app, long exponent);
static void zoomed_out (GtkMandelApplication *app, gpointer data);
static void zpower_updated (GtkMandelApplication *app, gpointer data);
static void threads_updated (GtkMandelApplication *app, gpointer data);
static void update_mandeldata (GtkMandelApplication *app, struct mandeldata *md);
static void gtk_mandel_application_set_area (GtkMandelApplication *app, struct mandel_area *area);
static void zoom_mode_selected (GtkMandelApplication *app, gpointer data);
static void to_julia_mode_selected (GtkMandelApplication *app, gpointer data);
static void type_dlg_type_updated (GtkComboBox *combo, struct fractal_type_dlg *dlg);


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
	create_type_dlg (&app->fractal_type_dlg, GTK_WINDOW (app->mainwin.win));
	//gtk_widget_show_all (app->fractal_type_dlg.dialog); /* XXX for testing only */
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

	app->mainwin.toolbar1_sep1 = GTK_WIDGET (gtk_separator_tool_item_new ());

	app->mainwin.restart = GTK_WIDGET (gtk_tool_button_new_from_stock (GTK_STOCK_REFRESH));

	app->mainwin.stop = GTK_WIDGET (gtk_tool_button_new_from_stock (GTK_STOCK_STOP));
	gtk_widget_set_sensitive (app->mainwin.stop, FALSE);

	app->mainwin.toolbar1_sep2 = GTK_WIDGET (gtk_separator_tool_item_new ());

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

	app->mainwin.threads_label = gtk_label_new ("Threads");
	gtk_misc_set_alignment (GTK_MISC (app->mainwin.threads_label), 0.0, 0.5);

	app->mainwin.threads_input = gtk_spin_button_new_with_range (1.0, 1024.0, 1.0);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (app->mainwin.threads_input), TRUE);
	gtk_entry_set_alignment (GTK_ENTRY (app->mainwin.threads_input), 1.0);
	gtk_entry_set_width_chars (GTK_ENTRY (app->mainwin.threads_input), 5);

	app->mainwin.controls_table = gtk_table_new (2, 4, FALSE);
	gtk_table_set_homogeneous (GTK_TABLE (app->mainwin.controls_table), FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (app->mainwin.controls_table), 2);
	gtk_table_set_col_spacings (GTK_TABLE (app->mainwin.controls_table), 2);

	gtk_table_attach (GTK_TABLE (app->mainwin.controls_table), app->mainwin.maxiter_label, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (app->mainwin.controls_table), app->mainwin.maxiter_input, 1, 2, 0, 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (app->mainwin.controls_table), app->mainwin.log_colors_checkbox, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (app->mainwin.controls_table), app->mainwin.log_colors_input, 1, 2, 1, 2, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (app->mainwin.controls_table), app->mainwin.zpower_label, 0, 1, 2, 3, GTK_FILL, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (app->mainwin.controls_table), app->mainwin.zpower_input, 1, 2, 2, 3, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (app->mainwin.controls_table), app->mainwin.threads_label, 0, 1, 3, 4, GTK_FILL, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (app->mainwin.controls_table), app->mainwin.threads_input, 1, 2, 3, 4, GTK_FILL | GTK_EXPAND, 0, 0, 0);

	app->mainwin.mandel = gtk_mandel_new ();
	gtk_mandel_set_selection_type (GTK_MANDEL (app->mainwin.mandel), GTK_MANDEL_SELECT_AREA);
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


static void
create_mandelbrot_param (struct mandelbrot_param *par)
{
	par->zpower_label = gtk_label_new ("Power of Z");
	par->zpower_input = gtk_spin_button_new_with_range (2.0, 100.0, 1.0);
	par->distance_est = gtk_check_button_new_with_label ("Distance Estimation");
	par->table = gtk_table_new (2, 2, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (par->table), 2);
	gtk_table_set_col_spacings (GTK_TABLE (par->table), 2);
	gtk_table_attach (GTK_TABLE (par->table), par->zpower_label, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (par->table), par->zpower_input, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (par->table), par->distance_est, 0, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 0, 0);
}


static void
create_julia_param (struct julia_param *par)
{
	par->zpower_label = gtk_label_new ("Power of Z");
	par->zpower_input = gtk_spin_button_new_with_range (2.0, 100.0, 1.0);
	par->preal_label = gtk_label_new ("preal");
	par->preal_input = gtk_entry_new ();
	par->pimag_label = gtk_label_new ("pimag");
	par->pimag_input = gtk_entry_new ();
	par->table = gtk_table_new (2, 3, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (par->table), 2);
	gtk_table_set_col_spacings (GTK_TABLE (par->table), 2);
	gtk_table_attach (GTK_TABLE (par->table), par->zpower_label, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (par->table), par->zpower_input, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (par->table), par->preal_label, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (par->table), par->preal_input, 1, 2, 1, 2, GTK_FILL, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (par->table), par->pimag_label, 0, 1, 2, 3, GTK_FILL, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (par->table), par->pimag_input, 1, 2, 2, 3, GTK_FILL, 0, 0, 0);
}


static void
create_type_dlg (struct fractal_type_dlg *dlg, GtkWindow *window)
{
	dlg->type_label = gtk_label_new ("Fractal Type");
	dlg->type_list = gtk_list_store_new (2, G_TYPE_INT, G_TYPE_STRING);
	GtkTreeIter iter[1];
	gtk_list_store_append (dlg->type_list, iter);
	gtk_list_store_set (dlg->type_list, iter, 0, 0, -1);
	gtk_list_store_set (dlg->type_list, iter, 1, "Mandelbrot", -1);
	gtk_list_store_append (dlg->type_list, iter);
	gtk_list_store_set (dlg->type_list, iter, 0, 1, -1);
	gtk_list_store_set (dlg->type_list, iter, 1, "Julia", -1);
	dlg->type_renderer = gtk_cell_renderer_text_new ();
	dlg->type_input = gtk_combo_box_new_with_model (GTK_TREE_MODEL (dlg->type_list));
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (dlg->type_input), dlg->type_renderer, FALSE);
	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (dlg->type_input), dlg->type_renderer, "text", 1);
	gtk_combo_box_set_active (GTK_COMBO_BOX (dlg->type_input), 0); /* XXX */
	dlg->type_hbox = gtk_hbox_new (FALSE, 2);
	gtk_box_pack_start (GTK_BOX (dlg->type_hbox), dlg->type_label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (dlg->type_hbox), dlg->type_input, FALSE, FALSE, 0);

	dlg->area_creal_label = gtk_label_new ("Center Real");
	dlg->area_creal_input = gtk_entry_new ();
	dlg->area_cimag_label = gtk_label_new ("Center Imag");
	dlg->area_cimag_input = gtk_entry_new ();
	dlg->area_magf_label = gtk_label_new ("Magnification");
	dlg->area_magf_input = gtk_entry_new ();

	dlg->area_table = gtk_table_new (2, 3, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (dlg->area_table), 2);
	gtk_table_set_col_spacings (GTK_TABLE (dlg->area_table), 2);
	gtk_table_attach (GTK_TABLE (dlg->area_table), dlg->area_creal_label, 0, 1, 0, 1, 0, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (dlg->area_table), dlg->area_creal_input, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (dlg->area_table), dlg->area_cimag_label, 0, 1, 1, 2, 0, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (dlg->area_table), dlg->area_cimag_input, 1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (dlg->area_table), dlg->area_magf_label, 0, 1, 2, 3, 0, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (dlg->area_table), dlg->area_magf_input, 1, 2, 2, 3, GTK_EXPAND | GTK_FILL, 0, 0, 0);

	dlg->area_frame = gtk_frame_new ("Area");
	gtk_container_add (GTK_CONTAINER (dlg->area_frame), dlg->area_table);

	create_mandelbrot_param (&dlg->mandelbrot_param);
	create_julia_param (&dlg->julia_param);
	dlg->type_param_notebook = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (dlg->type_param_notebook), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (dlg->type_param_notebook), FALSE);
	gtk_notebook_append_page (GTK_NOTEBOOK (dlg->type_param_notebook), dlg->mandelbrot_param.table, NULL);
	gtk_notebook_append_page (GTK_NOTEBOOK (dlg->type_param_notebook), dlg->julia_param.table, NULL);
	dlg->type_param_frame = gtk_frame_new ("Type-Specific Params");
	gtk_container_add (GTK_CONTAINER (dlg->type_param_frame), dlg->type_param_notebook);

	dlg->dialog = gtk_dialog_new_with_buttons ("Fractal Type", window, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, GTK_STOCK_CANCEL, GTK_RESPONSE_CLOSE, NULL);
	GtkBox *dlg_vbox = GTK_BOX (GTK_DIALOG (dlg->dialog)->vbox);
	gtk_box_pack_start (dlg_vbox, dlg->type_hbox, FALSE, FALSE, 0);
	gtk_box_pack_start (dlg_vbox, dlg->area_frame, FALSE, FALSE, 0);
	gtk_box_pack_start (dlg_vbox, dlg->type_param_frame, FALSE, FALSE, 0);

	g_signal_connect (G_OBJECT (dlg->type_input), "changed", (GCallback) type_dlg_type_updated, (gpointer) dlg);
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

	g_signal_connect_swapped (G_OBJECT (app->mainwin.threads_input), "value-changed", (GCallback) threads_updated, app);

	g_signal_connect_swapped (G_OBJECT (app->mainwin.undo), "clicked", (GCallback) undo_pressed, app);

	g_signal_connect_swapped (G_OBJECT (app->mainwin.redo), "clicked", (GCallback) redo_pressed, app);

	g_signal_connect_swapped (G_OBJECT (app->mainwin.restart), "clicked", (GCallback) restart_pressed, app);

	g_signal_connect_swapped (G_OBJECT (app->mainwin.stop), "clicked", (GCallback) stop_pressed, app);

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

	g_signal_connect (G_OBJECT (app->area_info.dialog), "delete-event", (GCallback) gtk_widget_hide_on_delete, NULL);
	g_signal_connect_swapped (G_OBJECT (app->area_info.dialog), "response", (GCallback) area_info_dlg_response, app);
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
	mandeldata_init (md);
	md->type = FRACTAL_JULIA;
	md->zpower = app->md->zpower;
	mpf_set (md->param.real, point->real);
	mpf_set (md->param.imag, point->imag);
	/* XXX get default params in a sensible way */
	mpf_set_str (md->area.center.real, "0", 10);
	mpf_set_str (md->area.center.imag, "0", 10);
	mpf_set_str (md->area.magf, ".5", 10);
	md->maxiter = 1000;
	md->log_factor = 0.0;
	gtk_mandel_application_set_mandeldata (app, md);
	restart_thread (app);
}


static void
maxiter_updated (GtkMandelApplication *app, gpointer data)
{
	if (app->updating_gui)
		return;
	int i = atoi (gtk_entry_get_text (GTK_ENTRY (app->mainwin.maxiter_input)));
	struct mandeldata *md = malloc (sizeof (*md));
	mandeldata_clone (md, app->md);
	md->maxiter = i;
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


static void
log_colors_updated (GtkMandelApplication *app, gpointer data)
{
	if (app->updating_gui)
		return;
	gboolean active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (app->mainwin.log_colors_checkbox));
	gtk_widget_set_sensitive (app->mainwin.log_colors_input, active);
	double lf = 0.0;
	if (active)
		lf = strtod (gtk_entry_get_text (GTK_ENTRY (app->mainwin.log_colors_input)), NULL);
	if (isfinite (lf)) {
		struct mandeldata *md = malloc (sizeof (*md));
		mandeldata_clone (md, app->md);
		md->log_factor = lf;
		gtk_mandel_application_set_mandeldata (app, md);
		restart_thread (app);
	}
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


/* XXX */
static void
open_coord_dlg_response (GtkMandelApplication *app, gint response, gpointer data)
{
	gtk_widget_hide (app->open_coord_chooser);

	if (response != GTK_RESPONSE_ACCEPT)
		return;

	const char *filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (app->open_coord_chooser));
	FILE *f = fopen (filename, "r");
	if (f == NULL) {
		/* XXX show dialog box */
		fprintf (stderr, "%s: fopen: %s\n", filename, strerror (errno));
		return;
	}
	struct mandeldata *md = malloc (sizeof (*md));
	mandeldata_init (md);
	bool ok = fread_mandeldata (f, md);
	fclose (f);
	if (!ok) {
		fprintf (stderr, "%s: Something went wrong reading the file.\n", filename);
		mandeldata_clear (md);
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
update_area_info (GtkMandelApplication *app)
{
	char b0[1024], b1[1024], b2[1024];
	mpf_t xmin, xmax, ymin, ymax;

	if (center_coords_to_string (app->md->area.center.real, app->md->area.center.imag, app->md->area.magf, b0, b1, b2, 1024) >= 0) {
		gtk_text_buffer_set_text (app->area_info.center.items[0].buffer, b0, strlen (b0));
		gtk_text_buffer_set_text (app->area_info.center.items[1].buffer, b1, strlen (b1));
		gtk_text_buffer_set_text (app->area_info.center.items[2].buffer, b2, strlen (b2));
	}

	mpf_init (xmin);
	mpf_init (xmax);
	mpf_init (ymin);
	mpf_init (ymax);

	center_to_corners (xmin, xmax, ymin, ymax, app->md->area.center.real, app->md->area.center.imag, app->md->area.magf, GTK_MANDEL (app->mainwin.mandel)->aspect);

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
update_gui_from_mandeldata (GtkMandelApplication *app)
{
	app->updating_gui = true;
	set_entry_from_long (GTK_ENTRY (app->mainwin.maxiter_input), app->md->maxiter);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (app->mainwin.zpower_input), app->md->zpower);
	bool use_log_factor = app->md->log_factor != 0.0;
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (app->mainwin.log_colors_checkbox), use_log_factor);
	gtk_widget_set_sensitive (app->mainwin.log_colors_input, use_log_factor);
	if (use_log_factor)
		set_entry_from_double (GTK_ENTRY (app->mainwin.log_colors_input), app->md->log_factor, 1);
	gtk_widget_set_sensitive (app->mainwin.to_julia_mode, app->md->type == FRACTAL_MANDELBROT);
	gtk_toggle_tool_button_set_active (GTK_TOGGLE_TOOL_BUTTON (app->mainwin.zoom_mode), TRUE);
	update_area_info (app);
	app->updating_gui = false;
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

	fwrite_mandeldata (f, app->md);
	fclose (f);
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
set_entry_from_double (GtkEntry *entry, double value, int prec)
{
	char buf[64];
	int r;
	r = snprintf (buf, sizeof (buf), "%.*f", prec, value);
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
zpower_updated (GtkMandelApplication *app, gpointer data)
{
	if (app->updating_gui)
		return;
	printf ("* zpower updated!\n");
	struct mandeldata *md = malloc (sizeof (*md));
	mandeldata_clone (md, app->md);
	md->zpower = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (app->mainwin.zpower_input));
	gtk_mandel_application_set_mandeldata (app, md);
	restart_thread (app);
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
type_dlg_type_updated (GtkComboBox *combo, struct fractal_type_dlg *dlg)
{
	GtkTreeIter iter[1];
	gint gi;
	gtk_combo_box_get_active_iter (combo, iter);
	gtk_tree_model_get (GTK_TREE_MODEL (dlg->type_list), iter, 0, &gi, -1);
	//gtk_tree_iter_free (iter);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (dlg->type_param_notebook), gi);
}


static void
rendering_progress (GtkMandelApplication *app, gdouble progress, gpointer data)
{
	char buf[256];
	int r = snprintf (buf, sizeof (buf), "Rendering (%.1f%%)", (double) progress * 100.0);
	if (r > 0 && r < sizeof (buf))
		gtk_label_set_text (GTK_LABEL (app->mainwin.status_info), buf);
}
