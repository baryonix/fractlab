#ifndef _GTKMANDEL_GUI_H
#define _GTKMANDEL_GUI_H

struct area_info_item {
	GtkWidget *label, *view;
	GtkTextBuffer *buffer;
};

typedef enum {
	GTK_MANDEL_APP_MODE_ZOOM = 0,
	GTK_MANDEL_APP_MODE_TO_JULIA = 1,
	GTK_MANDEL_APP_MODE_MAX = 2
} GtkMandelAppMode;


#if 0
struct mandelbrot_param {
	GtkWidget *table;
	GtkWidget *zpower_label, *zpower_input;
	GtkWidget *distance_est;
};


struct julia_param {
	GtkWidget *table;
	GtkWidget *zpower_label, *zpower_input;
	GtkWidget *preal_label, *preal_input;
	GtkWidget *pimag_label, *pimag_input;
};


struct fractal_type_dlg {
	GtkWidget *dialog;
	GtkListStore *type_list;
	GtkCellRenderer *type_renderer;
	GtkWidget *type_hbox, *type_label, *type_input;
	GtkWidget *area_frame, *area_table;
	GtkWidget *area_creal_label, *area_creal_input;
	GtkWidget *area_cimag_label, *area_cimag_input;
	GtkWidget *area_magf_label, *area_magf_input;
	GtkWidget *type_param_frame;
	GtkWidget *type_param_notebook;
	struct mandelbrot_param mandelbrot_param;
	struct julia_param julia_param;
};
#endif



typedef struct {
	GObject parent;
	struct {
		GtkWidget *win;
		GtkWidget *main_vbox;
		GtkWidget *toolbar1, *undo, *redo, *toolbar1_sep1, *restart, *stop, *toolbar1_sep2, *zoom_out;
		GtkWidget *toolbar2, *zoom_mode, *to_julia_mode;
		GSList *mode_group;
		GtkWidget *controls_table;
		GtkWidget *maxiter_label, *maxiter_input;
		GtkWidget *log_colors_checkbox, *log_colors_hbox, *log_colors_label, *log_colors_input;
		GtkWidget *zpower_label, *zpower_input;
		GtkWidget *threads_label, *threads_input;
		GtkWidget *mandel;
		GtkWidget *status_hbox, *status_info, *math_info, *math_info_frame;
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
	struct mandeldata *md;
	bool updating_gui;
	GtkMandelAppMode mode;
	//struct fractal_type_dlg fractal_type_dlg;
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
GtkMandelApplication *gtk_mandel_application_new (const struct mandeldata *md);
void gtk_mandel_application_set_mandeldata (GtkMandelApplication *app, struct mandeldata *md);
void gtk_mandel_application_start (GtkMandelApplication *app);
void gtk_mandel_app_set_mode (GtkMandelApplication *app, GtkMandelAppMode mode);

#endif /* _GTKMANDEL_GUI_H */
