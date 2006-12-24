#ifndef _GTKMANDEL_H
#define _GTKMANDEL_H


#include "mandelbrot.h"


typedef struct
{
	GtkDrawingArea widget;
	GdkPixmap *pixmap;
	GdkGC *gc, *pm_gc, *frame_gc;
	GdkColor black, red, white;
	GThread *thread;
	struct mandeldata *md;
	gdouble center_x, center_y, selection_size;
} GtkMandel;


typedef struct
{
	GtkDrawingAreaClass parent_class;
	guint selection_signal;
	guint rendering_started_signal;
	guint rendering_stopped_signal;
} GtkMandelClass;

#define GTK_MANDEL(obj) GTK_CHECK_CAST ((obj), gtk_mandel_get_type (), GtkMandel)
#define GTK_MANDEL_CLASS(klass) GTK_CHECK_CLASS_CAST ((klass), gtk_mandel_get_type (), GtkMandel)
#define GTK_IS_MANDEL(obj) GET_CHECK_TYPE ((obj), gtk_mandel_get_type ())
#define GTK_MANDEL_GET_CLASS(obj) G_TYPE_INSTANCE_GET_CLASS ((obj), GtkMandel, GtkMandelClass)


typedef struct {
	GObject parent;
	mpf_t xmin, xmax, ymin, ymax;
} GtkMandelArea;

typedef struct {
	GObjectClass parent_class;
} GtkMandelAreaClass;

#define GTK_MANDEL_AREA(obj) GTK_CHECK_CAST (obj, gtk_mandel_area_get_type (), GtkMandelArea)
#define GTK_MANDEL_AREA_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, gtk_mandel_area_get_type (), GtkMandelArea)
#define GTK_IS_MANDEL_AREA(obj) GET_CHECK_TYPE (obj, gtk_mandel_area_get_type ())


extern GdkColor mandelcolors[];

GType gtk_mandel_get_type ();
GtkWidget *gtk_mandel_new (void);
void gtk_mandel_restart_thread (GtkMandel *mandel, mpf_t xmin, mpf_t xmax, mpf_t ymin, mpf_t ymax, unsigned maxiter, render_method_t render_method, double log_factor);

GType gtk_mandel_area_get_type ();
GtkMandelArea *gtk_mandel_area_new (mpf_t xmin, mpf_t xmax, mpf_t ymin, mpf_t ymax);
GtkMandelArea *gtk_mandel_area_new_from_file (const char *filename);

#endif /* _GTKMANDEL_H */
