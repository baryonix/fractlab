#ifndef _GTKMANDEL_GUI_H
#define _GTKMANDEL_GUI_H

#include "gui-mainwin.h"
#include "gui-infodlg.h"
#include "gui-typedlg.h"

typedef struct {
	GObject parent;
	FractalMainWindow *main_window;
	GtkWidget *open_coord_chooser;
	GtkWidget *save_coord_chooser;
	FractalInfoDialog *fractal_info_dlg;
	FractalTypeDialog *fractal_type_dlg;
	GtkAboutDialog *about_dlg;
	bool disposed;
} GtkMandelApplication;

typedef struct {
	GObjectClass parent_class;
	//GtkIconFactory *icon_factory;
} GtkMandelApplicationClass;

#define GTK_MANDEL_APPLICATION(obj) GTK_CHECK_CAST (obj, gtk_mandel_application_get_type (), GtkMandelApplication)
#define GTK_MANDEL_APPLICATION_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, gtk_mandel_application_get_type (), GtkMandelApplication)
#define GTK_IS_MANDEL_APPLICATION(obj) GET_CHECK_TYPE (obj, gtk_mandel_application_get_type ())

GType gtk_mandel_application_get_type (void);
GtkMandelApplication *gtk_mandel_application_new (const struct mandeldata *md);
void gtk_mandel_application_start (GtkMandelApplication *app);

#endif /* _GTKMANDEL_GUI_H */
