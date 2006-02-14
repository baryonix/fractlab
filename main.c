#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

#include <unistd.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include <gmp.h>


#define INT_LIMBS 1

#define USE_CENTER_MAGF
#define USE_MPMATH

#define MARIANI_SILVER 1
#define SUCCESSIVE_REFINE 2
#define RENDERING_METHOD SUCCESSIVE_REFINE

#define SR_CHUNK_SIZE 32
#define LOG_FACTOR 100.0

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
	mpz_t xmin, xmax, ymin, ymax;
	unsigned w, h, maxiter;
	unsigned *data;
	GThread *thread;
	struct mandeldata *md;
	gdouble center_x, center_y;
	unsigned frac_limbs;
};


struct _GtkMandelClass
{
	GtkDrawingAreaClass parent_class;
};


struct mandeldata {
	mpz_t xmin, xmax, ymin, ymax;
	unsigned maxiter;
	GtkWidget *widget;
	GThread *join_me;
	volatile bool terminate;
	unsigned frac_limbs;
};


void
gtk_mandel_convert_x (GtkMandel *mandel, mpz_t rop, unsigned op)
{
	mpz_sub (rop, mandel->xmax, mandel->xmin);
	mpz_mul_ui (rop, rop, op);
	mpz_div_ui (rop, rop, mandel->w);
	mpz_add (rop, rop, mandel->xmin);
}


void
gtk_mandel_convert_y (GtkMandel *mandel, mpz_t rop, unsigned op)
{
	mpz_sub (rop, mandel->ymin, mandel->ymax);
	mpz_mul_ui (rop, rop, op);
	mpz_div_ui (rop, rop, mandel->h);
	mpz_add (rop, rop, mandel->ymax);
}


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

	mpz_init (mandel->xmin);
	mpz_init (mandel->xmax);
	mpz_init (mandel->ymin);
	mpz_init (mandel->ymax);

	mandel->w = mandel->h = 200;
	mandel->maxiter = 10000;
}

gpointer * calcmandel (gpointer *data);


void
gtk_mandel_restart_thread (GtkWidget *widget, mpz_t xmin, mpz_t xmax, mpz_t ymin, mpz_t ymax, unsigned maxiter, unsigned frac_limbs)
{
	GtkMandel *mandel = GTK_MANDEL (widget);
	struct mandeldata *md = malloc (sizeof (struct mandeldata));
	mpz_init_set (md->xmin, xmin);
	mpz_init_set (md->xmax, xmax);
	mpz_init_set (md->ymin, ymin);
	mpz_init_set (md->ymax, ymax);
	md->maxiter = maxiter;
	md->widget = widget;
	md->join_me = mandel->thread;
	md->terminate = false;
	md->frac_limbs = frac_limbs;

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
	mandel->pixmap = gdk_pixmap_new (my_img->window, 200, 200, -1);
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
		mpz_t xmin, xmax, ymin, ymax, cx, cy, dx, dy;
		mpz_init (xmin);
		mpz_init (xmax);
		mpz_init (ymin);
		mpz_init (ymax);
		mpz_init (cx);
		mpz_init (cy);
		mpz_init (dx);
		mpz_init (dy);
		gtk_mandel_convert_x (mandel, cx, mandel->center_x);
		gtk_mandel_convert_y (mandel, cy, mandel->center_y);
		gtk_mandel_convert_x (mandel, dx, e->x);
		gtk_mandel_convert_y (mandel, dy, e->y);
		mpz_sub (dx, cx, dx);
		mpz_abs (dx, dx);
		mpz_sub (dy, cy, dy);
		mpz_abs (dy, dy);
		if (mpz_cmp (dx, dy) > 0) {
			mpz_sub (xmin, cx, dx);
			mpz_add (xmax, cx, dx);
			mpz_sub (ymin, cy, dx);
			mpz_add (ymax, cy, dx);
		} else {
			mpz_sub (xmin, cx, dy);
			mpz_add (xmax, cx, dy);
			mpz_sub (ymin, cy, dy);
			mpz_add (ymax, cy, dy);
		}
		mpf_t f;
		mpf_init2 (f, mandel->frac_limbs * mp_bits_per_limb);
		my_mpz_to_mpf (f, xmin, mandel->frac_limbs);
		gmp_printf ("* xmin = %.Ff\n", f);
		my_mpz_to_mpf (f, xmax, mandel->frac_limbs);
		gmp_printf ("* xmax = %.Ff\n", f);
		my_mpz_to_mpf (f, ymin, mandel->frac_limbs);
		gmp_printf ("* ymin = %.Ff\n", f);
		my_mpz_to_mpf (f, ymax, mandel->frac_limbs);
		gmp_printf ("* ymax = %.Ff\n", f);
		mpf_clear (f);
		gtk_mandel_restart_thread (my_img, xmin, xmax, ymin, ymax, mandel->maxiter, mandel->frac_limbs);
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
}


void
gtk_mandel_put_pixel (GtkWidget *widget, int x, int y, unsigned iter)
{
	GtkMandel *mandel = GTK_MANDEL (widget);
	gtk_mandel_set_pixel (widget, x, y, iter);

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

bool
gtk_mandel_all_neighbors_same (GtkWidget *widget, int x, int y, int d)
{
	GtkMandel *mandel = GTK_MANDEL (widget);
	int px = gtk_mandel_get_pixel (widget, x, y);
	return x >= d && y >= d && x < mandel->w - d && y < mandel->h - d
		&& gtk_mandel_get_pixel (widget, x - d, y - d) == px
		&& gtk_mandel_get_pixel (widget, x - d, y    ) == px
		&& gtk_mandel_get_pixel (widget, x - d, y + d) == px
		&& gtk_mandel_get_pixel (widget, x    , y - d) == px
		&& gtk_mandel_get_pixel (widget, x    , y + d) == px
		&& gtk_mandel_get_pixel (widget, x + d, y - d) == px
		&& gtk_mandel_get_pixel (widget, x + d, y    ) == px
		&& gtk_mandel_get_pixel (widget, x + d, y + d) == px;
}


void
my_mpn_mul_fast (mp_limb_t *p, mp_limb_t *f0, mp_limb_t *f1, unsigned frac_limbs)
{
	unsigned total_limbs = INT_LIMBS + frac_limbs;
	mp_limb_t tmp[total_limbs * 2];
	int i;
	mpn_mul_n (tmp, f0, f1, total_limbs);
	for (i = 0; i < total_limbs; i++)
		p[i] = tmp[frac_limbs + i];
}

bool
my_mpn_add_signed (mp_limb_t *rop, mp_limb_t *op1, bool op1_sign, mp_limb_t *op2, bool op2_sign, unsigned frac_limbs)
{
	unsigned total_limbs = INT_LIMBS + frac_limbs;
	if (op1_sign == op2_sign) {
		mpn_add_n (rop, op1, op2, total_limbs);
		return op1_sign;
	} else {
		if (mpn_cmp (op1, op2, total_limbs) > 0) {
			mpn_sub_n (rop, op1, op2, total_limbs);
			return op1_sign;
		} else {
			mpn_sub_n (rop, op2, op1, total_limbs);
			return op2_sign;
		}
	}
}

unsigned iter_saved = 0;


#ifdef USE_MPMATH

unsigned
mandelbrot (mpz_t x0z, mpz_t y0z, unsigned maxiter, unsigned frac_limbs)
{
	unsigned total_limbs = INT_LIMBS + frac_limbs;
	mp_limb_t x[total_limbs], y[total_limbs], x0[total_limbs], y0[total_limbs], xsqr[total_limbs], ysqr[total_limbs], sqrsum[total_limbs], four[total_limbs];
	mp_limb_t cd_x[total_limbs], cd_y[total_limbs];
	mpz_t ztmp;
	unsigned i;

	for (i = 0; i < total_limbs; i++)
		four[i] = 0;
	four[frac_limbs] = 4;

	//mpz_init (ztmp);
	//my_double_to_mpz (ztmp, x0f);
	bool x0_sign = mpz_sgn (x0z) < 0;
	for (i = 0; i < total_limbs; i++)
		x0[i] = x[i] = cd_x[i] = mpz_getlimbn (x0z, i);

	//my_double_to_mpz (ztmp, y0f);
	bool y0_sign = mpz_sgn (y0z) < 0;
	for (i = 0; i < total_limbs; i++)
		y0[i] = y[i] = cd_y[i] = mpz_getlimbn (y0z, i);

	bool x_sign = x0_sign, y_sign = y0_sign;

	int k = 1, m = 1;
	i = 0;
	my_mpn_mul_fast (xsqr, x, x, frac_limbs);
	my_mpn_mul_fast (ysqr, y, y, frac_limbs);
	mpn_add_n (sqrsum, xsqr, ysqr, total_limbs);
	while (i < maxiter && mpn_cmp (sqrsum, four, total_limbs) < 0) {
		mp_limb_t tmp1[total_limbs];
		my_mpn_mul_fast (tmp1, x, y, frac_limbs);
		mpn_lshift (y, tmp1, total_limbs, 1);
		y_sign = my_mpn_add_signed (y, y, x_sign != y_sign, y0, y0_sign, frac_limbs);

		if (mpn_cmp (xsqr, ysqr, total_limbs) > 0) {
			mpn_sub_n (x, xsqr, ysqr, total_limbs);
			x_sign = false;
		} else {
			mpn_sub_n (x, ysqr, xsqr, total_limbs);
			x_sign = true;
		}
		x_sign = my_mpn_add_signed (x, x, x_sign, x0, x0_sign, frac_limbs);


		k--;
		if (mpn_cmp (x, cd_x, total_limbs) == 0 && mpn_cmp (y, cd_y, total_limbs) == 0) {
			//printf ("* Cycle of length %d detected after %u iterations.\n", m - k + 1, i);
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


		my_mpn_mul_fast (xsqr, x, x, frac_limbs);
		my_mpn_mul_fast (ysqr, y, y, frac_limbs);
		mpn_add_n (sqrsum, xsqr, ysqr, total_limbs);

		i++;
	}
#ifdef LOG_FACTOR
	return (unsigned) (log (i) * LOG_FACTOR);
#else
	return i;
#endif
}


#else /* USE_MPMATH */


unsigned
mandelbrot (mpz_t x0z, mpz_t y0z, unsigned maxiter, unsigned frac_limbs)
{
	double x0 = ldexp (mpz_get_d (x0z), -frac_limbs * mp_bits_per_limb);
	double y0 = ldexp (mpz_get_d (y0z), -frac_limbs * mp_bits_per_limb);
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
}

#endif /* USE_MPMATH */


void
gtk_mandel_render_pixel (GtkWidget *widget, int x, int y)
{
	GtkMandel *mandel = GTK_MANDEL (widget);
	mpz_t xz, yz;
	mpz_init (xz);
	mpz_init (yz);
	gtk_mandel_convert_x (mandel, xz, x);
	gtk_mandel_convert_y (mandel, yz, y);
	unsigned i = mandelbrot (xz, yz, mandel->maxiter, mandel->frac_limbs);
	mpz_clear (xz);
	mpz_clear (yz);
	gdk_threads_enter ();
	gtk_mandel_put_pixel (widget, x, y, i);
	//gdk_flush ();
	gdk_threads_leave ();
}

void calcpart (struct mandeldata *md, GtkWidget *widget, int x0, int y0, int x1, int y1);


void
gtk_mandel_put_rect (GtkWidget *widget, int x, int y, int d, unsigned iter)
{
	GtkMandel *mandel = GTK_MANDEL (widget);
	gtk_mandel_set_pixel (widget, x, y, iter);
	gtk_mandel_set_gc_color (mandel, iter);
	gdk_draw_rectangle (GDK_DRAWABLE (widget->window), mandel->gc, true, x, y, d, d);
	gdk_draw_rectangle (GDK_DRAWABLE (mandel->pixmap), mandel->pm_gc, true, x, y, d, d);
}


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

	mpz_set (mandel->xmin, md->xmin);
	mpz_set (mandel->xmax, md->xmax);
	mpz_set (mandel->ymin, md->ymin);
	mpz_set (mandel->ymax, md->ymax);
	mpz_clear (md->xmin);
	mpz_clear (md->xmax);
	mpz_clear (md->ymin);
	mpz_clear (md->ymax);
	mandel->maxiter = md->maxiter;
	mandel->frac_limbs = md->frac_limbs;

#if RENDERING_METHOD == MARIANI_SILVER
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
#elif RENDERING_METHOD == SUCCESSIVE_REFINE
	unsigned x, y, chunk_size = SR_CHUNK_SIZE;

	while (chunk_size != 0) {
		for (y = 0; y < mandel->h; y += chunk_size)
			for (x = 0; x < mandel->w && !md->terminate; x += chunk_size) {
				unsigned parent_x, parent_y;
				bool do_eval;
				if (x % (2 * chunk_size) == 0)
					parent_x = x;
				else
					parent_x = x - chunk_size;
				if (y % (2 * chunk_size) == 0)
					parent_y = y;
				else
					parent_y = y - chunk_size;

				if (chunk_size == SR_CHUNK_SIZE) // 1st pass
					do_eval = true;
				else if (parent_x == x && parent_y == y)
					do_eval = false;
				else if (gtk_mandel_all_neighbors_same (mandel, parent_x, parent_y, chunk_size << 1))
					do_eval = false;
				else
					do_eval = true;

				if (do_eval) {
					gtk_mandel_render_pixel (widget, x, y);
					gdk_threads_enter ();
					gtk_mandel_put_rect (widget, x, y, chunk_size, gtk_mandel_get_pixel (widget, x, y));
					gdk_threads_leave ();
				} else {
					gdk_threads_enter ();
					gtk_mandel_put_pixel (widget, x, y, gtk_mandel_get_pixel (widget, parent_x, parent_y));
					gdk_threads_leave ();
				}
			}
		chunk_size >>= 1;
	}
#else
#error Unrecognized rendering method
#endif /* RENDERING_METHOD */

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
		gtk_mandel_restart_thread (GTK_WIDGET (mandel), mandel->xmin, mandel->xmax, mandel->ymin, mandel->ymax, i, mandel->frac_limbs);
}


int
main (int argc, char **argv)
{
	printf ("* mp_bits_per_limb = %d\n", mp_bits_per_limb);
	g_thread_init (NULL);
	gdk_threads_init ();
	gdk_threads_enter ();

	unsigned frac_limbs = 5, total_limbs = INT_LIMBS + frac_limbs;

	mpf_set_default_prec (total_limbs * mp_bits_per_limb);

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
	gtk_widget_set_size_request (img, 200, 200);
	gtk_container_add (GTK_CONTAINER (vbox), img);
	gtk_container_add (GTK_CONTAINER (win), vbox);

	gtk_signal_connect (maxiter_entry, "activate", new_maxiter, (gpointer) img);

	gtk_widget_show_all (win);

	printf ("now running main loop\n");

	mpz_t xmin, xmax, ymin, ymax;
	mpf_t xc, yc, magf, f;
	mpz_init (xmin);
	mpz_init (xmax);
	mpz_init (ymin);
	mpz_init (ymax);

	mpf_init (f);


#ifdef USE_CENTER_MAGF

	FILE *cfile = fopen (argv[1], "r");
	if (cfile == NULL) {
		perror ("fopen");
		exit (1);
	}
	char buf[1024];
	fgets (buf, 1024, cfile);
	mpf_init_set_str (xc, buf, 10);
	fgets (buf, 1024, cfile);
	mpf_init_set_str (yc, buf, 10);
	fgets (buf, 1024, cfile);
	mpf_init_set_str (magf, buf, 10);
	fclose (cfile);

	mpf_ui_div (magf, 1, magf);
	long exponent;
	mpf_get_d_2exp (&exponent, magf);
	fprintf (stderr, "* Using %d limbs.\n", (abs (exponent) + 16) / 32 + 1);

	mpf_sub (f, xc, magf);
	mpf_mul_2exp (f, f, frac_limbs * mp_bits_per_limb);
	mpz_set_f (xmin, f);

	mpf_add (f, xc, magf);
	mpf_mul_2exp (f, f, frac_limbs * mp_bits_per_limb);
	mpz_set_f (xmax, f);

	mpf_sub (f, yc, magf);
	mpf_mul_2exp (f, f, frac_limbs * mp_bits_per_limb);
	mpz_set_f (ymin, f);

	mpf_add (f, yc, magf);
	mpf_mul_2exp (f, f, frac_limbs * mp_bits_per_limb);
	mpz_set_f (ymax, f);

#else /* USE_CENTER_MAGF */

	mpf_set_str (f, "-1.25336541010300744474", 10);
	mpf_mul_2exp (f, f, frac_limbs * mp_bits_per_limb);
	mpz_set_f (xmin, f);
	mpf_set_str (f, "-1.25336539811306487297", 10);
	mpf_mul_2exp (f, f, frac_limbs * mp_bits_per_limb);
	mpz_set_f (xmax, f);

	mpf_set_str (f, "0.34461507040406514393", 10);
	mpf_mul_2exp (f, f, frac_limbs * mp_bits_per_limb);
	mpz_set_f (ymin, f);
	mpf_set_str (f, "0.344615082394007715705", 10);
	mpf_mul_2exp (f, f, frac_limbs * mp_bits_per_limb);
	mpz_set_f (ymax, f);

#endif /* USE_CENTER_MAGF */

	//my_double_to_mpz (xmin, -2.0);
	//my_double_to_mpz (xmax, 1.0);
	//my_double_to_mpz (ymin, -1.5);
	//my_double_to_mpz (ymax, 1.5);
	gtk_mandel_restart_thread (img, xmin, xmax, ymin, ymax, 1000, frac_limbs);
	gtk_main ();
	gdk_threads_leave ();

	return 0;
}
