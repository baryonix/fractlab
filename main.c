#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

#include <unistd.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include <gmp.h>


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
	GdkColor col0, col1, black;
	double xmin, xmax, ymin, ymax;
	unsigned w, h, maxiter;
	unsigned *data;
	GThread *thread;
	struct mandeldata *md;
	gdouble center_x, center_y;
};


struct _GtkMandelClass
{
	GtkDrawingAreaClass parent_class;
};


struct mandeldata {
	double xmin, xmax, ymin, ymax;
	unsigned maxiter;
	GtkWidget *widget;
	GThread *join_me;
	volatile bool terminate;
};


#define COLORS 256
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
	GtkWidgetClass *widget_class = (GtkWidgetClass *) class;
	//widget_class->realize = my_realize;
	//widget_class->button_press_event = mouse_event;
}

gboolean my_expose (GtkWidget *widget, GdkEventExpose *event, gpointer user_data);

static void
gtk_mandel_init (GtkMandel *mandel)
{
	gtk_signal_connect (mandel, "realize", my_realize, NULL);
	printf ("* realize signal connected.\n");
	gtk_signal_connect (mandel, "button-press-event", mouse_event, NULL);
	printf ("* button-press-event signal connected.\n");
	gtk_signal_connect (mandel, "button-release-event", mouse_event, NULL);
	printf ("* button-release-event signal connected.\n");
	gtk_signal_connect (mandel, "motion-notify-event", mouse_event, NULL);
	printf ("* motion-notify-event signal connected.\n");
	gtk_signal_connect (mandel, "expose-event", my_expose, NULL);
	printf ("* expose-event signal connected.\n");

	mandel->w = mandel->h = 500;
	mandel->maxiter = 10000;
}

gpointer * calcmandel (gpointer *data);


void
gtk_mandel_restart_thread (GtkWidget *widget, double xmin, double xmax, double ymin, double ymax, unsigned maxiter)
{
	GtkMandel *mandel = GTK_MANDEL (widget);
	struct mandeldata *md = malloc (sizeof (struct mandeldata));
	md->xmin = xmin;
	md->xmax = xmax;
	md->ymin = ymin;
	md->ymax = ymax;
	md->maxiter = maxiter;
	md->widget = widget;
	md->join_me = mandel->thread;
	md->terminate = false;

	if (mandel->md != NULL)
		mandel->md->terminate = true;

	mandel->md = md;
	mandel->thread = g_thread_create (calcmandel, (gpointer) md, true, NULL);
}

void
my_realize (GtkWidget *my_img, gpointer user_data)
{
	GtkMandel *mandel = GTK_MANDEL (my_img);
	printf ("* realize signal triggered: win=%p\n", my_img->window);
	mandel->pixmap = gdk_pixmap_new (my_img->window, 500, 500, -1);
	printf ("* Pixmap created: %p\n", mandel->pixmap);
	mandel->gc = gdk_gc_new (GDK_DRAWABLE (my_img->window));
	mandel->pm_gc = gdk_gc_new (GDK_DRAWABLE (mandel->pixmap));
	gtk_widget_add_events (my_img, GDK_BUTTON_PRESS_MASK |
		GDK_BUTTON_RELEASE_MASK | /* GDK_BUTTON1_MOTION_MASK | */
		GDK_EXPOSURE_MASK);
	GdkColormap *cmap = gdk_colormap_get_system ();
	gdk_color_parse ("black", &mandel->black);
	gdk_color_alloc (cmap, &mandel->black);
	gdk_color_parse ("red", &mandel->col0);
	gdk_color_alloc (cmap, &mandel->col0);
	gdk_color_parse ("green", &mandel->col1);
	gdk_color_alloc (cmap, &mandel->col1);

	mandel->data = malloc (mandel->w * mandel->h * sizeof (unsigned));

	mandel->thread = NULL;

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
		double cx = mandel->center_x * (mandel->xmax - mandel->xmin) / mandel->w + mandel->xmin;
		double cy = mandel->center_y * (mandel->ymin - mandel->ymax) / mandel->h + mandel->ymax;
		double curx = e->x * (mandel->xmax - mandel->xmin) / mandel->w + mandel->xmin;
		double cury = e->y * (mandel->ymin - mandel->ymax) / mandel->h + mandel->ymax;
		double dist = fmax (fabs (cx - curx), fabs (cy - cury));
		gtk_mandel_restart_thread (my_img, cx - dist, cx + dist, cy - dist, cy + dist, mandel->maxiter);
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
gtk_mandel_set_gc_color (GtkWidget *widget, unsigned iter)
{
	GtkMandel *mandel = GTK_MANDEL (widget);
	gdk_gc_set_rgb_fg_color (mandel->pm_gc, &mandelcolors[iter % COLORS]);
	gdk_gc_set_rgb_fg_color (mandel->gc, &mandelcolors[iter % COLORS]);
}


void
gtk_mandel_set_pixel (GtkWidget *widget, int x, int y, unsigned iter)
{
	GtkMandel *mandel = GTK_MANDEL (widget);

	mandel->data[x * mandel->h + y] = iter;

	gtk_mandel_set_gc_color (widget, iter);

	gdk_draw_point (GDK_DRAWABLE (mandel->pixmap), mandel->pm_gc, x, y);
	gdk_draw_point (GDK_DRAWABLE (widget->window), mandel->gc, x, y);
}


unsigned
gtk_mandel_get_pixel (GtkWidget *widget, int x, int y)
{
	GtkMandel *mandel = GTK_MANDEL (widget);
	return mandel->data[x * mandel->h + y];
}


// Both of these must be even numbers, and TOTAL_LIMBS > FRAC_LIMBS.
#define TOTAL_LIMBS 10
#define FRAC_LIMBS 8

void
my_mpn_mul_fast (mp_limb_t *p, mp_limb_t *f0, mp_limb_t *f1)
{
	mpn_mul_n (p, f0 + FRAC_LIMBS / 2, f1 + FRAC_LIMBS / 2, TOTAL_LIMBS / 2);
}

bool
my_mpn_add_signed (mp_limb_t *rop, mp_limb_t *op1, bool op1_sign, mp_limb_t *op2, bool op2_sign)
{
	if (op1_sign == op2_sign) {
		mpn_add_n (rop, op1, op2, TOTAL_LIMBS);
		return op1_sign;
	} else {
		if (mpn_cmp (op1, op2, TOTAL_LIMBS) > 0) {
			mpn_sub_n (rop, op1, op2, TOTAL_LIMBS);
			return op1_sign;
		} else {
			mpn_sub_n (rop, op2, op1, TOTAL_LIMBS);
			return op2_sign;
		}
	}
}

unsigned iter_saved = 0;

void
my_double_to_mpz (mpz_t rop, double op)
{
	int expo;

	op = frexp (op, &expo);
	mpz_set_d (rop, ldexp (op, 64));
	expo = FRAC_LIMBS * mp_bits_per_limb - 64 + expo;
	if (expo > 0)
		mpz_mul_2exp (rop, rop, expo);
	else if (expo < 0)
		mpz_tdiv_q_2exp (rop, rop, -expo);
}


unsigned
mandelbrot (double x0f, double y0f, unsigned maxiter)
{
	mp_limb_t x[TOTAL_LIMBS], y[TOTAL_LIMBS], x0[TOTAL_LIMBS], y0[TOTAL_LIMBS], xsqr[TOTAL_LIMBS], ysqr[TOTAL_LIMBS], sqrsum[TOTAL_LIMBS], four[TOTAL_LIMBS];
	mp_limb_t cd_x[TOTAL_LIMBS], cd_y[TOTAL_LIMBS];
	mpz_t ztmp;
	unsigned i;

	for (i = 0; i < TOTAL_LIMBS; i++)
		four[i] = 0;
	four[FRAC_LIMBS] = 4;

	mpz_init (ztmp);
	my_double_to_mpz (ztmp, x0f);
	bool x0_sign = x0f < 0.0;
	for (i = 0; i < TOTAL_LIMBS; i++)
		x0[i] = x[i] = cd_x[i] = mpz_getlimbn (ztmp, i);

	my_double_to_mpz (ztmp, y0f);
	bool y0_sign = y0f < 0.0;
	for (i = 0; i < TOTAL_LIMBS; i++)
		y0[i] = y[i] = cd_y[i] = mpz_getlimbn (ztmp, i);

	bool x_sign = x0_sign, y_sign = y0_sign;

	int k = 1, m = 1;
	i = 0;
	my_mpn_mul_fast (xsqr, x, x);
	my_mpn_mul_fast (ysqr, y, y);
	mpn_add_n (sqrsum, xsqr, ysqr, TOTAL_LIMBS);
	while (i < maxiter && mpn_cmp (sqrsum, four, TOTAL_LIMBS) < 0) {
		mp_limb_t tmp1[TOTAL_LIMBS];
		my_mpn_mul_fast (tmp1, x, y);
		mpn_lshift (y, tmp1, TOTAL_LIMBS, 1);
		y_sign = my_mpn_add_signed (y, y, x_sign != y_sign, y0, y0_sign);

		if (mpn_cmp (xsqr, ysqr, TOTAL_LIMBS) > 0) {
			mpn_sub_n (x, xsqr, ysqr, TOTAL_LIMBS);
			x_sign = false;
		} else {
			mpn_sub_n (x, ysqr, xsqr, TOTAL_LIMBS);
			x_sign = true;
		}
		x_sign = my_mpn_add_signed (x, x, x_sign, x0, x0_sign);


		k--;
		if (mpn_cmp (x, cd_x, TOTAL_LIMBS) == 0 && mpn_cmp (y, cd_y, TOTAL_LIMBS) == 0) {
			printf ("* Cycle of length %d detected after %u iterations.\n", m - k + 1, i);
			iter_saved += maxiter - i;
			i = maxiter;
			break;
		}
		if (k == 0) {
			k = m <<= 1;
			int j;
			memcpy (cd_x, x, sizeof (x));
			memcpy (cd_y, y, sizeof (y));
		}


		my_mpn_mul_fast (xsqr, x, x);
		my_mpn_mul_fast (ysqr, y, y);
		mpn_add_n (sqrsum, xsqr, ysqr, TOTAL_LIMBS);

		i++;
	}
	//return i;
	return (unsigned) (log (i) * 50.0);
}


/*unsigned
mandelbrot (double x0, double y0, unsigned maxiter)
{
	unsigned i = 0;
	double x = x0, y = y0;
	while (i < maxiter && x * x + y * y < 4.0) {
		double xold = x, yold = y;
		x = x * x - y * y + x0;
		y = 2 * xold * yold + y0;
		i++;
	}
	//return i;
	return (unsigned) (log (i) * 50.0);
}*/


void
gtk_mandel_render_pixel (GtkWidget *widget, int x, int y)
{
	GtkMandel *mandel = GTK_MANDEL (widget);
	unsigned i = mandelbrot (x * (mandel->xmax - mandel->xmin) / mandel->w + mandel->xmin, y * (mandel->ymin - mandel->ymax) / mandel->h + mandel->ymax, mandel->maxiter);
	gdk_threads_enter ();
	gtk_mandel_set_pixel (widget, x, y, i);
	//gdk_flush ();
	gdk_threads_leave ();
}

void calcpart (struct mandeldata *md, GtkWidget *widget, int x0, int y0, int x1, int y1);


/*void
calcmandel (GtkWidget *widget, double xmin, double xmax, double ymin, double ymax, unsigned w, unsigned h, unsigned maxiter)*/
gpointer *
calcmandel (gpointer *data)
{
	struct mandeldata *md = (struct mandeldata *) data;
	GtkWidget *widget = md->widget;
	GtkMandel *mandel = GTK_MANDEL (widget);

	if (md->join_me != NULL)
		g_thread_join (md->join_me);

	gdk_gc_set_foreground (mandel->gc, &mandel->black);
	gdk_draw_rectangle (GDK_DRAWABLE (widget->window), mandel->gc, true, 0, 0, mandel->w, mandel->h);
	gdk_gc_set_foreground (mandel->pm_gc, &mandel->black);
	gdk_draw_rectangle (GDK_DRAWABLE (mandel->pixmap), mandel->pm_gc, true, 0, 0, mandel->w, mandel->h);

	mandel->xmin = md->xmin;
	mandel->xmax = md->xmax;
	mandel->ymin = md->ymin;
	mandel->ymax = md->ymax;
	mandel->maxiter = md->maxiter;

	int x, y;

	for (x = 0; x < mandel->w; x++) {
		gtk_mandel_render_pixel (widget, x, 0);
		gtk_mandel_render_pixel (widget, x, mandel->h - 1);
	}

	for (y = 1; y < mandel->h - 1; y++) {
		gtk_mandel_render_pixel (widget, 0, y);
		gtk_mandel_render_pixel (widget, mandel->w -1, y);
	}

	calcpart (md, widget, 0, 0, mandel->w - 1, mandel->h - 1);

	gdk_threads_enter ();
	gdk_flush ();
	gdk_threads_leave ();

	printf ("* Iterations saved by cycle detection: %u\n", iter_saved);

	return NULL;
}


void
gtk_mandel_set_rect (GtkWidget *widget, int x0, int y0, int x1, int y1, unsigned iter)
{
	GtkMandel *mandel = GTK_MANDEL (widget);

	gtk_mandel_set_gc_color (widget, iter);

	gdk_draw_rectangle (GDK_DRAWABLE (mandel->pixmap), mandel->pm_gc, true, x0, y0, x1 - x0 + 1, y1 - y0 + 1);
	gdk_draw_rectangle (GDK_DRAWABLE (widget->window), mandel->gc, true, x0, y0, x1 - x0 + 1, y1 - y0 + 1);
}


void
calcpart (struct mandeldata *md, GtkWidget *widget, int x0, int y0, int x1, int y1)
{
	if (md->terminate)
		return;

	int x, y;
	bool failed = false;
	unsigned p0 = gtk_mandel_get_pixel (widget, x0, y0);

	for (x = x0; !failed && x <= x1; x++)
		failed = gtk_mandel_get_pixel (widget, x, y0) != p0 || gtk_mandel_get_pixel (widget, x, y1) != p0;

	for (y = y0; !failed && y <= y1; y++)
		failed = gtk_mandel_get_pixel (widget, x0, y) != p0 || gtk_mandel_get_pixel (widget, x1, y) != p0;

	if (failed) {
		if (x1 - x0 > y1 - y0) {
			unsigned xm = (x0 + x1) / 2;
			for (y = y0 + 1; y < y1; y++)
				gtk_mandel_render_pixel (widget, xm, y);

			if (xm - x0 > 1)
				calcpart (md, widget, x0, y0, xm, y1);
			if (x1 - xm > 1)
				calcpart (md, widget, xm, y0, x1, y1);
		} else {
			unsigned ym = (y0 + y1) / 2;
			for (x = x0 + 1; x < x1; x++)
				gtk_mandel_render_pixel (widget, x, ym);

			if (ym - y0 > 1)
				calcpart (md, widget, x0, y0, x1, ym);
			if (y1 - ym > 1)
				calcpart (md, widget, x0, ym, x1, y1);
		}
	} else {
		gdk_threads_enter ();
		gtk_mandel_set_rect (widget, x0 + 1, y0 + 1, x1 - 1, y1 - 1, p0);
		//gdk_flush ();
		gdk_threads_leave ();
	}
}


void
new_maxiter (GtkWidget *widget, gpointer *data)
{
	GtkMandel *mandel = GTK_MANDEL (data);
	int i = atoi (gtk_entry_get_text (GTK_ENTRY (widget)));
	if (i > 0)
		gtk_mandel_restart_thread (GTK_WIDGET (mandel), mandel->xmin, mandel->xmax, mandel->ymin, mandel->ymax, i);
}


int
main (int argc, char **argv)
{
	printf ("* mp_bits_per_limb = %d\n", mp_bits_per_limb);
	g_thread_init (NULL);
	gdk_threads_init ();
	gdk_threads_enter ();

	int i;
	for (i = 0; i < COLORS; i++) {
		mandelcolors[i].red = (guint16) (sin (2 * M_PI * i / COLORS) * 32767) + 32768;
		mandelcolors[i].green = (guint16) (sin (4 * M_PI * i / COLORS) * 32767) + 32768;
		mandelcolors[i].blue = (guint16) (sin (6 * M_PI * i / COLORS) * 32767) + 32768;
	}

	GtkWidget *win, *img;
	gtk_init (&argc, &argv);

	GtkWidget *hbox = gtk_hbox_new (false, 5);
	GtkLabel *maxiter_label = gtk_label_new ("maxiter:");
	gtk_container_add (GTK_CONTAINER (hbox), maxiter_label);
	GtkEntry *maxiter_entry = gtk_entry_new ();
	gtk_container_add (GTK_CONTAINER (hbox), maxiter_entry);

	GtkWidget *vbox = gtk_vbox_new (false, 5);
	gtk_container_add (GTK_CONTAINER (vbox), hbox);

	win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	img = gtk_mandel_new ();
	gtk_widget_set_size_request (img, 500, 500);
	gtk_container_add (GTK_CONTAINER (vbox), img);
	gtk_container_add (GTK_CONTAINER (win), vbox);

	gtk_signal_connect (maxiter_entry, "activate", new_maxiter, (gpointer) img);

	gtk_widget_show_all (win);

	printf ("now running main loop\n");

	gtk_mandel_restart_thread (img, -2.0, 1.0, -1.5, 1.5, 1000);
	gtk_main ();
	gdk_threads_leave ();

	return 0;
}
