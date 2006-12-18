#ifndef _GTKMANDEL_GUI_H
#define _GTKMANDEL_GUI_H


typedef struct {
	GObject parent;
	struct {
		GtkWidget *win;
		GtkWidget *main_vbox;
		GtkWidget *maxiter_hbox, *maxiter_label, *maxiter_input;
		GtkWidget *log_colors_hbox, *log_colors_checkbox, *log_colors_input;
		GtkWidget *mandel;
	} mainwin;
	struct {
		GtkWidget *bar;
		GtkWidget *file_item, *file_menu;
		GtkWidget *area_info_item;
		GtkWidget *render_item, *render_menu;
		GtkWidget *render_method_items[RM_MAX];
		GtkWidget *quit_item;
		GSList *render_item_group;
	} menu;
} GtkMandelApplication;

typedef struct {
	GObjectClass parent_class;
	render_method_t render_methods[RM_MAX];
} GtkMandelApplicationClass;

#define GTK_MANDEL_APPLICATION(obj) GTK_CHECK_CAST (obj, gtk_mandel_application_get_type (), GtkMandelApplication)
#define GTK_MANDEL_APPLICATION_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, gtk_mandel_application_get_type (), GtkMandelApplication)
#define GTK_IS_MANDEL_APPLICATION(obj) GET_CHECK_TYPE (obj, gtk_mandel_application_get_type ())

GType gtk_mandel_application_get_type ();
GtkMandelApplication *gtk_mandel_application_new ();
void gtk_mandel_application_start (GtkMandelApplication *app, mpf_t xmin, mpf_t xmax, mpf_t ymin, mpf_t ymax);

#endif /* _GTKMANDEL_GUI_H */
