#ifndef _GTKMANDEL_H
#define _GTKMANDEL_H


#include "mandelbrot.h"


typedef struct
{
	GtkDrawingArea widget;
	GdkPixbuf *pixbuf;
	GMutex *pb_mutex;
	int pb_rowstride, pb_nchan;
	guchar *pb_data;
	bool need_redraw;
	int pb_xmin, pb_xmax, pb_ymin, pb_ymax; /* These indicate the area that has been updated and must be redrawn on screen. */
	GdkGC *gc, *frame_gc;
	GdkColor black, red, white;
	GThread *thread;
	const struct mandeldata *md;
	render_method_t render_method;
	unsigned thread_count;
	struct mandel_renderer *renderer;
	guint redraw_source_id;
	gdouble center_x, center_y, selection_size;
	int cur_w, cur_h;
	double aspect;
	bool selection_active;
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
	mpf_t cx, cy, magf;
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
void gtk_mandel_set_mandeldata (GtkMandel *mandel, const struct mandeldata *md);
void gtk_mandel_set_render_method (GtkMandel *mandel, render_method_t render_method);
void gtk_mandel_set_thread_count (GtkMandel *mandel, unsigned thread_count);
void gtk_mandel_start (GtkMandel *mandel);
void gtk_mandel_stop (GtkMandel *mandel);
void gtk_mandel_redraw (GtkMandel *mandel);

GType gtk_mandel_area_get_type ();
GtkMandelArea *gtk_mandel_area_new (const mpf_t cx, const mpf_t cy, const mpf_t magf);
GtkMandelArea *gtk_mandel_area_new_from_file (const char *filename);

#endif /* _GTKMANDEL_H */
