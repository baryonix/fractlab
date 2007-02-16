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
#include "fractal-render.h"
#include "defs.h"
#include "file.h"
#include "util.h"


struct rendering_started_info {
	GtkMandel *mandel;
	int bits;
};

struct rendering_stopped_info {
	GtkMandel *mandel;
	bool completed;
};


typedef gboolean mouse_handler_t (GtkWidget *, GdkEvent *, gpointer);


static void gtk_mandel_display_pixel (unsigned x, unsigned y, unsigned iter, void *user_data);
static void gtk_mandel_display_rect (unsigned x, unsigned y, unsigned w, unsigned h, unsigned iter, void *user_data);
static gboolean mouse_event (GtkWidget *widget, GdkEvent *e, gpointer user_data);
static gboolean select_area_mouse_handler (GtkWidget *widget, GdkEvent *e, gpointer user_data);
static gboolean select_point_mouse_handler (GtkWidget *widget, GdkEvent *e, gpointer user_data);
static void my_realize (GtkWidget *my_img, gpointer user_data);
static void gtk_mandel_class_init (GtkMandelClass *class);
static void gtk_mandel_init (GtkMandel *mandel);
static gboolean my_expose (GtkWidget *widget, GdkEventExpose *event, gpointer user_data);
static gpointer calcmandel (gpointer data);
static void size_allocate (GtkWidget *widget, GtkAllocation *allocation, gpointer data);
static gboolean do_emit_rendering_started (gpointer data);
static gboolean do_emit_rendering_stopped (gpointer data);
static gboolean redraw_source_func (gpointer data);
static gboolean redraw_source_func_once (gpointer data);
static void redraw_area (GtkMandel *mandel, int x, int y, int w, int h);
static void init_renderer (GtkMandel *mandel);
static void update_selection_cursor (GtkMandel *mandel);
static void gtk_mandel_dispose (GObject *object);
static void gtk_mandel_finalize (GObject *object);


mouse_handler_t *mouse_handlers[] = {
	NULL,
	select_area_mouse_handler,
	select_point_mouse_handler
};

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
			.class_size		= sizeof (GtkMandelClass),
			.class_init		= (GClassInitFunc) gtk_mandel_class_init,
			.instance_size	= sizeof (GtkMandel),
			.instance_init	= (GInstanceInitFunc) gtk_mandel_init
		};
		mandel_type = g_type_register_static (GTK_TYPE_DRAWING_AREA, "GtkMandel", &mandel_info, 0);
	}

	return mandel_type;
}


static void
gtk_mandel_class_init (GtkMandelClass *g_class)
{
	G_OBJECT_CLASS (g_class)->dispose = gtk_mandel_dispose;
	G_OBJECT_CLASS (g_class)->finalize = gtk_mandel_finalize;

	g_class->area_selected_signal = g_signal_new (
		"area-selected",
		G_TYPE_FROM_CLASS (g_class),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE,
		1,
		G_TYPE_POINTER
	);
	g_class->point_selected_signal = g_signal_new (
		"point-selected",
		G_TYPE_FROM_CLASS (g_class),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE,
		1,
		G_TYPE_POINTER
	);
	g_class->rendering_started_signal = g_signal_new (
		"rendering-started",
		G_TYPE_FROM_CLASS (g_class),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__ULONG,
		G_TYPE_NONE,
		1,
		G_TYPE_ULONG
	);
	g_class->rendering_progress_signal = g_signal_new (
		"rendering-progress",
		G_TYPE_FROM_CLASS (g_class),
		G_SIGNAL_RUN_LAST,
		0, NULL, NULL,
		g_cclosure_marshal_VOID__DOUBLE,
		G_TYPE_NONE,
		1,
		G_TYPE_DOUBLE
	);
	g_class->rendering_stopped_signal = g_signal_new (
		"rendering-stopped",
		G_TYPE_FROM_CLASS (g_class),
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
	GtkWidget *widget = GTK_WIDGET (mandel);

	mandel->disposed = false;

	mandel->render_method = RM_SUCCESSIVE_REFINE;
	mandel->thread_count = 1;
	mandel->md = NULL;
	mandel->renderer = NULL;
	mandel->pixbuf = NULL;
	mandel->gc = NULL;
	mandel->thread = NULL;
	mandel->realized = false;

	mandel->selection_type = GTK_MANDEL_SELECT_NONE;
	mandel->cur_w = -1;
	mandel->cur_h = -1;
	mandel->selection_active = false;

	/* initialize aspect to some arbitrary value, so we don't get div-by-zero
	 * errors at initialization time */
	mandel->aspect = 1.0;

	mandel->need_redraw = false;
	mandel->pb_mutex = g_mutex_new ();

	gtk_widget_set_events (widget, GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_BUTTON1_MOTION_MASK);

	g_signal_connect (G_OBJECT (mandel), "realize", (GCallback) my_realize, NULL);
	g_signal_connect (G_OBJECT (mandel), "button-press-event", (GCallback) mouse_event, NULL);
	g_signal_connect (G_OBJECT (mandel), "button-release-event", (GCallback) mouse_event, NULL);
	g_signal_connect (G_OBJECT (mandel), "motion-notify-event", (GCallback) mouse_event, NULL);
	g_signal_connect (G_OBJECT (mandel), "expose-event", (GCallback) my_expose, NULL);
	g_signal_connect (G_OBJECT (mandel), "size-allocate", (GCallback) size_allocate, NULL);
}


void
gtk_mandel_stop (GtkMandel *mandel)
{
	if (mandel->thread != NULL) {
		mandel->renderer->terminate = true;
		g_thread_join (mandel->thread);
		mandel->thread = NULL;
	}
}


static void
init_renderer (GtkMandel *mandel)
{
	if (mandel->renderer != NULL)
	{
		mandel_renderer_clear (mandel->renderer);
		free (mandel->renderer);
		mandel->renderer = NULL;
	}

	GtkWidget *widget = GTK_WIDGET (mandel);

	struct mandel_renderer *renderer = malloc (sizeof (*renderer));
	mandel_renderer_init (renderer, mandel->md, mandel->cur_w, mandel->cur_h);
	renderer->render_method = mandel->render_method;
	renderer->thread_count = mandel->thread_count;
	renderer->user_data = mandel;
	renderer->display_pixel = gtk_mandel_display_pixel;
	renderer->display_rect = gtk_mandel_display_rect;
	mandel->renderer = renderer;

	/* Clear image */
	if (mandel->pixbuf != NULL)
		gdk_pixbuf_fill (mandel->pixbuf, 0);
	if (mandel->gc != NULL) {
		gdk_gc_set_foreground (mandel->gc, &mandel->black);
		gdk_draw_rectangle (GDK_DRAWABLE (widget->window), mandel->gc, true, 0, 0, mandel->cur_w, mandel->cur_h);
	}
}


void
gtk_mandel_start (GtkMandel *mandel)
{
	gtk_mandel_stop (mandel);
	init_renderer (mandel);

	/*
	 * Emit the "rendering-started" signal.
	 * We enqueue it to the main loop instead of calling g_signal_emit()
	 * directly from here. This is required because the "rendering-stopped"
	 * signal has been enqueued in the same way, and "rendering-started"
	 * must be emitted _after_ it.
	 */
	struct rendering_started_info *info = malloc (sizeof (struct rendering_started_info));
	info->mandel = mandel;
	info->bits = mandel_get_precision (mandel->renderer);
	g_idle_add (do_emit_rendering_started, info);

	mandel->redraw_source_id = g_timeout_add (500, redraw_source_func, mandel);

	GError *thread_err;
	mandel->thread = g_thread_create (calcmandel, (gpointer) mandel->renderer, true, &thread_err);
	if (mandel->thread == NULL) {
		fprintf (stderr, "* BUG: g_thread_create() error: %s\n", thread_err->message);
		g_error_free (thread_err);
	}
}


static void
my_realize (GtkWidget *widget, gpointer user_data)
{
	GtkMandel *mandel = GTK_MANDEL (widget);
	mandel->gc = gdk_gc_new (GDK_DRAWABLE (widget->window));
	mandel->frame_gc = gdk_gc_new (GDK_DRAWABLE (widget->window));
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

	GdkDisplay *disp = gtk_widget_get_display (widget);
	mandel->crosshair = gdk_cursor_new_for_display (disp, GDK_CROSSHAIR);
	/*mandel->left_cursor = gdk_cursor_new_for_display (disp, GDK_LEFT_SIDE);
	mandel->right_cursor = gdk_cursor_new_for_display (disp, GDK_RIGHT_SIDE);
	mandel->top_cursor = gdk_cursor_new_for_display (disp, GDK_TOP_SIDE);
	mandel->bottom_cursor = gdk_cursor_new_for_display (disp, GDK_BOTTOM_SIDE);*/
	mandel->left_cursor = gdk_cursor_new_for_display (disp, GDK_SB_RIGHT_ARROW);
	mandel->right_cursor = gdk_cursor_new_for_display (disp, GDK_SB_LEFT_ARROW);
	mandel->top_cursor = gdk_cursor_new_for_display (disp, GDK_SB_DOWN_ARROW);
	mandel->bottom_cursor = gdk_cursor_new_for_display (disp, GDK_SB_UP_ARROW);

	/* XXX don't do this here once we have different selection modes implemented */
	update_selection_cursor (mandel);

	mandel->realized = true;
}


static gboolean
mouse_event (GtkWidget *widget, GdkEvent *e, gpointer user_data)
{
	GtkMandel *mandel = GTK_MANDEL (widget);
	mouse_handler_t *handler = mouse_handlers[mandel->selection_type];
	if (handler != NULL)
		return handler (widget, e, user_data);
	else
		return FALSE;
}


static gboolean
select_area_mouse_handler (GtkWidget *widget, GdkEvent *e, gpointer user_data)
{
	GtkMandel *mandel = GTK_MANDEL (widget);
	switch (e->type) {
		case GDK_BUTTON_PRESS: {
			if (e->button.button != 1)
				return FALSE;
			mandel->center_x = e->button.x;
			mandel->center_y = e->button.y;
			mandel->selection_size = 0.0;
			mandel->selection_active = true;
			return TRUE;
		}
		case GDK_BUTTON_RELEASE: {
			if (e->button.button != 1)
				return FALSE;
			if (!mandel->selection_active)
				return TRUE;
			mandel->selection_active = false;
			update_selection_cursor (mandel);
			if (mandel->center_x == e->button.x && mandel->center_y == e->button.y)
				return TRUE; /* avoid zero size selections */
			mpf_t cx, cy, dx, dy, mpaspect;
			mpf_init (cx);
			mpf_init (cy);
			mpf_init (dx);
			mpf_init (dy);
			mpf_init (mpaspect);
			mpf_set_d (mpaspect, mandel->aspect);
			mandel_convert_x_f (mandel->renderer, cx, mandel->center_x);
			mandel_convert_y_f (mandel->renderer, cy, mandel->center_y);
			mandel_convert_x_f (mandel->renderer, dx, e->button.x);
			mandel_convert_y_f (mandel->renderer, dy, e->button.y);
			mpf_sub (dx, cx, dx);
			mpf_abs (dx, dx);
			mpf_sub (dy, cy, dy);
			mpf_abs (dy, dy);
			if (mandel->aspect > 1.0)
				mpf_div (dx, dx, mpaspect);
			else
				mpf_mul (dy, dy, mpaspect);
			if (mpf_cmp (dx, dy) < 0)
				mpf_set (dx, dy);
			struct mandel_area area[1];
			mandel_area_init (area);
			mpf_set (area->center.real, cx);
			mpf_set (area->center.imag, cy);
			mpf_ui_div (area->magf, 1, dx);
			mpf_clear (cx);
			mpf_clear (cy);
			mpf_clear (dx);
			mpf_clear (dy);
			mpf_clear (mpaspect);
			g_signal_emit (mandel, GTK_MANDEL_GET_CLASS (mandel)->area_selected_signal, 0, area);
			mandel_area_clear (area);
			return TRUE;
		}
		case GDK_MOTION_NOTIFY: {
			if (!mandel->selection_active)
				return TRUE;
			GdkCursor *cursor;
			double d, dx = e->motion.x - mandel->center_x, dy = (e->motion.y - mandel->center_y) * mandel->aspect, dxabs = fabs (dx), dyabs = fabs (dy);
			if (dxabs > dyabs) {
				d = dxabs;
				if (dx > 0.0)
					cursor = mandel->right_cursor;
				else
					cursor = mandel->left_cursor;
			} else {
				d = dyabs;
				if (dy > 0.0)
					cursor = mandel->bottom_cursor;
				else
					cursor = mandel->top_cursor;
			}
			gdk_window_set_cursor (widget->window, cursor);
			int oldx = mandel->center_x - mandel->selection_size;
			int oldy = mandel->center_y - mandel->selection_size / mandel->aspect;
			int oldw = 2 * mandel->selection_size + 1;
			int oldh = 2 * mandel->selection_size / mandel->aspect + 1;
			redraw_area (mandel, oldx, oldy, oldw, 1);
			redraw_area (mandel, oldx, oldy + oldh - 1, oldw, 1);
			redraw_area (mandel, oldx, oldy + 1, 1, oldh - 2);
			redraw_area (mandel, oldx + oldw - 1, oldy + 1, 1, oldh - 2);
			gdk_draw_rectangle (GDK_DRAWABLE (widget->window), mandel->frame_gc, false,
				mandel->center_x - d,
				mandel->center_y - d / mandel->aspect,
				2 * d,
				2 * d / mandel->aspect);
			mandel->selection_size = d;
			return TRUE;
		}
		default: {
			/* We don't care for double and triple clicks. */
			return FALSE;
		}
	}
}


static gboolean
select_point_mouse_handler (GtkWidget *widget, GdkEvent *e, gpointer user_data)
{
	if (e->type != GDK_BUTTON_RELEASE || e->button.button != 1)
		return FALSE;
	GtkMandel *mandel = GTK_MANDEL (widget);
	struct mandel_point point[1];
	mandel_point_init (point);
	mandel_convert_x_f (mandel->renderer, point->real, round (e->button.x));
	mandel_convert_y_f (mandel->renderer, point->imag, round (e->button.y));
	g_signal_emit (mandel, GTK_MANDEL_GET_CLASS (mandel)->point_selected_signal, 0, point);
	mandel_point_clear (point);
	return TRUE;
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
	struct mandel_renderer *renderer = (struct mandel_renderer *) data;
	GtkMandel *mandel = GTK_MANDEL (renderer->user_data);

	mandel_render (renderer);

	if (!g_source_remove (mandel->redraw_source_id))
		fprintf (stderr, "* BUG: g_source_remove failed for source %u\n", (unsigned) mandel->redraw_source_id);

	g_idle_add (redraw_source_func_once, mandel);

	struct rendering_stopped_info *info = malloc (sizeof (struct rendering_stopped_info));
	info->mandel = mandel;
	info->completed = !renderer->terminate;
	g_idle_add (do_emit_rendering_stopped, info);

	return NULL;
}


static void
size_allocate (GtkWidget *widget, GtkAllocation *allocation, gpointer data)
{
	GtkMandel *mandel = GTK_MANDEL (widget);
	gtk_mandel_stop (mandel);

	if (mandel->pixbuf == NULL || allocation->width != mandel->cur_w || allocation->height != mandel->cur_h) {
		if (mandel->pixbuf != NULL)
			g_object_unref (G_OBJECT (mandel->pixbuf));
		mandel->cur_w = allocation->width;
		mandel->cur_h = allocation->height;
		mandel->aspect = (double) mandel->cur_w / mandel->cur_h;
		mandel->pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, mandel->cur_w, mandel->cur_h);
		mandel->pb_rowstride = gdk_pixbuf_get_rowstride (mandel->pixbuf);
		mandel->pb_nchan = gdk_pixbuf_get_n_channels (mandel->pixbuf);
		mandel->pb_data = gdk_pixbuf_get_pixels (mandel->pixbuf);
	}

	if (mandel->realized && mandel->md != NULL)
		gtk_mandel_start (mandel);
}


void
gtk_mandel_set_mandeldata (GtkMandel *mandel, const struct mandeldata *md)
{
	gtk_mandel_stop (mandel);
	mandel->md = md;
	// XXX init_renderer (mandel);
}


void
gtk_mandel_set_render_method (GtkMandel *mandel, render_method_t render_method)
{
	mandel->render_method = render_method;
}


void
gtk_mandel_set_thread_count (GtkMandel *mandel, unsigned thread_count)
{
	mandel->thread_count = thread_count;
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
	g_signal_emit (mandel, GTK_MANDEL_GET_CLASS (mandel)->rendering_progress_signal, 0, (gdouble) mandel_renderer_progress (mandel->renderer));
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
	if (w < 0 || h < 0 || x <= -w || y <= -h || x >= (int) mandel->cur_w || y >= (int) mandel->cur_h)
		return; /* area is completely off-screen */

	GtkWidget *widget = GTK_WIDGET (mandel);
	int my_x = x, my_y = y, my_w = w, my_h = h;
	if (my_x < 0)
		my_x = 0;
	if (my_y < 0)
		my_y = 0;
	if (mandel->cur_w - my_x < my_w)
		my_w = mandel->cur_w - my_x;
	if (mandel->cur_h - my_y < my_h)
		my_h = mandel->cur_h - my_y;
	g_mutex_lock (mandel->pb_mutex);
	gdk_draw_pixbuf (GDK_DRAWABLE (widget->window), mandel->gc, mandel->pixbuf, my_x, my_y, my_x, my_y, my_w, my_h, GDK_RGB_DITHER_NORMAL, 0, 0);
	g_mutex_unlock (mandel->pb_mutex);
}


void
gtk_mandel_set_selection_type (GtkMandel *mandel, GtkMandelSelectionType selection_type)
{
	mandel->selection_type = selection_type;
	if (mandel->realized)
		update_selection_cursor (mandel);
}


static void
update_selection_cursor (GtkMandel *mandel)
{
	GdkCursor *cursor;
	switch (mandel->selection_type) {
		case GTK_MANDEL_SELECT_NONE:
			cursor = NULL;
			break;
		case GTK_MANDEL_SELECT_AREA:
		case GTK_MANDEL_SELECT_POINT:
			cursor = mandel->crosshair;
			break;
		default:
			fprintf (stderr, "* Invalid selection type %d\n", (int) mandel->selection_type);
			return;
	}
	gdk_window_set_cursor (GTK_WIDGET (mandel)->window, cursor);
}


double
gtk_mandel_get_progress (GtkMandel *mandel)
{
	if (mandel->renderer != NULL)
		return mandel_renderer_progress (mandel->renderer);
	else
		return 0.0;
}


static void
gtk_mandel_dispose (GObject *object)
{
	fprintf (stderr, "* DEBUG: disposing GtkMandel\n");
	GtkMandel *mandel = GTK_MANDEL (object);
	if (!mandel->disposed) {
		gtk_mandel_stop (mandel);
		my_g_object_unref_not_null (mandel->pixbuf);
		my_g_object_unref_not_null (mandel->gc);
		my_g_object_unref_not_null (mandel->frame_gc);
		gdk_cursor_unref (mandel->crosshair);
		gdk_cursor_unref (mandel->left_cursor);
		gdk_cursor_unref (mandel->right_cursor);
		gdk_cursor_unref (mandel->top_cursor);
		gdk_cursor_unref (mandel->bottom_cursor);
		mandel->disposed = true;
	}
	G_OBJECT_CLASS (g_type_class_peek_parent (G_OBJECT_GET_CLASS (object)))->dispose (object);
}


static void
gtk_mandel_finalize (GObject *object)
{
	fprintf (stderr, "* DEBUG: finalizing GtkMandel\n");
	GtkMandel *mandel = GTK_MANDEL (object);
	g_mutex_free (mandel->pb_mutex);
	if (mandel->renderer != NULL) {
		mandel_renderer_clear (mandel->renderer);
		free (mandel->renderer);
	}
	G_OBJECT_CLASS (g_type_class_peek_parent (G_OBJECT_GET_CLASS (object)))->finalize (object);
}
