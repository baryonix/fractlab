#ifndef _GTKMANDEL_H
#define _GTKMANDEL_H


#include "mandelbrot.h"


typedef struct _GtkMandel GtkMandel;
typedef struct _GtkMandelClass GtkMandelClass;


struct _GtkMandel
{
	GtkDrawingArea widget;
	GdkPixmap *pixmap;
	GdkGC *gc, *pm_gc, *frame_gc;
	GdkColor black, red, white;
	GThread *thread;
	struct mandeldata *md;
	gdouble center_x, center_y, selection_size;
};


struct _GtkMandelClass
{
	GtkDrawingAreaClass parent_class;
};

#define GTK_MANDEL(obj) GTK_CHECK_CAST (obj, gtk_mandel_get_type (), GtkMandel)
#define GTK_MANDEL_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, gtk_mandel_get_type (), GtkMandel)
#define GTK_IS_MANDEL(obj) GET_CHECK_TYPE (obj, gtk_mandel_get_type ())

extern GdkColor mandelcolors[];

GType gtk_mandel_get_type ();
GtkWidget *gtk_mandel_new (void);
void gtk_mandel_restart_thread (GtkMandel *mandel, mpf_t xmin, mpf_t xmax, mpf_t ymin, mpf_t ymax, unsigned maxiter, render_method_t render_method);

#endif /* _GTKMANDEL_H */
