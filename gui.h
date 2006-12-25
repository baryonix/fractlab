#ifndef _GTKMANDEL_GUI_H
#define _GTKMANDEL_GUI_H

struct area_info_item {
	GtkWidget *label, *view;
	GtkTextBuffer *buffer;
};

typedef struct {
	GObject parent;
	struct {
		GtkWidget *win;
		GtkWidget *main_vbox;
		GtkWidget *tool_bar;
		GtkWidget *undo, *redo, *toolbar_sep1, *restart, *stop, *toolbar_sep2, *zoom_in, *zoom_out;
		GtkWidget *maxiter_hbox, *maxiter_label, *maxiter_input;
		GtkWidget *log_colors_hbox, *log_colors_checkbox, *log_colors_input;
		GtkWidget *mandel;
		GtkWidget *status_hbox, *status_info, *status_info_frame, *math_info, *math_info_frame;
	} mainwin;
	struct {
		GtkWidget *bar;
		GtkWidget *file_item, *file_menu;
		GtkWidget *open_coord_item;
		GtkWidget *save_coord_item;
		GtkWidget *area_info_item;
		GtkWidget *render_item, *render_menu;
		GtkWidget *render_method_items[RM_MAX];
		GtkWidget *quit_item;
		GSList *render_item_group;
	} menu;
	struct {
		GtkWidget *dialog;
		GtkWidget *notebook;
		GtkWidget *corners_label, *center_label;
		struct {
			GtkWidget *table;
			struct area_info_item items[3];
		} center;
		struct {
			GtkWidget *table;
			struct area_info_item items[4];
		} corners;
	} area_info;
	GtkWidget *open_coord_chooser;
	GtkWidget *save_coord_chooser;
	GSList *undo, *redo;
	GtkMandelArea *area;
	unsigned maxiter;
	render_method_t render_method;
	double log_factor;
} GtkMandelApplication;

typedef struct {
	GObjectClass parent_class;
	render_method_t render_methods[RM_MAX];
	//GtkIconFactory *icon_factory;
} GtkMandelApplicationClass;

#define GTK_MANDEL_APPLICATION(obj) GTK_CHECK_CAST (obj, gtk_mandel_application_get_type (), GtkMandelApplication)
#define GTK_MANDEL_APPLICATION_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, gtk_mandel_application_get_type (), GtkMandelApplication)
#define GTK_IS_MANDEL_APPLICATION(obj) GET_CHECK_TYPE (obj, gtk_mandel_application_get_type ())

GType gtk_mandel_application_get_type ();
GtkMandelApplication *gtk_mandel_application_new (GtkMandelArea *area, unsigned maxiter, render_method_t render_method, double log_factor);
void gtk_mandel_application_set_area (GtkMandelApplication *app, GtkMandelArea *area);
void gtk_mandel_application_set_maxiter (GtkMandelApplication *app, unsigned long maxiter);
void gtk_mandel_application_start (GtkMandelApplication *app);

#endif /* _GTKMANDEL_GUI_H */
