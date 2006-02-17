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


#include "cmdline.h"
#include "file.h"
#include "fpdefs.h"
#include "mandelbrot.h"


#define COLORS 256
#define PIXELS 300


void gtk_mandel_display_pixel (unsigned x, unsigned y, unsigned iter, void *user_data);
void gtk_mandel_display_rect (unsigned x, unsigned y, unsigned w, unsigned h, unsigned iter, void *user_data);

#define GTK_MANDEL(obj) GTK_CHECK_CAST (obj, gtk_mandel_get_type (), GtkMandel)
#define GTK_MANDEL_CLASS(klass) GTK_CHECK_CLASS_CAST (klass, gtk_mandel_get_type (), GtkMandel)
#define GTK_IS_MANDEL(obj) GET_CHECK_TYPE (obj, gtk_mandel_get_type ())

gboolean mouse_event (GtkWidget *my_img, GdkEventButton *e, gpointer user_data);
void my_realize (GtkWidget *my_img, gpointer user_data);

typedef struct _GtkMandel GtkMandel;
typedef struct _GtkMandelClass GtkMandelClass;


struct _GtkMandel
{
	GtkDrawingArea widget;
	GdkPixmap *pixmap;
	GdkGC *gc, *pm_gc;
	GdkColor black;
	GThread *thread;
	struct mandeldata *md;
	gdouble center_x, center_y;
};


struct _GtkMandelClass
{
	GtkDrawingAreaClass parent_class;
};


static GdkColor mandelcolors[COLORS];


GType gtk_mandel_get_type ();

GtkWidget *
gtk_mandel_new (void)
{
	GtkMandel *mandel = g_object_new (gtk_mandel_get_type (), NULL);
	return GTK_WIDGET (mandel);
}

static void gtk_mandel_class_init (GtkMandelClass *class);
static void gtk_mandel_init (GtkMandel *mandel);

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

gboolean my_expose (GtkWidget *widget, GdkEventExpose *event, gpointer user_data);

static void
gtk_mandel_init (GtkMandel *mandel)
{
	gtk_signal_connect (GTK_OBJECT (mandel), "realize", (GtkSignalFunc) my_realize, NULL);
	gtk_signal_connect (GTK_OBJECT (mandel), "button-press-event", (GtkSignalFunc) mouse_event, NULL);
	gtk_signal_connect (GTK_OBJECT (mandel), "button-release-event", (GtkSignalFunc) mouse_event, NULL);
	gtk_signal_connect (GTK_OBJECT (mandel), "motion-notify-event", (GtkSignalFunc) mouse_event, NULL);
	gtk_signal_connect (GTK_OBJECT (mandel), "expose-event", (GtkSignalFunc) my_expose, NULL);
}

gpointer *calcmandel (gpointer *data);


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

void
my_realize (GtkWidget *my_img, gpointer user_data)
{
	GtkMandel *mandel = GTK_MANDEL (my_img);
	mandel->pixmap = gdk_pixmap_new (my_img->window, PIXELS, PIXELS, -1);
	mandel->gc = gdk_gc_new (GDK_DRAWABLE (my_img->window));
	mandel->pm_gc = gdk_gc_new (GDK_DRAWABLE (mandel->pixmap));
	gtk_widget_add_events (my_img, GDK_BUTTON_PRESS_MASK |
		GDK_BUTTON_RELEASE_MASK | /* GDK_BUTTON1_MOTION_MASK | */
		GDK_EXPOSURE_MASK);
	GdkColormap *cmap = gdk_colormap_get_system ();
	gdk_color_parse ("black", &mandel->black);
	gdk_color_alloc (cmap, &mandel->black);

	mandel->thread = NULL;
}


void
my_mpz_to_mpf (mpf_t rop, mpz_t op, unsigned frac_limbs)
{
	mpf_set_z (rop, op);
	mpf_div_2exp (rop, rop, frac_limbs * mp_bits_per_limb);
}


gboolean
mouse_event (GtkWidget *my_img, GdkEventButton *e, gpointer user_data)
{
	GtkMandel *mandel = GTK_MANDEL (my_img);
	if (e->type == GDK_BUTTON_PRESS) {
		printf ("* Button pressed, x=%f, y=%f\n", e->x, e->y);
		mandel->center_x = e->x;
		mandel->center_y = e->y;
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
		if (mpf_cmp (dx, dy) > 0) {
			mpf_sub (xmin, cx, dx);
			mpf_add (xmax, cx, dx);
			mpf_sub (ymin, cy, dx);
			mpf_add (ymax, cy, dx);
		} else {
			mpf_sub (xmin, cx, dy);
			mpf_add (xmax, cx, dy);
			mpf_sub (ymin, cy, dy);
			mpf_add (ymax, cy, dy);
		}
		gmp_printf ("* xmin = %.Ff\n", xmin);
		gmp_printf ("* xmax = %.Ff\n", xmax);
		gmp_printf ("* ymin = %.Ff\n", ymin);
		gmp_printf ("* ymax = %.Ff\n", ymax);
		gtk_mandel_restart_thread (mandel, xmin, xmax, ymin, ymax, mandel->md->maxiter, mandel->md->render_method);
		return TRUE;
	} else {
		printf ("Other event!\n");
		return FALSE;
	}
}


gboolean
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


void
gtk_mandel_set_gc_color (GtkMandel *mandel, unsigned iter)
{
	gdk_gc_set_rgb_fg_color (mandel->pm_gc, &mandelcolors[iter % COLORS]);
	gdk_gc_set_rgb_fg_color (mandel->gc, &mandelcolors[iter % COLORS]);
}


void
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


void
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


gpointer *
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


void
new_maxiter (GtkWidget *widget, gpointer *data)
{
	GtkMandel *mandel = GTK_MANDEL (data);
	int i = atoi (gtk_entry_get_text (GTK_ENTRY (widget)));
	if (i > 0)
		gtk_mandel_restart_thread (mandel, mandel->md->xmin_f, mandel->md->xmax_f, mandel->md->ymin_f, mandel->md->ymax_f, i, mandel->md->render_method);
}


int
main (int argc, char **argv)
{
	g_thread_init (NULL);
	gdk_threads_init ();
	gdk_threads_enter ();

	//unsigned frac_limbs = 5, total_limbs = INT_LIMBS + frac_limbs;
	parse_command_line (&argc, &argv);

	mpf_set_default_prec (1024);

	int i;
	for (i = 0; i < COLORS; i++) {
		mandelcolors[i].red = (guint16) (sin (2 * M_PI * i / COLORS) * 32767) + 32768;
		mandelcolors[i].green = (guint16) (sin (4 * M_PI * i / COLORS) * 32767) + 32768;
		mandelcolors[i].blue = (guint16) (sin (6 * M_PI * i / COLORS) * 32767) + 32768;
	}

	GtkWidget *win, *img;
	gtk_init (&argc, &argv);

	GtkWidget *hbox = gtk_hbox_new (false, 5);
	GtkWidget *maxiter_label = gtk_label_new ("maxiter:");
	gtk_container_add (GTK_CONTAINER (hbox), maxiter_label);
	GtkWidget *maxiter_entry = gtk_entry_new ();
	gtk_container_add (GTK_CONTAINER (hbox), maxiter_entry);

	GtkWidget *vbox = gtk_vbox_new (false, 5);
	gtk_container_add (GTK_CONTAINER (vbox), hbox);

	win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	img = gtk_mandel_new ();
	gtk_widget_set_size_request (img, PIXELS, PIXELS);
	gtk_container_add (GTK_CONTAINER (vbox), img);
	gtk_container_add (GTK_CONTAINER (win), vbox);

	gtk_signal_connect (GTK_OBJECT (maxiter_entry), "activate", (GtkSignalFunc) new_maxiter, (gpointer) img);

	gtk_widget_show_all (win);

	mpf_t xmin, xmax, ymin, ymax;
	mpf_init (xmin);
	mpf_init (xmax);
	mpf_init (ymin);
	mpf_init (ymax);

	bool coords_ok;

	if (option_center_coords != NULL)
		coords_ok = read_cmag_coords_from_file (option_center_coords, xmin, xmax, ymin, ymax);
	else if (option_corner_coords != NULL)
		coords_ok = read_corner_coords_from_file (option_corner_coords, xmin, xmax, ymin, ymax);
	else {
		fprintf (stderr, "No start coordinates specified.\n");
		exit (2);
	}

	if (!coords_ok) {
		perror ("reading coordinates file");
		exit (3);
	}

	render_method_t render_method = RM_SUCCESSIVE_REFINE;

	if (option_successive_refine)
		render_method = RM_SUCCESSIVE_REFINE;
	else if (option_mariani_silver)
		render_method = RM_MARIANI_SILVER;

	gtk_mandel_restart_thread (GTK_MANDEL (img), xmin, xmax, ymin, ymax, 1000, render_method);
	gtk_main ();
	gdk_threads_leave ();

	return 0;
}
