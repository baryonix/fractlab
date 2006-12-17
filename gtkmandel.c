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


static void gtk_mandel_display_pixel (unsigned x, unsigned y, unsigned iter, void *user_data);
static void gtk_mandel_display_rect (unsigned x, unsigned y, unsigned w, unsigned h, unsigned iter, void *user_data);
static gboolean mouse_event (GtkWidget *my_img, GdkEventButton *e, gpointer user_data);
static void my_realize (GtkWidget *my_img, gpointer user_data);
static void gtk_mandel_class_init (GtkMandelClass *class);
static void gtk_mandel_init (GtkMandel *mandel);
static gboolean my_expose (GtkWidget *widget, GdkEventExpose *event, gpointer user_data);
static gpointer *calcmandel (gpointer *data);




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


static void
gtk_mandel_class_init (GtkMandelClass *class)
{
	//GtkWidgetClass *widget_class = (GtkWidgetClass *) class;
	//widget_class->realize = my_realize;
	//widget_class->button_press_event = mouse_event;
}

static void
gtk_mandel_init (GtkMandel *mandel)
{
	g_signal_connect (G_OBJECT (mandel), "realize", (GCallback) my_realize, NULL);
	g_signal_connect (G_OBJECT (mandel), "button-press-event", (GCallback) mouse_event, NULL);
	g_signal_connect (G_OBJECT (mandel), "button-release-event", (GCallback) mouse_event, NULL);
	g_signal_connect (G_OBJECT (mandel), "motion-notify-event", (GCallback) mouse_event, NULL);
	g_signal_connect (G_OBJECT (mandel), "expose-event", (GCallback) my_expose, NULL);
}



void
gtk_mandel_restart_thread (GtkMandel *mandel, mpf_t xmin, mpf_t xmax, mpf_t ymin, mpf_t ymax, unsigned maxiter, render_method_t render_method)
{
	struct mandeldata *md = malloc (sizeof (struct mandeldata));
	mpf_init_set (md->xmin_f, xmin);
	mpf_init_set (md->xmax_f, xmax);
	mpf_init_set (md->ymin_f, ymin);
	mpf_init_set (md->ymax_f, ymax);
	md->maxiter = maxiter;
	md->user_data = mandel;
	md->render_method = render_method;
	md->join_me = mandel->thread;
	md->terminate = false;
	md->w = md->h = PIXELS;
	md->data = malloc (md->w * md->h * sizeof (unsigned));
	md->display_pixel = gtk_mandel_display_pixel;
	md->display_rect = gtk_mandel_display_rect;

	if (mandel->md != NULL)
		mandel->md->terminate = true;

	mandel->thread = g_thread_create ((GThreadFunc) calcmandel, (gpointer) md, true, NULL);
}

static void
my_realize (GtkWidget *my_img, gpointer user_data)
{
	GtkMandel *mandel = GTK_MANDEL (my_img);
	mandel->pixmap = gdk_pixmap_new (my_img->window, PIXELS, PIXELS, -1);
	mandel->gc = gdk_gc_new (GDK_DRAWABLE (my_img->window));
	mandel->frame_gc = gdk_gc_new (GDK_DRAWABLE (my_img->window));
	mandel->pm_gc = gdk_gc_new (GDK_DRAWABLE (mandel->pixmap));
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
mouse_event (GtkWidget *my_img, GdkEventButton *e, gpointer user_data)
{
	GtkMandel *mandel = GTK_MANDEL (my_img);
	if (e->type == GDK_BUTTON_PRESS) {
		printf ("* Button pressed, x=%f, y=%f\n", e->x, e->y);
		mandel->center_x = e->x;
		mandel->center_y = e->y;
		mandel->selection_size = 0.0;
		return TRUE;
	} else if (e->type == GDK_BUTTON_RELEASE) {
		printf ("* Button released!\n");
		mpf_t xmin, xmax, ymin, ymax, cx, cy, dx, dy;
		mpf_init (xmin);
		mpf_init (xmax);
		mpf_init (ymin);
		mpf_init (ymax);
		mpf_init (cx);
		mpf_init (cy);
		mpf_init (dx);
		mpf_init (dy);
		mandel_convert_x_f (mandel->md, cx, mandel->center_x);
		mandel_convert_y_f (mandel->md, cy, mandel->center_y);
		mandel_convert_x_f (mandel->md, dx, e->x);
		mandel_convert_y_f (mandel->md, dy, e->y);
		mpf_sub (dx, cx, dx);
		mpf_abs (dx, dx);
		mpf_sub (dy, cy, dy);
		mpf_abs (dy, dy);
		if (mpf_cmp (dx, dy) < 0)
			mpf_set (dx, dy);
		mpf_sub (xmin, cx, dx);
		mpf_add (xmax, cx, dx);
		mpf_sub (ymin, cy, dx);
		mpf_add (ymax, cy, dx);
		long xprec;
		mpf_get_d_2exp (&xprec, dx);
		/* We are using %f format, so the absolute difference between
		 * the min and max values dictates the required precision. */
		gmp_printf ("* xmin = %.*Ff\n", (int) (-xprec / 3.3219 + 5), xmin);
		gmp_printf ("* xmax = %.*Ff\n", (int) (-xprec / 3.3219 + 5), xmax);
		gmp_printf ("* ymin = %.*Ff\n", (int) (-xprec / 3.3219 + 5), ymin);
		gmp_printf ("* ymax = %.*Ff\n", (int) (-xprec / 3.3219 + 5), ymax);
		gtk_mandel_restart_thread (mandel, xmin, xmax, ymin, ymax, mandel->md->maxiter, mandel->md->render_method);
		return TRUE;
	} else if (e->type == GDK_MOTION_NOTIFY) {
		gdouble d = fmax (fabs (e->x - mandel->center_x), fabs (e->y - mandel->center_y));
		gdk_draw_drawable (GDK_DRAWABLE (my_img->window), mandel->gc, GDK_DRAWABLE (mandel->pixmap), mandel->center_x - mandel->selection_size, mandel->center_y - mandel->selection_size, mandel->center_x - mandel->selection_size, mandel->center_y - mandel->selection_size, 2 * mandel->selection_size + 1, 2 * mandel->selection_size + 1);
		gdk_draw_rectangle (GDK_DRAWABLE (my_img->window), mandel->frame_gc, false, mandel->center_x - d, mandel->center_y - d, 2 * d, 2 * d);
		mandel->selection_size = d;
		return true;
	} else {
		printf ("Other event!\n");
		return FALSE;
	}
}


static gboolean
my_expose (GtkWidget *widget, GdkEventExpose *event, gpointer user_data)
{
	GtkMandel *mandel = GTK_MANDEL (widget);
	gdk_draw_drawable (GDK_DRAWABLE (widget->window), mandel->gc,
		GDK_DRAWABLE (mandel->pixmap),
		event->area.x, event->area.y,
		event->area.x, event->area.y,
		event->area.width, event->area.height);
	return true;
}


static void
gtk_mandel_set_gc_color (GtkMandel *mandel, unsigned iter)
{
	gdk_gc_set_rgb_fg_color (mandel->pm_gc, &mandelcolors[iter % COLORS]);
	gdk_gc_set_rgb_fg_color (mandel->gc, &mandelcolors[iter % COLORS]);
}


static void
gtk_mandel_display_pixel (unsigned x, unsigned y, unsigned iter, void *user_data)
{
	GtkMandel *mandel = GTK_MANDEL (user_data);
	GtkWidget *widget = GTK_WIDGET (mandel);

	gdk_threads_enter ();
	gtk_mandel_set_gc_color (mandel, iter);
	gdk_draw_point (GDK_DRAWABLE (mandel->pixmap), mandel->pm_gc, x, y);
	gdk_draw_point (GDK_DRAWABLE (widget->window), mandel->gc, x, y);
	gdk_threads_leave ();
}


static void
gtk_mandel_display_rect (unsigned x, unsigned y, unsigned w, unsigned h, unsigned iter, void *user_data)
{
	GtkMandel *mandel = GTK_MANDEL (user_data);
	GtkWidget *widget = GTK_WIDGET (mandel);

	gdk_threads_enter ();
	gtk_mandel_set_gc_color (mandel, iter);
	gdk_draw_rectangle (GDK_DRAWABLE (widget->window), mandel->gc, true, x, y, w, h);
	gdk_draw_rectangle (GDK_DRAWABLE (mandel->pixmap), mandel->pm_gc, true, x, y, w, h);
	gdk_threads_leave ();
}


static gpointer *
calcmandel (gpointer *data)
{
	struct mandeldata *md = (struct mandeldata *) data;
	GtkMandel *mandel = GTK_MANDEL (md->user_data);
	GtkWidget *widget = GTK_WIDGET (mandel);

	if (md->join_me != NULL)
		g_thread_join (md->join_me);

	if (mandel->md != NULL) {
		mandel_free (mandel->md);
	}

	mandel->md = md;

	gdk_threads_enter ();
	gdk_gc_set_foreground (mandel->gc, &mandel->black);
	gdk_draw_rectangle (GDK_DRAWABLE (widget->window), mandel->gc, true, 0, 0, md->w, md->h);
	gdk_gc_set_foreground (mandel->pm_gc, &mandel->black);
	gdk_draw_rectangle (GDK_DRAWABLE (mandel->pixmap), mandel->pm_gc, true, 0, 0, md->w, md->h);
	gdk_threads_leave ();

	mandel_render (md);

	gdk_threads_enter ();
	gdk_flush ();
	gdk_threads_leave ();

	return NULL;
}


