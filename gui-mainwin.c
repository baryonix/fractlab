#include <stdlib.h>

#include <glib.h>
#include <gtk/gtk.h>

#include "mandelbrot.h"
#include "gtkmandel.h"
#include "gui-util.h"
#include "gui-mainwin.h"


typedef enum {
	FRACTAL_MODE_ZOOM = 0,
	FRACTAL_MODE_TO_JULIA = 1,
	FRACTAL_MODE_MAX = 2
} FractalMainWindowMode;


struct mainwin_menu {
	GtkWidget *render_method_items[RM_MAX];
};


struct _FractalMainWindowPrivate {
	GtkWidget *undo_button, *redo_button, *stop;
	GtkWidget *zoom_mode, *to_julia_mode;
	GtkWidget *threads_input;
	GtkWidget *mandel;
	GtkWidget *status_info, *math_info;
	struct mainwin_menu menu;
	GSList *undo, *redo;
	FractalMainWindowMode mode;
};


static void fractal_main_window_class_init (gpointer g_class, gpointer data);
static void fractal_main_window_init (GTypeInstance *instance, gpointer g_class);

static GtkWidget *create_menus (FractalMainWindow *win);
static void restart_thread (FractalMainWindow *win);
static void area_selected (FractalMainWindow *win, struct mandel_area *area, gpointer data);
static void point_for_julia_selected (FractalMainWindow *win, struct mandel_point *point, gpointer data);
static void render_method_updated (FractalMainWindow *win, gpointer data);
static void undo_pressed (FractalMainWindow *win, gpointer data);
static void redo_pressed (FractalMainWindow *win, gpointer data);
static void rendering_started (FractalMainWindow *win, gulong bits, gpointer data);
static void rendering_progress (FractalMainWindow *win, gdouble progress, gpointer data);
static void rendering_stopped (FractalMainWindow *win, gboolean completed, gpointer data);
static void quit_selected (FractalMainWindow *win, gpointer data);
static void restart_pressed (FractalMainWindow *win, gpointer data);
static void stop_pressed (FractalMainWindow *win, gpointer data);
static void zoom_2exp (FractalMainWindow *win, long exponent);
static void zoomed_out (FractalMainWindow *win, gpointer data);
static void threads_updated (FractalMainWindow *win, gpointer data);
static void zoom_mode_selected (FractalMainWindow *win, gpointer data);
static void to_julia_mode_selected (FractalMainWindow *win, gpointer data);
static void fractal_main_window_set_area (FractalMainWindow *win, struct mandel_area *area);
static void update_mandeldata (FractalMainWindow *win, const struct mandeldata *md);
static void load_coords_requested (FractalMainWindow *win, gpointer data);
static void save_coords_requested (FractalMainWindow *win, gpointer data);
static void info_dlg_requested (FractalMainWindow *win, gpointer data);
static void type_dlg_requested (FractalMainWindow *win, gpointer data);
static void about_dlg_requested (FractalMainWindow *win, gpointer data);


GType
fractal_main_window_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (FractalMainWindowClass),
			NULL, NULL,
			fractal_main_window_class_init,
			NULL, NULL,
			sizeof (FractalMainWindow),
			0,
			fractal_main_window_init
		};
		type = g_type_register_static (GTK_TYPE_WINDOW, "FractalMainWindow", &info, 0);
	}

	return type;
}


static void
fractal_main_window_class_init (gpointer g_class_, gpointer data)
{
	FractalMainWindowClass *const g_class = FRACTAL_MAIN_WINDOW_CLASS (g_class_);

	render_method_t i;
	for (i = 0; i < RM_MAX; i++)
		g_class->render_methods[i] = i;

	g_class->mandeldata_updated_signal = g_signal_new (
		"mandeldata-updated",
		G_TYPE_FROM_CLASS (g_class),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE,
		0
	);

	g_class->load_coords_signal = g_signal_new (
		"load-coords-requested",
		G_TYPE_FROM_CLASS (g_class),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE,
		0
	);

	g_class->save_coords_signal = g_signal_new (
		"save-coords-requested",
		G_TYPE_FROM_CLASS (g_class),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE,
		0
	);

	g_class->info_dlg_signal = g_signal_new (
		"info-dialog-requested",
		G_TYPE_FROM_CLASS (g_class),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE,
		0
	);

	g_class->type_dlg_signal = g_signal_new (
		"type-dialog-requested",
		G_TYPE_FROM_CLASS (g_class),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE,
		0
	);

	g_class->about_dlg_signal = g_signal_new (
		"about-dialog-requested",
		G_TYPE_FROM_CLASS (g_class),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE,
		0
	);
}


static void
fractal_main_window_init (GTypeInstance *instance, gpointer g_class)
{
	FractalMainWindow *win = FRACTAL_MAIN_WINDOW (instance);

	win->priv = malloc (sizeof (*win->priv));
	FractalMainWindowPrivate *const priv = win->priv;

	priv->undo = NULL;
	priv->redo = NULL;
	win->md = NULL;

	gtk_window_set_title (GTK_WINDOW (win), "mandel-gtk");
	g_signal_connect_object (win, "delete-event", (GCallback) quit_selected, win, G_CONNECT_SWAPPED);

	GtkWidget *container, *container2, *widget;
	GtkWidget *main_vbox = gtk_vbox_new (false, 2);
	gtk_container_add (GTK_CONTAINER (win), main_vbox);

	gtk_box_pack_start (GTK_BOX (main_vbox), create_menus (win), FALSE, FALSE, 0);

	gint ww, wh;

	/*
	 * Toolbar 1
	 */
	container = gtk_toolbar_new ();
	gtk_box_pack_start (GTK_BOX (main_vbox), container, FALSE, FALSE, 0);
	gtk_toolbar_set_style (GTK_TOOLBAR (container), GTK_TOOLBAR_ICONS);
	//gtk_toolbar_set_show_arrow (GTK_TOOLBAR (priv->toolbar1), FALSE);

	widget = GTK_WIDGET (gtk_tool_button_new_from_stock (GTK_STOCK_GO_BACK));
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_set_sensitive (widget, FALSE);
	g_signal_connect_object (G_OBJECT (widget), "clicked", (GCallback) undo_pressed, win, G_CONNECT_SWAPPED);
	priv->undo_button = widget;
	g_object_ref (priv->undo_button);

	widget = GTK_WIDGET (gtk_tool_button_new_from_stock (GTK_STOCK_GO_FORWARD));
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_set_sensitive (widget, FALSE);
	g_signal_connect_object (G_OBJECT (widget), "clicked", (GCallback) redo_pressed, win, G_CONNECT_SWAPPED);
	priv->redo_button = widget;
	g_object_ref (priv->redo_button);

	gtk_container_add (GTK_CONTAINER (container), GTK_WIDGET (gtk_separator_tool_item_new ()));

	widget = GTK_WIDGET (gtk_tool_button_new_from_stock (GTK_STOCK_REFRESH));
	gtk_container_add (GTK_CONTAINER (container), widget);
	g_signal_connect_object (G_OBJECT (widget), "clicked", (GCallback) restart_pressed, win, G_CONNECT_SWAPPED);

	widget = GTK_WIDGET (gtk_tool_button_new_from_stock (GTK_STOCK_STOP));
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_widget_set_sensitive (widget, FALSE);
	g_signal_connect_object (G_OBJECT (widget), "clicked", (GCallback) stop_pressed, win, G_CONNECT_SWAPPED);
	priv->stop = widget;
	g_object_ref (priv->stop);

	gtk_container_add (GTK_CONTAINER (container), GTK_WIDGET (gtk_separator_tool_item_new ()));

	widget = GTK_WIDGET (gtk_tool_button_new_from_stock (GTK_STOCK_PROPERTIES));
	gtk_tool_button_set_label (GTK_TOOL_BUTTON (widget), "Fractal Type and Parameters...");
	gtk_container_add (GTK_CONTAINER (container), widget);
	g_signal_connect_object (G_OBJECT (widget), "clicked", (GCallback) type_dlg_requested, win, G_CONNECT_SWAPPED);

	widget = GTK_WIDGET (gtk_tool_button_new_from_stock (GTK_STOCK_ZOOM_OUT));
	gtk_container_add (GTK_CONTAINER (container), widget);
	g_signal_connect_object (G_OBJECT (widget), "clicked", (GCallback) zoomed_out, win, G_CONNECT_SWAPPED);


	/*
	 * Toolbar 2
	 */
	container = gtk_toolbar_new ();
	gtk_box_pack_start (GTK_BOX (main_vbox), container, FALSE, FALSE, 0);
	gtk_toolbar_set_style (GTK_TOOLBAR (container), GTK_TOOLBAR_ICONS);
	//gtk_toolbar_set_show_arrow (GTK_TOOLBAR (priv->toolbar2), FALSE);

	widget = GTK_WIDGET (gtk_radio_tool_button_new_from_stock (NULL, GTK_STOCK_ZOOM_IN));
	gtk_container_add (GTK_CONTAINER (container), widget);
	g_signal_connect_object (G_OBJECT (widget), "toggled", (GCallback) zoom_mode_selected, win, G_CONNECT_SWAPPED);
	priv->zoom_mode = widget;
	g_object_ref (priv->zoom_mode);
	GSList *group = gtk_radio_tool_button_get_group (GTK_RADIO_TOOL_BUTTON (priv->zoom_mode));

	widget = GTK_WIDGET (gtk_radio_tool_button_new (group));
	gtk_container_add (GTK_CONTAINER (container), widget);
	gtk_tool_button_set_label (GTK_TOOL_BUTTON (widget), "-> Julia");
	gtk_tool_item_set_homogeneous (GTK_TOOL_ITEM (widget), FALSE);
	g_signal_connect_object (G_OBJECT (widget), "toggled", (GCallback) to_julia_mode_selected, win, G_CONNECT_SWAPPED);
	priv->to_julia_mode = widget;
	g_object_ref (priv->to_julia_mode);

	/*
	 * Controls Table
	 */
	container = gtk_table_new (2, 1, FALSE);
	gtk_box_pack_start (GTK_BOX (main_vbox), container, FALSE, FALSE, 0);
	gtk_table_set_homogeneous (GTK_TABLE (container), FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (container), 2);
	gtk_table_set_col_spacings (GTK_TABLE (container), 2);

	widget = gtk_label_new ("Threads");
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	gtk_table_attach (GTK_TABLE (container), widget, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);

	widget = gtk_spin_button_new_with_range (1.0, 1024.0, 1.0);
	gtk_table_attach (GTK_TABLE (container), widget, 1, 2, 0, 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (widget), TRUE);
	gtk_entry_set_alignment (GTK_ENTRY (widget), 1.0);
	gtk_entry_set_width_chars (GTK_ENTRY (widget), 5);
	g_signal_connect_object (widget, "value-changed", (GCallback) threads_updated, win, G_CONNECT_SWAPPED);
	priv->threads_input = widget;
	g_object_ref (priv->threads_input);

	/*
	 * GtkMandel
	 */
	widget = gtk_mandel_new ();
	gtk_box_pack_start (GTK_BOX (main_vbox), widget, TRUE, TRUE, 0);
	gtk_mandel_set_selection_type (GTK_MANDEL (widget), GTK_MANDEL_SELECT_AREA);
	gtk_widget_set_size_request (widget, 50, 50);
	g_signal_connect_object (widget, "area-selected", (GCallback) area_selected, win, G_CONNECT_SWAPPED);
	g_signal_connect_object (widget, "point-selected", (GCallback) point_for_julia_selected, win, G_CONNECT_SWAPPED); /* XXX */
	g_signal_connect_object (widget, "rendering-started", (GCallback) rendering_started, win, G_CONNECT_SWAPPED);
	g_signal_connect_object (widget, "rendering-progress", (GCallback) rendering_progress, win, G_CONNECT_SWAPPED);
	g_signal_connect_object (widget, "rendering-stopped", (GCallback) rendering_stopped, win, G_CONNECT_SWAPPED);
	priv->mandel = widget;
	g_object_ref (priv->mandel);
	/* FIXME how to set initial widget size? */

	/*
	 * Status Bar
	 */
	container = gtk_hbox_new (false, 2);
	gtk_box_pack_start (GTK_BOX (main_vbox), container, FALSE, FALSE, 0);

	widget = gtk_progress_bar_new ();
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	gtk_progress_set_text_alignment (GTK_PROGRESS (widget), 0.0, 0.5);
	/* The default size request of GtkProgressBar is too large here */
	gtk_widget_get_size_request (widget, &ww, &wh);
	gtk_widget_set_size_request (widget, 10, wh);
	priv->status_info = widget;
	g_object_ref (priv->status_info);

	container2 = gtk_frame_new (NULL);
	gtk_box_pack_start (GTK_BOX (container), container2, FALSE, FALSE, 0);
	gtk_frame_set_shadow_type (GTK_FRAME (container2), GTK_SHADOW_IN);

	widget = gtk_label_new (NULL);
	gtk_container_add (GTK_CONTAINER (container2), widget);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.0, 0.5);
	priv->math_info = widget;
	g_object_ref (priv->math_info);

	gtk_widget_show_all (main_vbox);
}


static GtkWidget *
create_menus (FractalMainWindow *win)
{
	FractalMainWindowPrivate *const priv = win->priv;
	struct mainwin_menu *const menu = &priv->menu;
	GtkMenuShell *menu_bar, *shell, *subshell;
	GtkWidget *item;

	render_method_t i;

	menu_bar = GTK_MENU_SHELL (gtk_menu_bar_new ());

	item = gtk_menu_item_new_with_label ("File");
	gtk_menu_shell_append (menu_bar, item);

	shell = GTK_MENU_SHELL (gtk_menu_new ());
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), GTK_WIDGET (shell));

	item = my_gtk_stock_menu_item_with_label (GTK_STOCK_OPEN, "Open coordinate file...");
	gtk_menu_shell_append (shell, item);
	g_signal_connect_object (G_OBJECT (item), "activate", (GCallback) load_coords_requested, win, G_CONNECT_SWAPPED);

	item = my_gtk_stock_menu_item_with_label (GTK_STOCK_SAVE_AS, "Save coordinate file...");
	gtk_menu_shell_append (shell, item);
	g_signal_connect_object (G_OBJECT (item), "activate", (GCallback) save_coords_requested, win, G_CONNECT_SWAPPED);

	gtk_menu_shell_append (shell, gtk_separator_menu_item_new ());

	item = my_gtk_stock_menu_item_with_label (GTK_STOCK_PROPERTIES, "Fractal Type and Parameters...");
	gtk_menu_shell_append (shell, item);
	g_signal_connect_object (G_OBJECT (item), "activate", (GCallback) type_dlg_requested, win, G_CONNECT_SWAPPED);

	item = my_gtk_stock_menu_item_with_label (GTK_STOCK_INFO, "Area Info");
	gtk_menu_shell_append (shell, item);
	g_signal_connect_object (G_OBJECT (item), "activate", (GCallback) info_dlg_requested, win, G_CONNECT_SWAPPED);

	item = gtk_menu_item_new_with_label ("Rendering Method");
	gtk_menu_shell_append (shell, item);

	subshell = GTK_MENU_SHELL (gtk_menu_new ());
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), GTK_WIDGET (subshell));

	GSList *group = NULL;
	for (i = 0; i < RM_MAX; i++) {
		item = gtk_radio_menu_item_new_with_label (group, render_method_names[i]);
		menu->render_method_items[i] = item;
		group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (item));
		gtk_menu_shell_append (subshell, item);
		g_object_set_data (G_OBJECT (item), "render_method", FRACTAL_MAIN_WINDOW_GET_CLASS (win)->render_methods + i);
		if (i == DEFAULT_RENDER_METHOD)
			gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), TRUE);
		g_signal_connect_object (item, "toggled", (GCallback) render_method_updated, win, G_CONNECT_SWAPPED);
	}

	gtk_menu_shell_append (shell, gtk_separator_menu_item_new ());

	item = gtk_image_menu_item_new_from_stock (GTK_STOCK_QUIT, NULL);
	gtk_menu_shell_append (shell, item);
	g_signal_connect_object (G_OBJECT (item), "activate", (GCallback) quit_selected, win, G_CONNECT_SWAPPED);

	item = gtk_menu_item_new_with_label ("Help");
	gtk_menu_shell_append (menu_bar, item);

	shell = GTK_MENU_SHELL (gtk_menu_new ());
	gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), GTK_WIDGET (shell));

	item = gtk_image_menu_item_new_from_stock (GTK_STOCK_ABOUT, NULL);
	gtk_menu_shell_append (shell, item);
	g_signal_connect_object (G_OBJECT (item), "activate", (GCallback) about_dlg_requested, win, G_CONNECT_SWAPPED);

	return GTK_WIDGET (menu_bar);
}


FractalMainWindow *
fractal_main_window_new (void)
{
	FractalMainWindow *win = FRACTAL_MAIN_WINDOW (g_object_new (TYPE_FRACTAL_MAIN_WINDOW, NULL));
	return win;
}


static void
area_selected (FractalMainWindow *win, struct mandel_area *area, gpointer data)
{
	fractal_main_window_set_area (win, area);
	restart_thread (win);
}


static void
point_for_julia_selected (FractalMainWindow *win, struct mandel_point *point, gpointer data)
{
	struct mandeldata *md = malloc (sizeof (*md));
	mandeldata_init (md, fractal_type_by_id (FRACTAL_JULIA));
	mandeldata_set_defaults (md);
	struct mandelbrot_param *oldmparam = (struct mandelbrot_param *) win->md->type_param;
	struct julia_param *jparam = (struct julia_param *) md->type_param;
	jparam->mjparam.zpower = oldmparam->mjparam.zpower;
	mpf_set (jparam->param.real, point->real);
	mpf_set (jparam->param.imag, point->imag);
	fractal_main_window_set_mandeldata (win, md);
	restart_thread (win);
}


static void
render_method_updated (FractalMainWindow *win, gpointer data)
{
	FractalMainWindowPrivate *const priv = win->priv;
	GtkCheckMenuItem *item = GTK_CHECK_MENU_ITEM (data);
	if (!item->active)
		return;
	render_method_t *method = (render_method_t *) g_object_get_data (G_OBJECT (item), "render_method");
	gtk_mandel_set_render_method (GTK_MANDEL (priv->mandel), *method);
	/* restart_thread (win); */
}


void
fractal_main_window_set_mandeldata (FractalMainWindow *win, const struct mandeldata *md)
{
	FractalMainWindowPrivate *const priv = win->priv;

	GSList *l;
	if (win->md != NULL) {
		priv->undo = g_slist_prepend (priv->undo, (gpointer) win->md);
		gtk_widget_set_sensitive (priv->undo_button, TRUE);
	}
	update_mandeldata (win, md);
	l = priv->redo;
	while (l != NULL) {
		GSList *next_l = g_slist_next (l);
		mandeldata_clear (l->data);
		free (l->data);
		g_slist_free_1 (l);
		l = next_l;
	}
	priv->redo = NULL;
	gtk_widget_set_sensitive (priv->redo_button, FALSE);
}


static void
undo_pressed (FractalMainWindow *win, gpointer data)
{
	FractalMainWindowPrivate *const priv = win->priv;

	if (priv->undo == NULL) {
		fprintf (stderr, "! Undo called with empty history.\n");
		return;
	}
	priv->redo = g_slist_prepend (priv->redo, (gpointer) win->md);
	update_mandeldata (win, (struct mandeldata *) priv->undo->data);
	GSList *old = priv->undo;
	priv->undo = g_slist_next (priv->undo);
	g_slist_free_1 (old);
	if (priv->undo == NULL)
		gtk_widget_set_sensitive (priv->undo_button, FALSE);
	gtk_widget_set_sensitive (priv->redo_button, TRUE);
	restart_thread (win);
}


/* FIXME: This is an exact "mirror" of undo_pressed(). */
static void
redo_pressed (FractalMainWindow *win, gpointer data)
{
	FractalMainWindowPrivate *const priv = win->priv;

	if (priv->redo == NULL) {
		fprintf (stderr, "! Redo called with empty history.\n");
		return;
	}
	priv->undo = g_slist_prepend (priv->undo, (gpointer) win->md);
	update_mandeldata (win, (struct mandeldata *) priv->redo->data);
	GSList *old = priv->redo;
	priv->redo = g_slist_next (priv->redo);
	g_slist_free_1 (old);
	if (priv->redo == NULL)
		gtk_widget_set_sensitive (priv->redo_button, FALSE);
	gtk_widget_set_sensitive (priv->undo_button, TRUE);
	restart_thread (win);
}


static void
restart_thread (FractalMainWindow *win)
{
	FractalMainWindowPrivate *const priv = win->priv;
	gtk_mandel_start (GTK_MANDEL (priv->mandel));
}


static void
rendering_started (FractalMainWindow *win, gulong bits, gpointer data)
{
	FractalMainWindowPrivate *const priv = win->priv;
	char buf[64];
	int r;

	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (priv->status_info), 0.0);
	gtk_progress_bar_set_text (GTK_PROGRESS_BAR (priv->status_info), "Rendering");
	gtk_widget_set_sensitive (priv->stop, TRUE);
	if (bits == 0)
		gtk_label_set_text (GTK_LABEL (priv->math_info), "FP");
	else {
		r = snprintf (buf, sizeof (buf), "MP (%lu bits)", bits);
		if (r < 0 || r >= sizeof (buf))
			return;
		gtk_label_set_text (GTK_LABEL (priv->math_info), buf);
	}
}


static void
quit_selected (FractalMainWindow *win, gpointer data)
{
	gtk_main_quit ();
}


static void
rendering_stopped (FractalMainWindow *win, gboolean completed, gpointer data)
{
	FractalMainWindowPrivate *const priv = win->priv;
	const char *msg;
	char buf[256];
	double progress;
	if (completed) {
		msg = "Finished";
		progress = 1.0;
	} else {
		msg = "Stopped";
		progress = gtk_mandel_get_progress (GTK_MANDEL (priv->mandel));
		int r = snprintf (buf, sizeof (buf), "Stopped (%.1f%%)", progress * 100.0);
		if (r > 0 && r < sizeof (buf))
			msg = buf;
	}

	gtk_widget_set_sensitive (priv->stop, FALSE);
	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (priv->status_info), progress);
	gtk_progress_bar_set_text (GTK_PROGRESS_BAR (priv->status_info), msg);
}


static void
restart_pressed (FractalMainWindow *win, gpointer data)
{
	restart_thread (win);
}


static void
stop_pressed (FractalMainWindow *win, gpointer data)
{
	FractalMainWindowPrivate *const priv = win->priv;
	GtkMandel *mandel = GTK_MANDEL (priv->mandel);
	gtk_progress_bar_set_text (GTK_PROGRESS_BAR (priv->status_info), "Stopping...");
	gtk_mandel_stop (mandel);
}


static void
zoom_2exp (FractalMainWindow *win, long exponent)
{
	struct mandeldata *md = malloc (sizeof (*md));
	mandeldata_clone (md, win->md);
	if (exponent > 0)
		mpf_mul_2exp (md->area.magf, md->area.magf, exponent);
	else
		mpf_div_2exp (md->area.magf, md->area.magf, -exponent);
	fractal_main_window_set_mandeldata (win, md);
	restart_thread (win);
}


static void
zoomed_out (FractalMainWindow *win, gpointer data)
{
	zoom_2exp (win, -1);
}


static void
threads_updated (FractalMainWindow *win, gpointer data)
{
	FractalMainWindowPrivate *const priv = win->priv;
	gtk_mandel_set_thread_count (GTK_MANDEL (priv->mandel), gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (priv->threads_input)));
}


void
fractal_main_window_set_threads (FractalMainWindow *win, unsigned threads)
{
	FractalMainWindowPrivate *const priv = win->priv;
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->threads_input), threads);
}


static void
update_mandeldata (FractalMainWindow *win, const struct mandeldata *md)
{
	FractalMainWindowPrivate *const priv = win->priv;
	win->md = md;
	gtk_mandel_set_mandeldata (GTK_MANDEL (priv->mandel), md);
	g_signal_emit (win, FRACTAL_MAIN_WINDOW_GET_CLASS (win)->mandeldata_updated_signal, 0);
}


/* XXX this should disappear */
static void
fractal_main_window_set_area (FractalMainWindow *win, struct mandel_area *area)
{
	struct mandeldata *md = malloc (sizeof (*md));
	mandeldata_clone (md, win->md);
	mpf_set (md->area.center.real, area->center.real);
	mpf_set (md->area.center.imag, area->center.imag);
	mpf_set (md->area.magf, area->magf);
	fractal_main_window_set_mandeldata (win, md);
}


static void
fractal_main_window_set_mode (FractalMainWindow *win, FractalMainWindowMode mode)
{
	FractalMainWindowPrivate *const priv = win->priv;
	priv->mode = mode;
	switch (mode) {
		case FRACTAL_MODE_ZOOM:
			gtk_mandel_set_selection_type (GTK_MANDEL (priv->mandel), GTK_MANDEL_SELECT_AREA);
			break;
		case FRACTAL_MODE_TO_JULIA:
			gtk_mandel_set_selection_type (GTK_MANDEL (priv->mandel), GTK_MANDEL_SELECT_POINT);
			break;
		default:
			fprintf (stderr, "* Invalid FractalMainWindow mode %d\n", (int) mode);
			break;
	}
}


static void
zoom_mode_selected (FractalMainWindow *win, gpointer data)
{
	fractal_main_window_set_mode (win, FRACTAL_MODE_ZOOM);
}


static void
to_julia_mode_selected (FractalMainWindow *win, gpointer data)
{
	fractal_main_window_set_mode (win, FRACTAL_MODE_TO_JULIA);
}


static void
rendering_progress (FractalMainWindow *win, gdouble progress, gpointer data)
{
	FractalMainWindowPrivate *const priv = win->priv;
	char buf[256];
	int r = snprintf (buf, sizeof (buf), "Rendering (%.1f%%)", (double) progress * 100.0);
	if (r > 0 && r < sizeof (buf))
		gtk_progress_bar_set_text (GTK_PROGRESS_BAR (priv->status_info), buf);
	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (priv->status_info), progress);
}


const struct mandeldata *
fractal_main_window_get_mandeldata (FractalMainWindow *win)
{
	return win->md;
}


void
fractal_main_window_restart (FractalMainWindow *win)
{
	restart_thread (win);
}


static void
load_coords_requested (FractalMainWindow *win, gpointer data)
{
	g_signal_emit (win, FRACTAL_MAIN_WINDOW_GET_CLASS (win)->load_coords_signal, 0);
}


static void
save_coords_requested (FractalMainWindow *win, gpointer data)
{
	g_signal_emit (win, FRACTAL_MAIN_WINDOW_GET_CLASS (win)->save_coords_signal, 0);
}


static void
info_dlg_requested (FractalMainWindow *win, gpointer data)
{
	g_signal_emit (win, FRACTAL_MAIN_WINDOW_GET_CLASS (win)->info_dlg_signal, 0);
}


static void
type_dlg_requested (FractalMainWindow *win, gpointer data)
{
	g_signal_emit (win, FRACTAL_MAIN_WINDOW_GET_CLASS (win)->type_dlg_signal, 0);
}


static void
about_dlg_requested (FractalMainWindow *win, gpointer data)
{
	g_signal_emit (win, FRACTAL_MAIN_WINDOW_GET_CLASS (win)->about_dlg_signal, 0);
}
