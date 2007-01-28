#ifndef _GTKMANDEL_GUI_H
#define _GTKMANDEL_GUI_H

#include "gui-typedlg.h"

struct area_info_item {
	GtkWidget *label, *view;
	GtkTextBuffer *buffer;
};

typedef enum {
	GTK_MANDEL_APP_MODE_ZOOM = 0,
	GTK_MANDEL_APP_MODE_TO_JULIA = 1,
	GTK_MANDEL_APP_MODE_MAX = 2
} GtkMandelAppMode;


typedef struct {
	GObject parent;
	struct {
		GtkWidget *win;
		GtkWidget *main_vbox;
		GtkWidget *toolbar1, *undo, *redo, *toolbar1_sep1, *restart, *stop, *toolbar1_sep2, *fractal_type, *zoom_out;
		GtkWidget *toolbar2, *zoom_mode, *to_julia_mode;
		GSList *mode_group;
		GtkWidget *controls_table;
		GtkWidget *threads_label, *threads_input;
		GtkWidget *mandel;
		GtkWidget *status_hbox, *status_info, *math_info, *math_info_frame;
	} mainwin;
	struct {
		GtkWidget *bar;
		GtkWidget *file_item, *file_menu;
		GtkWidget *open_coord_item;
		GtkWidget *save_coord_item;
		GtkWidget *fractal_type_item;
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
	FractalTypeDialog *fractal_type_dlg;
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
