// ANSI C
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

// POSIX
#include <unistd.h>

// GTK
#include <gtk/gtk.h>
#include <gdk/gdk.h>

// GMP
#include <gmp.h>


#include "gtkmandel.h"
#include "mandelbrot.h"
#include "defs.h"
#include "file.h"


struct rendering_started_info {
	GtkMandel *mandel;
	int bits;
};

struct rendering_stopped_info {
	GtkMandel *mandel;
	bool completed;
};


static void gtk_mandel_display_pixel (unsigned x, unsigned y, unsigned iter, void *user_data);
static void gtk_mandel_display_rect (unsigned x, unsigned y, unsigned w, unsigned h, unsigned iter, void *user_data);
static gboolean mouse_event (GtkWidget *widget, GdkEventButton *e, gpointer user_data);
static void my_realize (GtkWidget *my_img, gpointer user_data);
static void gtk_mandel_class_init (GtkMandelClass *class);
static void gtk_mandel_init (GtkMandel *mandel);
static void gtk_mandel_area_class_init (GtkMandelAreaClass *class);
static void gtk_mandel_area_init (GtkMandelArea *mandel);
static gboolean my_expose (GtkWidget *widget, GdkEventExpose *event, gpointer user_data);
static gpointer calcmandel (gpointer data);
static void size_allocate (GtkWidget *widget, GtkAllocation allocation, gpointer data);
static gboolean do_emit_rendering_started (gpointer data);
static gboolean do_emit_rendering_stopped (gpointer data);
static gboolean redraw_source_func (gpointer data);
static gboolean redraw_source_func_once (gpointer data);
static void redraw_area (GtkMandel *mandel, int x, int y, int w, int h);


GdkColor mandelcolors[COLORS];


GtkWidget *
gtk_mandel_new (void)
{
	GtkMandel *mandel = g_object_new (gtk_mandel_get_type (), NULL);
	return GTK_WIDGET (mandel);
}


GType
gtk_mandel_get_type ()
{
	static GType mandel_type = 0;

	if (!mandel_type) {
		static const GTypeInfo mandel_info = {
			sizeof (GtkMandelClass),
			NULL, NULL,
			(GClassInitFunc) gtk_mandel_class_init,
			NULL, NULL,
			sizeof (GtkMandel),
			0,
			(GInstanceInitFunc) gtk_mandel_init
		};
		mandel_type = g_type_register_static (GTK_TYPE_DRAWING_AREA, "GtkMandel", &mandel_info, 0);
	}

	return mandel_type;
}


GType
gtk_mandel_area_get_type ()
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (GtkMandelAreaClass),
			NULL, NULL,
			(GClassInitFunc) gtk_mandel_area_class_init,
			NULL, NULL,
			sizeof (GtkMandelArea),
			0,
			(GInstanceInitFunc) gtk_mandel_area_init
		};
		type = g_type_register_static (G_TYPE_OBJECT, "GtkMandelArea", &info, 0);
	}

	return type;
}


static void
gtk_mandel_class_init (GtkMandelClass *class)
{
	class->selection_signal = g_signal_new (
		"selection",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__OBJECT,
		G_TYPE_NONE,
		1,
		gtk_mandel_area_get_type ()
	);
	class->rendering_started_signal = g_signal_new (
		"rendering-started",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__ULONG,
		G_TYPE_NONE,
		1,
		G_TYPE_ULONG
	);
	class->rendering_stopped_signal = g_signal_new (
		"rendering-stopped",
		G_TYPE_FROM_CLASS (class),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__BOOLEAN,
		G_TYPE_NONE,
		1,
		G_TYPE_BOOLEAN
	);
}

static void
gtk_mandel_init (GtkMandel *mandel)
{
	mandel->need_redraw = false;
	mandel->pb_mutex = g_mutex_new ();

	g_signal_connect (G_OBJECT (mandel), "realize", (GCallback) my_realize, NULL);
	g_signal_connect (G_OBJECT (mandel), "button-press-event", (GCallback) mouse_event, NULL);
	g_signal_connect (G_OBJECT (mandel), "button-release-event", (GCallback) mouse_event, NULL);
	g_signal_connect (G_OBJECT (mandel), "motion-notify-event", (GCallback) mouse_event, NULL);
	g_signal_connect (G_OBJECT (mandel), "expose-event", (GCallback) my_expose, NULL);
	g_signal_connect (G_OBJECT (mandel), "size-allocate", (GCallback) size_allocate, NULL);
}


static void
gtk_mandel_area_class_init (GtkMandelAreaClass *class)
{
}


static void
gtk_mandel_area_init (GtkMandelArea *area)
{
	mpf_init (area->cx);
	mpf_init (area->cy);
	mpf_init (area->magf);
}


GtkMandelArea *
gtk_mandel_area_new (mpf_t cx, mpf_t cy, mpf_t magf)
{
	GtkMandelArea *area = g_object_new (gtk_mandel_area_get_type (), NULL);
	mpf_set (area->cx, cx);
	mpf_set (area->cy, cy);
	mpf_set (area->magf, magf);
	return area;
}


void
gtk_mandel_restart_thread (GtkMandel *mandel, mpf_t cx, mpf_t cy, mpf_t magf, unsigned maxiter, render_method_t render_method, double log_factor)
{
	GtkWidget *widget = GTK_WIDGET (mandel);
	struct mandeldata *md = malloc (sizeof (struct mandeldata));
	int oldw = -1, oldh = -1;
	memset (md, 0, sizeof (*md));

	md->type = FRACTAL_MANDELBROT;
	mpf_init_set (md->cx, cx);
	mpf_init_set (md->cy, cy);
	mpf_init_set (md->magf, magf);
	md->maxiter = maxiter;
	md->user_data = mandel;
	md->render_method = render_method;
	md->log_factor = log_factor;
	md->terminate = false;
	md->w = widget->allocation.width;
	md->h = widget->allocation.height;
	md->data = malloc (md->w * md->h * sizeof (*md->data));
	md->display_pixel = gtk_mandel_display_pixel;
	md->display_rect = gtk_mandel_display_rect;

	mandel_init_coords (md);

	if (mandel->thread != NULL) {
		mandel->md->terminate = true;
		g_thread_join (mandel->thread);
		mandel->thread = NULL;
		oldw = mandel->md->w;
		oldh = mandel->md->h;
		free (mandel->md->data);
		free (mandel->md);
		mandel->md = NULL;
	}

	/*
	 * The rendering thread has terminated, so we can touch the pixbuf
	 * and related fields without locking the pb_mutex.
	 */

	if (mandel->pixbuf == NULL || md->w != oldw || md->h != oldh) {
		if (mandel->pixbuf != NULL)
			g_object_unref (G_OBJECT (mandel->pixbuf));
		mandel->pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, md->w, md->h);
		mandel->pb_rowstride = gdk_pixbuf_get_rowstride (mandel->pixbuf);
		mandel->pb_nchan = gdk_pixbuf_get_n_channels (mandel->pixbuf);
		mandel->pb_data = gdk_pixbuf_get_pixels (mandel->pixbuf);
	}

	/* Clear image */
	gdk_pixbuf_fill (mandel->pixbuf, 0);
	gdk_gc_set_foreground (mandel->gc, &mandel->black);
	gdk_draw_rectangle (GDK_DRAWABLE (widget->window), mandel->gc, true, 0, 0, md->w, md->h);

	/*
	 * Emit the "rendering-started" signal.
	 * We enqueue it to the main loop instead of calling g_signal_emit()
	 * directly from here. This is required because the "rendering-stopped"
	 * signal has been enqueued in the same way, and "rendering-started"
	 * must be emitted _after_ it.
	 */
	struct rendering_started_info *info = malloc (sizeof (struct rendering_started_info));
	info->mandel = mandel;
	info->bits = get_precision (md);
	g_idle_add (do_emit_rendering_started, info);

	mandel->redraw_source_id = g_timeout_add (500, redraw_source_func, mandel);

	mandel->thread = g_thread_create (calcmandel, (gpointer) md, true, NULL);
}


static void
my_realize (GtkWidget *my_img, gpointer user_data)
{
	GtkMandel *mandel = GTK_MANDEL (my_img);
	mandel->gc = gdk_gc_new (GDK_DRAWABLE (my_img->window));
	mandel->frame_gc = gdk_gc_new (GDK_DRAWABLE (my_img->window));
	gtk_widget_add_events (my_img, GDK_BUTTON_PRESS_MASK |
		GDK_BUTTON_RELEASE_MASK | GDK_BUTTON1_MOTION_MASK |
		GDK_EXPOSURE_MASK);
	GdkColormap *cmap = gdk_colormap_get_system ();
	gdk_color_parse ("black", &mandel->black);
	gdk_color_alloc (cmap, &mandel->black);
	gdk_color_parse ("red", &mandel->red);
	gdk_color_alloc (cmap, &mandel->red);
	gdk_color_parse ("white", &mandel->white);
	gdk_color_alloc (cmap, &mandel->white);

	gdk_gc_set_foreground (mandel->frame_gc, &mandel->red);
	gdk_gc_set_background (mandel->frame_gc, &mandel->white);
	gdk_gc_set_line_attributes (mandel->frame_gc, 1, GDK_LINE_DOUBLE_DASH, GDK_CAP_NOT_LAST, GDK_JOIN_MITER);

	mandel->thread = NULL;
}


static gboolean
mouse_event (GtkWidget *widget, GdkEventButton *e, gpointer user_data)
{
	GtkMandel *mandel = GTK_MANDEL (widget);
	switch (e->type) {
		case GDK_BUTTON_PRESS: {
			mandel->center_x = e->x;
			mandel->center_y = e->y;
			mandel->selection_size = 0.0;
			return TRUE;
		}
		case GDK_BUTTON_RELEASE: {
			if (mandel->center_x == e->x && mandel->center_y == e->y)
				return TRUE; /* avoid zero size selections */
			mpf_t cx, cy, dx, dy, mpaspect;
			mpf_init (cx);
			mpf_init (cy);
			mpf_init (dx);
			mpf_init (dy);
			mpf_init (mpaspect);
			mpf_set_d (mpaspect, mandel->md->aspect);
			mandel_convert_x_f (mandel->md, cx, mandel->center_x);
			mandel_convert_y_f (mandel->md, cy, mandel->center_y);
			mandel_convert_x_f (mandel->md, dx, e->x);
			mandel_convert_y_f (mandel->md, dy, e->y);
			mpf_sub (dx, cx, dx);
			mpf_abs (dx, dx);
			mpf_sub (dy, cy, dy);
			mpf_abs (dy, dy);
			if (mandel->md->aspect > 1.0)
				mpf_div (dx, dx, mpaspect);
			else
				mpf_mul (dy, dy, mpaspect);
			if (mpf_cmp (dx, dy) < 0)
				mpf_set (dx, dy);
			mpf_ui_div (dx, 1, dx);
			GtkMandelArea *area = gtk_mandel_area_new (cx, cy, dx);
			mpf_clear (cx);
			mpf_clear (cy);
			mpf_clear (dx);
			mpf_clear (dy);
			mpf_clear (mpaspect);
			g_signal_emit (mandel, GTK_MANDEL_GET_CLASS (mandel)->selection_signal, 0, area);
			g_object_unref (G_OBJECT (area));
			return TRUE;
		}
		case GDK_MOTION_NOTIFY: {
			double d = fmax (fabs (e->x - mandel->center_x), fabs (e->y - mandel->center_y) * mandel->md->aspect);
			int oldx = mandel->center_x - mandel->selection_size;
			int oldy = mandel->center_y - mandel->selection_size / mandel->md->aspect;
			int oldw = 2 * mandel->selection_size + 1;
			int oldh = 2 * mandel->selection_size / mandel->md->aspect + 1;
			redraw_area (mandel, oldx, oldy, oldw, 1);
			redraw_area (mandel, oldx, oldy + oldh - 1, oldw, 1);
			redraw_area (mandel, oldx, oldy + 1, 1, oldh - 2);
			redraw_area (mandel, oldx + oldw - 1, oldy + 1, 1, oldh - 2);
			gdk_draw_rectangle (GDK_DRAWABLE (widget->window), mandel->frame_gc, false,
				mandel->center_x - d,
				mandel->center_y - d / mandel->md->aspect,
				2 * d,
				2 * d / mandel->md->aspect);
			mandel->selection_size = d;
			return TRUE;
		}
		default: {
			printf ("Other event!\n");
			return FALSE;
		}
	}
}


static gboolean
my_expose (GtkWidget *widget, GdkEventExpose *event, gpointer user_data)
{
	GtkMandel *mandel = GTK_MANDEL (widget);
	redraw_area (mandel, event->area.x, event->area.y, event->area.width, event->area.height);
	return true;
}


static void
gtk_mandel_display_pixel (unsigned x, unsigned y, unsigned iter, void *user_data)
{
	gtk_mandel_display_rect (x, y, 1, 1, iter, user_data);
}


static void
gtk_mandel_display_rect (unsigned x, unsigned y, unsigned w, unsigned h, unsigned iter, void *user_data)
{
	int xi, yi;

	GtkMandel *mandel = GTK_MANDEL (user_data);

	GdkColor *color = &mandelcolors[iter % COLORS]; /* FIXME why use GdkColor for mandelcolors? better define our own struct. */

	g_mutex_lock (mandel->pb_mutex);
	for (xi = x; xi < x + w; xi++)
		for (yi = y; yi < y + h; yi++) {
			guchar *p = mandel->pb_data + yi * mandel->pb_rowstride + xi * mandel->pb_nchan;
			p[0] = color->red >> 8;
			p[1] = color->green >> 8;
			p[2] = color->blue >> 8;
		}

	if (mandel->need_redraw) {
		if (x < mandel->pb_xmin)
			mandel->pb_xmin = x;
		if (y < mandel->pb_ymin)
			mandel->pb_ymin = y;
		if (x + w > mandel->pb_xmax)
			mandel->pb_xmax = x + w;
		if (y + h > mandel->pb_ymax)
			mandel->pb_ymax = y + h;
	} else {
		mandel->need_redraw = true;
		mandel->pb_xmin = x;
		mandel->pb_ymin = y;
		mandel->pb_xmax = x + w;
		mandel->pb_ymax = y + h;
	}

	g_mutex_unlock (mandel->pb_mutex);
}


static gpointer
calcmandel (gpointer data)
{
	struct mandeldata *md = (struct mandeldata *) data;
	GtkMandel *mandel = GTK_MANDEL (md->user_data);

	mandel->md = md;

	mandel_render (md);

	g_source_remove (mandel->redraw_source_id);
	g_idle_add (redraw_source_func_once, mandel);

	struct rendering_stopped_info *info = malloc (sizeof (struct rendering_stopped_info));
	info->mandel = mandel;
	info->completed = !md->terminate;
	g_idle_add (do_emit_rendering_stopped, info);

	return NULL;
}


GtkMandelArea *
gtk_mandel_area_new_from_file (const char *filename)
{
	FILE *f = fopen (filename, "r");
	GtkMandelArea *area = NULL;
	mpf_t cx, cy, magf;

	if (f == NULL)
		return NULL;

	mpf_init (cx);
	mpf_init (cy);
	mpf_init (magf);

	if (fread_coords_as_center (f, cx, cy, magf))
		area = gtk_mandel_area_new (cx, cy, magf);
	fclose (f);

	mpf_clear (cx);
	mpf_clear (cy);
	mpf_clear (magf);

	return area;
}


static void
size_allocate (GtkWidget *widget, GtkAllocation allocation, gpointer data)
{
	GtkMandel *mandel = GTK_MANDEL (widget);
	if (mandel->md != NULL) {
		gtk_mandel_restart_thread (mandel, mandel->md->cx, mandel->md->cy, mandel->md->magf, mandel->md->maxiter, mandel->md->render_method, mandel->md->log_factor);
	}
}


static gboolean
do_emit_rendering_started (gpointer data)
{
	struct rendering_started_info *info = (struct rendering_started_info *) data;
	GtkMandel *mandel = info->mandel;
	gulong bits = info->bits;
	free (info);
	g_signal_emit (G_OBJECT (mandel), GTK_MANDEL_GET_CLASS (mandel)->rendering_started_signal, 0, bits);
	return FALSE;
}


static gboolean
do_emit_rendering_stopped (gpointer data)
{
	struct rendering_stopped_info *info = (struct rendering_stopped_info *) data;
	GtkMandel *mandel = info->mandel;
	gboolean completed = info->completed;
	free (info);
	g_signal_emit (G_OBJECT (mandel), GTK_MANDEL_GET_CLASS (mandel)->rendering_stopped_signal, 0, completed);
	return FALSE;
}


void
gtk_mandel_redraw (GtkMandel *mandel)
{
	if (!mandel->need_redraw)
		return;
	redraw_area (mandel,
		mandel->pb_xmin, mandel->pb_ymin,
		mandel->pb_xmax - mandel->pb_xmin, mandel->pb_ymax - mandel->pb_ymin);
	mandel->need_redraw = false;
}


static gboolean
redraw_source_func (gpointer data)
{
	GtkMandel *mandel = GTK_MANDEL (data);
	gtk_mandel_redraw (mandel);
	return TRUE;
}


static gboolean
redraw_source_func_once (gpointer data)
{
	GtkMandel *mandel = GTK_MANDEL (data);
	gtk_mandel_redraw (mandel);
	return FALSE;
}


/*
 * This function does all kinds of boundary checking, so it can basically
 * be called for any area, will clip the specified area to the part of it
 * inside the widget area, and will not draw anything if called for a
 * completely off-screen area.
 */
static void
redraw_area (GtkMandel *mandel, int x, int y, int w, int h)
{
	if (mandel->pixbuf == NULL || mandel->md == NULL)
		return;
	if (w < 0 || h < 0 || x <= -w || y <= -h || x >= (int) mandel->md->w || y >= (int) mandel->md->h)
		return; /* area is completely off-screen */

	GtkWidget *widget = GTK_WIDGET (mandel);
	int my_x = x, my_y = y, my_w = w, my_h = h;
	if (my_x < 0)
		my_x = 0;
	if (my_y < 0)
		my_y = 0;
	if (mandel->md->w - my_x < my_w)
		my_w = mandel->md->w - my_x;
	if (mandel->md->h - my_y < my_h)
		my_h = mandel->md->h - my_y;
	g_mutex_lock (mandel->pb_mutex);
	gdk_draw_pixbuf (GDK_DRAWABLE (widget->window), mandel->gc, mandel->pixbuf, my_x, my_y, my_x, my_y, my_w, my_h, GDK_RGB_DITHER_NORMAL, 0, 0);
	g_mutex_unlock (mandel->pb_mutex);
}
