#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <gmp.h>

#include "mandelbrot.h"
#include "util.h"


struct sr_state {
	struct mandel_renderer *renderer;
	int y, chunk_size;
	GStaticMutex mutex;
};


struct ms_state {
	struct mandel_renderer *renderer;
	GMutex *mutex;
	GQueue *queue;
	GCond *cond;
	volatile int idle_threads;
};

struct ms_q_entry {
	int x0, y0, x1, y1;
};


struct btrace_q_entry {
	int x, y, xstep, ystep;
};


static void calc_sr_row (struct mandel_renderer *mandel, int y, int chunk_size);
static void calc_sr_mt_pass (struct mandel_renderer *mandel, int chunk_size);
static gpointer sr_mt_thread_func (gpointer data);
static void calc_ms_mt (struct mandel_renderer *mandel);
static gpointer ms_mt_thread_func (gpointer data);
static void ms_queue_push (struct ms_state *state, int x0, int y0, int x1, int y1);
static void ms_do_work (struct mandel_renderer *md, int x0, int y0, int x1, int y1, void (*enqueue) (int, int, int, int, void *), void *data);
static void ms_enqueue (int x0, int y0, int x1, int y1, void *data);
static void ms_mt_enqueue (int x0, int y0, int x1, int y1, void *data);
static void render_btrace (struct mandel_renderer *md, int x0, int y0, unsigned char *flags, bool fill_mode);
static void render_btrace_test (struct mandel_renderer *md, int x0, int y0, int xstep0, int ystep0, GQueue *queue, unsigned char *flags, bool fill_mode);
static void bt_turn_right (int xs, int ys, int *xsn, int *ysn);
static void bt_turn_left (int xs, int ys, int *xsn, int *ysn);
static unsigned *pascal_triangle (unsigned n);
static inline mandel_fp_t stored_power_fp (mandel_fp_t x, unsigned n, mandel_fp_t *powers);
static void store_powers_fp (mandel_fp_t *powers, mandel_fp_t x, unsigned n);
static void complex_pow_fp (mandel_fp_t xreal, mandel_fp_t ximag, unsigned n, mandel_fp_t *rreal, mandel_fp_t *rimag, unsigned *pascal);
static void store_powers (mp_limb_t *powers, bool *signs, mp_limb_t *x, bool xsign, unsigned n, unsigned frac_limbs);
static void complex_pow (mp_limb_t *xreal, bool xreal_sign, mp_limb_t *ximag, bool ximag_sign, unsigned n, mp_limb_t *real, bool *rreal_sign, mp_limb_t *imag, bool *rimag_sign, unsigned frac_limbs, unsigned *pascal);
static unsigned mandel_julia_z2 (mp_limb_t *x0, bool x0_sign, mp_limb_t *y0, bool y0_sign, mp_limb_t *preal, bool preal_sign, mp_limb_t *pimag, bool pimag_sign, unsigned maxiter, unsigned frac_limbs);
static unsigned mandel_julia_zpower (const struct mandel_renderer *md, mp_limb_t *x0, bool x0_sign, mp_limb_t *y0, bool y0_sign, mp_limb_t *preal, bool preal_sign, mp_limb_t *pimag, bool pimag_sign, unsigned maxiter, unsigned frac_limbs);
static unsigned mandel_julia_z2_fp (mandel_fp_t x0, mandel_fp_t y0, mandel_fp_t preal, mandel_fp_t pimag, unsigned maxiter);
static unsigned mandel_julia_zpower_fp (const struct mandel_renderer *md, mandel_fp_t x0, mandel_fp_t y0, mandel_fp_t preal, mandel_fp_t pimag, unsigned maxiter);
static void mandeldata_init_mpvars (struct mandeldata *md);
static void btrace_queue_push (GQueue *queue, int x, int y, int xstep, int ystep);
static void btrace_queue_pop (GQueue *queue, int *x, int *y, int *xstep, int *ystep);


const char *render_method_names[] = {
	"Successive Refinement",
	"Mariani-Silver",
	"Boundary Tracing"
};


void
mandel_convert_x (const struct mandel_renderer *mandel, mpz_t rop, unsigned op)
{
	mpz_sub (rop, mandel->xmax, mandel->xmin);
	mpz_mul_ui (rop, rop, op);
	mpz_div_ui (rop, rop, mandel->w);
	mpz_add (rop, rop, mandel->xmin);
}


void
mandel_convert_y (const struct mandel_renderer *mandel, mpz_t rop, unsigned op)
{
	mpz_sub (rop, mandel->ymin, mandel->ymax);
	mpz_mul_ui (rop, rop, op);
	mpz_div_ui (rop, rop, mandel->h);
	mpz_add (rop, rop, mandel->ymax);
}


void
mandel_convert_x_f (const struct mandel_renderer *mandel, mpf_t rop, unsigned op)
{
	mpf_sub (rop, mandel->xmax_f, mandel->xmin_f);
	mpf_mul_ui (rop, rop, op);
	mpf_div_ui (rop, rop, mandel->w);
	mpf_add (rop, rop, mandel->xmin_f);
}


void
mandel_convert_y_f (const struct mandel_renderer *mandel, mpf_t rop, unsigned op)
{
	mpf_sub (rop, mandel->ymin_f, mandel->ymax_f);
	mpf_mul_ui (rop, rop, op);
	mpf_div_ui (rop, rop, mandel->h);
	mpf_add (rop, rop, mandel->ymax_f);
}

void
mandel_set_pixel (struct mandel_renderer *mandel, int x, int y, unsigned iter)
{
	mandel->data[x * mandel->h + y] = iter;
}


void
mandel_put_pixel (struct mandel_renderer *mandel, unsigned x, unsigned y, unsigned iter)
{
	mandel_set_pixel (mandel, x, y, iter);
	if (mandel->display_pixel != NULL)
		mandel->display_pixel (x, y, iter, mandel->user_data);
}


int
mandel_get_pixel (const struct mandel_renderer *mandel, int x, int y)
{
	return mandel->data[x * mandel->h + y];
}


bool
mandel_all_neighbors_same (const struct mandel_renderer *mandel, unsigned x, unsigned y, unsigned d)
{
	int px = mandel_get_pixel (mandel, x, y);
	return x >= d && y >= d && x < mandel->w - d && y < mandel->h - d
		&& mandel_get_pixel (mandel, x - d, y - d) == px
		&& mandel_get_pixel (mandel, x - d, y    ) == px
		&& mandel_get_pixel (mandel, x - d, y + d) == px
		&& mandel_get_pixel (mandel, x    , y - d) == px
		&& mandel_get_pixel (mandel, x    , y + d) == px
		&& mandel_get_pixel (mandel, x + d, y - d) == px
		&& mandel_get_pixel (mandel, x + d, y    ) == px
		&& mandel_get_pixel (mandel, x + d, y + d) == px;
}


/*
 * FIXME: Despite the name, this routine isn't especially fast.
 * It should probably suffice to multiply only part of the operands,
 * ignoring a few of the least significant limbs, but when I tried this
 * resulted in a significant loss of precision.
 */
void
my_mpn_mul_fast (mp_limb_t *p, mp_limb_t *f0, mp_limb_t *f1, unsigned frac_limbs)
{
	unsigned total_limbs = INT_LIMBS + frac_limbs;
	mp_limb_t tmp[total_limbs * 2];
	int i;
	mpn_mul (tmp, f0, total_limbs, f1, total_limbs);
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
#ifdef MY_MPN_SUB_SLOW
		if (mpn_cmp (op1, op2, total_limbs) > 0) {
			mpn_sub_n (rop, op1, op2, total_limbs);
			return op1_sign;
		} else {
			mpn_sub_n (rop, op2, op1, total_limbs);
			return op2_sign;
		}
#else /* MY_MPN_SUB_SLOW */
		if (mpn_sub_n (rop, op1, op2, total_limbs) != 0) {
			my_mpn_invert (rop, total_limbs);
			return op2_sign;
		} else
			return op1_sign;
#endif /* MY_MPN_SUB_SLOW */
	}
}


/* XXX this doesn't work with GMP nails */
void
my_mpn_invert (mp_limb_t *op, unsigned total_limbs)
{
	int i = 0;
	while (i < total_limbs) {
		bool bk = op[i] != 0;
		op[i] = ~op[i] + 1;
		i++;
		if (bk)
			break;
	}
	while (i < total_limbs) {
		op[i] = ~op[i];
		i++;
	}
}


unsigned iter_saved = 0;


static unsigned
mandel_julia_z2 (mp_limb_t *x0, bool x0_sign, mp_limb_t *y0, bool y0_sign, mp_limb_t *preal, bool preal_sign, mp_limb_t *pimag, bool pimag_sign, unsigned maxiter, unsigned frac_limbs)
{
	unsigned total_limbs = INT_LIMBS + frac_limbs;
	mp_limb_t x[total_limbs], y[total_limbs], xsqr[total_limbs], ysqr[total_limbs], sqrsum[total_limbs], four[INT_LIMBS];
	mp_limb_t cd_x[total_limbs], cd_y[total_limbs];
	unsigned i;

	four[0] = 4;
	for (i = 1; i < INT_LIMBS; i++)
		four[i] = 0;

	memcpy (x, x0, sizeof (x));
	memcpy (cd_x, x0, sizeof (cd_x));
	memcpy (y, y0, sizeof (y));
	memcpy (cd_y, y0, sizeof (cd_y));

	bool x_sign = x0_sign, y_sign = y0_sign;

	int k = 1, m = 1;
	i = 0;
	my_mpn_mul_fast (xsqr, x, x, frac_limbs);
	my_mpn_mul_fast (ysqr, y, y, frac_limbs);
	mpn_add_n (sqrsum, xsqr, ysqr, total_limbs);
	while (i < maxiter && mpn_cmp (sqrsum + frac_limbs, four, INT_LIMBS) < 0) {
		mp_limb_t tmp1[total_limbs];
		my_mpn_mul_fast (tmp1, x, y, frac_limbs);
		mpn_lshift (y, tmp1, total_limbs, 1);
		y_sign = my_mpn_add_signed (y, y, x_sign != y_sign, pimag, pimag_sign, frac_limbs);
		x_sign = my_mpn_add_signed (x, xsqr, false, ysqr, true, frac_limbs);
		x_sign = my_mpn_add_signed (x, x, x_sign, preal, preal_sign, frac_limbs);

		k--;
		/* FIXME we must compare the signs here! */
		if (mpn_cmp (x, cd_x, total_limbs) == 0 && mpn_cmp (y, cd_y, total_limbs) == 0) {
			//printf ("* Cycle of length %d detected after %u iterations.\n", m - k + 1, i);
			iter_saved += maxiter - i;
			i = maxiter;
			break;
		}
		if (k == 0) {
			k = m <<= 1;
			memcpy (cd_x, x, sizeof (x));
			memcpy (cd_y, y, sizeof (y));
		}

		my_mpn_mul_fast (xsqr, x, x, frac_limbs);
		my_mpn_mul_fast (ysqr, y, y, frac_limbs);
		mpn_add_n (sqrsum, xsqr, ysqr, total_limbs);

		i++;
	}
	return i;
}


static unsigned
mandel_julia_zpower (const struct mandel_renderer *renderer, mp_limb_t *x0, bool x0_sign, mp_limb_t *y0, bool y0_sign, mp_limb_t *preal, bool preal_sign, mp_limb_t *pimag, bool pimag_sign, unsigned maxiter, unsigned frac_limbs)
{
	unsigned total_limbs = INT_LIMBS + frac_limbs;
	mp_limb_t x[total_limbs], y[total_limbs], xsqr[total_limbs], ysqr[total_limbs], sqrsum[total_limbs], four[INT_LIMBS];
	mp_limb_t cd_x[total_limbs], cd_y[total_limbs];
	unsigned i;
	unsigned zpower = renderer->md->zpower;
	unsigned *ptri = renderer->ptriangle;

	/* FIXME we shouldn't have to do this for every call */
	four[0] = 4;
	for (i = 1; i < INT_LIMBS; i++)
		four[i] = 0;

	memcpy (x, x0, sizeof (x));
	memcpy (cd_x, x0, sizeof (cd_x));
	memcpy (y, y0, sizeof (y));
	memcpy (cd_y, y0, sizeof (cd_y));

	bool x_sign = x0_sign, y_sign = y0_sign;

	int k = 1, m = 1;
	i = 0;
	my_mpn_mul_fast (xsqr, x, x, frac_limbs);
	my_mpn_mul_fast (ysqr, y, y, frac_limbs);
	mpn_add_n (sqrsum, xsqr, ysqr, total_limbs);
	while (i < maxiter && mpn_cmp (sqrsum + frac_limbs, four, INT_LIMBS) < 0) {
		mp_limb_t tmpreal[total_limbs], tmpimag[total_limbs];
		bool tmpreal_sign, tmpimag_sign;
		complex_pow (x, x_sign, y, y_sign, zpower, tmpreal, &tmpreal_sign, tmpimag, &tmpimag_sign, frac_limbs, ptri);
		x_sign = my_mpn_add_signed (x, tmpreal, tmpreal_sign, preal, preal_sign, frac_limbs);
		y_sign = my_mpn_add_signed (y, tmpimag, tmpimag_sign, pimag, pimag_sign, frac_limbs);

		k--;
		/* FIXME we must compare the signs here! */
		if (mpn_cmp (x, cd_x, total_limbs) == 0 && mpn_cmp (y, cd_y, total_limbs) == 0) {
			//printf ("* Cycle of length %d detected after %u iterations.\n", m - k + 1, i);
			iter_saved += maxiter - i;
			i = maxiter;
			break;
		}
		if (k == 0) {
			k = m <<= 1;
			memcpy (cd_x, x, sizeof (x));
			memcpy (cd_y, y, sizeof (y));
		}

		my_mpn_mul_fast (xsqr, x, x, frac_limbs);
		my_mpn_mul_fast (ysqr, y, y, frac_limbs);
		mpn_add_n (sqrsum, xsqr, ysqr, total_limbs);

		i++;
	}
	return i;
}


static unsigned
mandel_julia_z2_fp (mandel_fp_t x0, mandel_fp_t y0, mandel_fp_t preal, mandel_fp_t pimag, unsigned maxiter)
{
	unsigned i = 0, k = 1, m = 1;
	mandel_fp_t x = x0, y = y0, cd_x = x, cd_y = y;
	while (i < maxiter && x * x + y * y < 4.0) {
		mandel_fp_t xold = x, yold = y;
		x = x * x - y * y + preal;
		y = 2 * xold * yold + pimag;

		k--;
		if (x == cd_x && y == cd_y) {
			iter_saved += maxiter - i;
			i = maxiter;
			break;
		}

		if (k == 0) {
			k = m <<= 1;
			cd_x = x;
			cd_y = y;
		}

		i++;
	}
	return i;
}


static unsigned
mandel_julia_zpower_fp (const struct mandel_renderer *renderer, mandel_fp_t x0, mandel_fp_t y0, mandel_fp_t preal, mandel_fp_t pimag, unsigned maxiter)
{
	unsigned i = 0, k = 1, m = 1;
	unsigned zpower = renderer->md->zpower;
	unsigned *ptri = renderer->ptriangle;
	mandel_fp_t x = x0, y = y0, cd_x = x, cd_y = y;
	while (i < maxiter && x * x + y * y < 4.0) {
		complex_pow_fp (x, y, zpower, &x, &y, ptri);
		x += preal;
		y += pimag;

		k--;
		if (x == cd_x && y == cd_y) {
			iter_saved += maxiter - i;
			i = maxiter;
			break;
		}

		if (k == 0) {
			k = m <<= 1;
			cd_x = x;
			cd_y = y;
		}

		i++;
	}
	return i;
}


int
mandel_pixel_value (const struct mandel_renderer *mandel, int x, int y)
{
	int i = 0; /* initialize to make gcc happy */
	if (mandel->frac_limbs == 0) {
		// FP
		/* FIXME we shouldn't do this for every pixel */
		mandel_fp_t xmin = mpf_get_mandel_fp (mandel->xmin_f);
		mandel_fp_t xmax = mpf_get_mandel_fp (mandel->xmax_f);
		mandel_fp_t ymin = mpf_get_mandel_fp (mandel->ymin_f);
		mandel_fp_t ymax = mpf_get_mandel_fp (mandel->ymax_f);
		mandel_fp_t xf = x * (xmax - xmin) / mandel->w + xmin;
		mandel_fp_t yf = y * (ymin - ymax) / mandel->h + ymax;
		switch (mandel->md->type) {
			case FRACTAL_MANDELBROT: {
#ifdef MANDELBROT_FP_ASM
				if (mandel->md->zpower == 2) {
					i = mandelbrot_fp (xf, yf, mandel->md->maxiter);
					break;
				}
#endif
				i = mandel_julia_fp (mandel, xf, yf, xf, yf, mandel->md->maxiter);
				break;
			}
			case FRACTAL_JULIA: {
				i = mandel_julia_fp (mandel, xf, yf, mandel->preal_float, mandel->pimag_float, mandel->md->maxiter);
				break;
			}
			default: {
				fprintf (stderr, "* BUG: Unknown fractal type %d\n", mandel->md->type);
				return 0;
			}
		}
	} else {
		// MP
		mpz_t z;
		int j;
		unsigned total_limbs = INT_LIMBS + mandel->frac_limbs;
		mp_limb_t x0[total_limbs], y0[total_limbs];
		bool x0_sign, y0_sign;
		mpz_init (z);

		mandel_convert_x (mandel, z, x);
		for (j = 0; j < total_limbs; j++)
			x0[j] = mpz_getlimbn (z, j);
		x0_sign = mpz_sgn (z) < 0;

		mandel_convert_y (mandel, z, y);
		for (j = 0; j < total_limbs; j++)
			y0[j] = mpz_getlimbn (z, j);
		y0_sign = mpz_sgn (z) < 0;

		mpz_clear (z);
		switch (mandel->md->type) {
			case FRACTAL_MANDELBROT: {
				i = mandel_julia (mandel, x0, x0_sign, y0, y0_sign, x0, x0_sign, y0, y0_sign, mandel->md->maxiter, mandel->frac_limbs);
				break;
			}
			case FRACTAL_JULIA: {
				i = mandel_julia (mandel, x0, x0_sign, y0, y0_sign, mandel->preal, mandel->preal_sign, mandel->pimag, mandel->pimag_sign, mandel->md->maxiter, mandel->frac_limbs);
				break;
			}
			default: {
				fprintf (stderr, "* BUG: Unknown fractal type %d\n", mandel->md->type);
				return 0;
			}
		}
	}
	if (mandel->md->log_factor != 0.0)
		i = mandel->md->log_factor * log (i);
	return i;
}


int
mandel_render_pixel (struct mandel_renderer *mandel, int x, int y)
{
	int i = mandel_get_pixel (mandel, x, y);
	if (i >= 0)
		return i; /* pixel has been rendered previously */
	i = mandel_pixel_value (mandel, x, y);
	mandel_put_pixel (mandel, x, y, i);
	return i;
}



void
mandel_display_rect (struct mandel_renderer *mandel, int x, int y, int w, int h, unsigned iter)
{
	if (mandel->display_rect != NULL)
		mandel->display_rect (x, y, w, h, iter, mandel->user_data);
	else if (mandel->display_pixel != NULL)
		mandel->display_pixel (x, y, iter, mandel->user_data);
}


void
mandel_put_rect (struct mandel_renderer *mandel, int x, int y, int w, int h, unsigned iter)
{
	int xc, yc;
	for (xc = x; xc < x + w; xc++)
		for (yc = y; yc < y + h; yc++)
			mandel_set_pixel (mandel, xc, yc, iter);
	mandel_display_rect (mandel, x, y, w, h, iter);
}


void
mandel_renderer_init (struct mandel_renderer *renderer, const struct mandeldata *md, unsigned w, unsigned h)
{
	memset (renderer, 0, sizeof (*renderer)); /* just to be safe... */
	renderer->ptriangle = NULL;
	renderer->data = NULL;
	renderer->terminate = false;
	renderer->preal = NULL;
	renderer->pimag = NULL;
	renderer->display_pixel = NULL;
	renderer->display_rect = NULL;
	mpf_init (renderer->xmin_f);
	mpf_init (renderer->xmax_f);
	mpf_init (renderer->ymin_f);
	mpf_init (renderer->ymax_f);
	mpz_init (renderer->xmin);
	mpz_init (renderer->xmax);
	mpz_init (renderer->ymin);
	mpz_init (renderer->ymax);

	renderer->md = md;
	renderer->w = w;
	renderer->h = h;

	renderer->aspect = (double) renderer->w / renderer->h;
	center_to_corners (renderer->xmin_f, renderer->xmax_f, renderer->ymin_f, renderer->ymax_f, renderer->md->cx, renderer->md->cy, renderer->md->magf, renderer->aspect);

	// Determine the required precision.
	mpf_t dx;
	mpf_init (dx);

	mpf_sub (dx, renderer->xmax_f, renderer->xmin_f);
	mpf_div_ui (dx, dx, renderer->w);

	long exponent;
	mpf_get_d_2exp (&exponent, dx);

	mpf_clear (dx);

	if (exponent > 0)
		exponent = 0;

	// We add a minimum of 4 extra bits of precision, that should do.
	int required_bits = 4 - exponent;

	if (required_bits < MP_THRESHOLD)
		renderer->frac_limbs = 0;
	else
		renderer->frac_limbs = required_bits / mp_bits_per_limb + INT_LIMBS;

	unsigned frac_limbs = renderer->frac_limbs;
	unsigned total_limbs = INT_LIMBS + frac_limbs;

	// Convert coordinates to integer values.
	mpf_t f;
	mpf_init2 (f, total_limbs * mp_bits_per_limb);

	mpf_mul_2exp (f, renderer->xmin_f, frac_limbs * mp_bits_per_limb);
	mpz_set_f (renderer->xmin, f);

	mpf_mul_2exp (f, renderer->xmax_f, frac_limbs * mp_bits_per_limb);
	mpz_set_f (renderer->xmax, f);

	mpf_mul_2exp (f, renderer->ymin_f, frac_limbs * mp_bits_per_limb);
	mpz_set_f (renderer->ymin, f);

	mpf_mul_2exp (f, renderer->ymax_f, frac_limbs * mp_bits_per_limb);
	mpz_set_f (renderer->ymax, f);

	if (renderer->md->type == FRACTAL_JULIA) {
		if (frac_limbs > 0) {
			renderer->preal = malloc (total_limbs * sizeof (mp_limb_t));
			renderer->pimag = malloc (total_limbs * sizeof (mp_limb_t));
			mpz_t z;
			mpz_init (z);
			mpf_mul_2exp (f, renderer->md->preal_f, frac_limbs * mp_bits_per_limb);
			mpz_set_f (z, f);
			int i;
			for (i = 0; i < total_limbs; i++)
				renderer->preal[i] = mpz_getlimbn (z, i);
			mpf_mul_2exp (f, renderer->md->pimag_f, frac_limbs * mp_bits_per_limb);
			mpz_set_f (z, f);
			for (i = 0; i < total_limbs; i++)
				renderer->pimag[i] = mpz_getlimbn (z, i);
			mpz_clear (z);
		} else {
			renderer->preal_float = mpf_get_mandel_fp (renderer->md->preal_f);
			renderer->pimag_float = mpf_get_mandel_fp (renderer->md->pimag_f);
		}
	}

	mpf_clear (f);

	if (renderer->md->zpower < 2)
		fprintf (stderr, "* ERROR: zpower < 2 used\n");
	else if (renderer->md->zpower > 2)
		renderer->ptriangle = pascal_triangle (renderer->md->zpower);

	renderer->data = malloc (renderer->w * renderer->h * sizeof (*renderer->data));
}


void
mandel_renderer_clear (struct mandel_renderer *renderer)
{
	free_not_null (renderer->ptriangle);
	free_not_null (renderer->data);
	free_not_null (renderer->preal);
	free_not_null (renderer->pimag);
	mpf_clear (renderer->xmin_f);
	mpf_clear (renderer->xmax_f);
	mpf_clear (renderer->ymin_f);
	mpf_clear (renderer->ymax_f);
	mpz_clear (renderer->xmin);
	mpz_clear (renderer->xmax);
	mpz_clear (renderer->ymin);
	mpz_clear (renderer->ymax);
}


void
mandel_render (struct mandel_renderer *mandel)
{
	int i;
	for (i = 0; i < mandel->w * mandel->h; i++)
		mandel->data[i] = -1;

	switch (mandel->render_method) {
		case RM_MARIANI_SILVER: {
			int x, y;

			for (x = 0; !mandel->terminate && x < mandel->w; x++) {
				mandel_render_pixel (mandel, x, 0);
				mandel_render_pixel (mandel, x, mandel->h - 1);
			}

			for (y = 1; !mandel->terminate && y < mandel->h - 1; y++) {
				mandel_render_pixel (mandel, 0, y);
				mandel_render_pixel (mandel, mandel->w - 1, y);
			}

			if (mandel->terminate)
				break;

			if (mandel->thread_count > 1)
				calc_ms_mt (mandel);
			else
				calcpart (mandel, 0, 0, mandel->w - 1, mandel->h - 1);

			break;
		}

		case RM_SUCCESSIVE_REFINE: {
			unsigned y, chunk_size = SR_CHUNK_SIZE;

			while (!mandel->terminate && chunk_size != 0) {
				if (mandel->thread_count > 1)
					calc_sr_mt_pass (mandel, chunk_size);
				else
					for (y = 0; !mandel->terminate && y < mandel->h; y += chunk_size)
						calc_sr_row (mandel, y, chunk_size);
				chunk_size >>= 1;
			}

			break;
		}

		case RM_BOUNDARY_TRACE: {
			unsigned char flags[mandel->w * mandel->h];
			memset (flags, 0, sizeof (flags));
			int x, y;
			for (y = 0; !mandel->terminate && y < mandel->h; y++)
				for (x = 0; !mandel->terminate && x < mandel->w; x++)
					if (!flags[x * mandel->h + y]) {
						render_btrace (mandel, x, y, flags, false);
						render_btrace (mandel, x, y, flags, true);
					}
			/*GQueue *queue = g_queue_new ();
			btrace_queue_push (queue, 0, 0, 0, -1);
			while (!g_queue_is_empty (queue)) {
				int x, y, xstep, ystep;
				btrace_queue_pop (queue, &x, &y, &xstep, &ystep);
				if (!flags[x * mandel->h + y]) {
					render_btrace_test (mandel, x, y, xstep, ystep, queue, flags, false);
					render_btrace_test (mandel, x, y, xstep, ystep, queue, flags, true);
				}
			}*/
			break;
		}

		default: {
			fprintf (stderr, "* BUG: invalid case value at %s:%d\n", __FILE__, __LINE__);
			break;
		}
	}
}


void
calcpart (struct mandel_renderer *md, int x0, int y0, int x1, int y1)
{
	if (md->terminate)
		return;
	ms_do_work (md, x0, y0, x1, y1, ms_enqueue, md);
}


static void
calc_sr_row (struct mandel_renderer *mandel, int y, int chunk_size)
{
	int x;

	for (x = 0; x < mandel->w && !mandel->terminate; x += chunk_size) {
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
		else if (mandel_all_neighbors_same (mandel, parent_x, parent_y, chunk_size << 1))
			do_eval = false;
		else
			do_eval = true;

		if (do_eval) {
			mandel_render_pixel (mandel, x, y);
			mandel_display_rect (mandel, x, y, MIN (chunk_size, mandel->w - x), MIN (chunk_size, mandel->h - y), mandel_get_pixel (mandel, x, y));
		} else {
			mandel_put_pixel (mandel, x, y, mandel_get_pixel (mandel, parent_x, parent_y));
		}
	}
}


static void
calc_sr_mt_pass (struct mandel_renderer *mandel, int chunk_size)
{
	struct sr_state state = {mandel, 0, chunk_size, G_STATIC_MUTEX_INIT};
	GThread *threads[mandel->thread_count];
	int i;

	for (i = 0; i < mandel->thread_count; i++)
		threads[i] = g_thread_create (sr_mt_thread_func, &state, TRUE, NULL);
	for (i = 0; i < mandel->thread_count; i++)
		g_thread_join (threads[i]);
}


static gpointer
sr_mt_thread_func (gpointer data)
{
	struct sr_state *state = (struct sr_state *) data;
	while (!state->renderer->terminate) {
		int y;
		g_static_mutex_lock (&state->mutex);
		y = state->y;
		state->y += state->chunk_size;
		g_static_mutex_unlock (&state->mutex);
		if (y >= state->renderer->h)
			break; /* done */
		calc_sr_row (state->renderer, y, state->chunk_size);
	}
	return NULL;
}


static void
calc_ms_mt (struct mandel_renderer *mandel)
{
	struct ms_state state = {mandel, g_mutex_new (), g_queue_new (), g_cond_new (), 0};
	GThread *threads[mandel->thread_count];
	int i;

	ms_queue_push (&state, 0, 0, mandel->w - 1, mandel->h - 1);

	for (i = 0; i < mandel->thread_count; i++)
		threads[i] = g_thread_create (ms_mt_thread_func, &state, TRUE, NULL);

	for (i = 0; i < mandel->thread_count; i++)
		g_thread_join (threads[i]);

	g_mutex_free (state.mutex);
	g_queue_free (state.queue);
	g_cond_free (state.cond);
}


static gpointer
ms_mt_thread_func (gpointer data)
{
	struct ms_state *state = (struct ms_state *) data;
	struct mandel_renderer *md = state->renderer;
	while (!md->terminate) {
		g_mutex_lock (state->mutex);
		state->idle_threads++;
		/* Notify all waiting threads about the increase of idle_threads */
		g_cond_broadcast (state->cond);
		while (g_queue_is_empty (state->queue)) {
			if (state->idle_threads == md->thread_count) {
				/* Queue is empty, all threads idle. We're done. */
				g_mutex_unlock (state->mutex);
				return NULL;
			}
			g_cond_wait (state->cond, state->mutex);
		}
		state->idle_threads--;
		struct ms_q_entry *entry = g_queue_pop_head (state->queue);
		g_mutex_unlock (state->mutex);

		int x0 = entry->x0, y0 = entry->y0, x1 = entry->x1, y1 = entry->y1;
		free (entry);

		ms_do_work (md, x0, y0, x1, y1, ms_mt_enqueue, state);
	}
	return NULL;
}


static void
ms_queue_push (struct ms_state *state, int x0, int y0, int x1, int y1)
{
	struct ms_q_entry *new_job = malloc (sizeof (struct ms_q_entry));
	new_job->x0 = x0;
	new_job->y0 = y0;
	new_job->x1 = x1;
	new_job->y1 = y1;
	g_mutex_lock (state->mutex);
	g_queue_push_tail (state->queue, new_job);
	g_cond_signal (state->cond);
	g_mutex_unlock (state->mutex);
}


static void
ms_do_work (struct mandel_renderer *md, int x0, int y0, int x1, int y1, void (*enqueue) (int, int, int, int, void *), void *data)
{
	int x, y;
	bool failed = false;
	unsigned p0 = mandel_get_pixel (md, x0, y0);

	for (x = x0; !failed && x <= x1; x++)
		failed = mandel_get_pixel (md, x, y0) != p0 || mandel_get_pixel (md, x, y1) != p0;

	for (y = y0; !failed && y <= y1; y++)
		failed = mandel_get_pixel (md, x0, y) != p0 || mandel_get_pixel (md, x1, y) != p0;

	if (failed) {
		if (x1 - x0 > y1 - y0) {
			unsigned xm = (x0 + x1) / 2;
			for (y = y0 + 1; y < y1; y++)
				mandel_render_pixel (md, xm, y);

			if (xm - x0 > 1)
				enqueue (x0, y0, xm, y1, data);
			if (x1 - xm > 1)
				enqueue (xm, y0, x1, y1, data);
		} else {
			unsigned ym = (y0 + y1) / 2;
			for (x = x0 + 1; x < x1; x++)
				mandel_render_pixel (md, x, ym);

			if (ym - y0 > 1)
				enqueue (x0, y0, x1, ym, data);
			if (y1 - ym > 1)
				enqueue (x0, ym, x1, y1, data);
		}
	} else {
		mandel_put_rect (md, x0 + 1, y0 + 1, x1 - x0 - 1, y1 - y0 - 1, p0);
	}
}


static void
ms_enqueue (int x0, int y0, int x1, int y1, void *data)
{
	struct mandel_renderer *renderer = (struct mandel_renderer *) data;
	calcpart (renderer, x0, y0, x1, y1);
}


static void
ms_mt_enqueue (int x0, int y0, int x1, int y1, void *data)
{
	struct ms_state *state = (struct ms_state *) data;
	ms_queue_push (state, x0, y0, x1, y1);
}


unsigned
mandel_get_precision (const struct mandel_renderer *mandel)
{
	if (mandel->frac_limbs == 0)
		return 0;
	else
		return (mandel->frac_limbs + INT_LIMBS) * mp_bits_per_limb;
}


static bool
is_inside (struct mandel_renderer *md, int x, int y, int iter)
{
	int p = mandel_get_pixel (md, x, y);
	return p == -1 || p == iter;
}


static void
render_btrace (struct mandel_renderer *md, int x0, int y0, unsigned char *flags, bool fill_mode)
{
	int x = x0, y = y0;
	/* XXX is it safe to choose this arbitrarily? */
	int xstep = 0, ystep = -1;
	unsigned inside = mandel_render_pixel (md, x0, y0);

	int turns = 0;
	while (!md->terminate) {
		if (x + xstep < 0 || x + xstep >= md->w || y + ystep < 0 || y + ystep >= md->h || mandel_render_pixel (md, x + xstep, y + ystep) != inside) {
			/* can't move forward, turn left */
			bt_turn_left (xstep, ystep, &xstep, &ystep);
			if (++turns == 4)
				break;
			continue;
		}
		/* move forward */
		turns = 0;
		x += xstep;
		y += ystep;
		if (fill_mode && (xstep == 1 || ystep == 1)) {
			int xfs, yfs;
			bt_turn_left (xstep, ystep, &xfs, &yfs);
			int xf = x, yf = y;
			while (xf >= 0 && yf >= 0 && xf < md->w && yf < md->h && is_inside (md, xf, yf, inside)) {
				flags[xf * md->h + yf] = 1;
				mandel_put_pixel (md, xf, yf, inside);
				xf += xfs;
				yf += yfs;
			}
		}
		if (x == x0 && y == y0)
			break;
		int xsn, ysn;
		bt_turn_right (xstep, ystep, &xsn, &ysn);
		/* If we don't have a wall at the right, turn right. */
		if (x + xsn >= 0 && x + xsn < md->w && y + ysn >= 0 && y + ysn < md->h && mandel_render_pixel (md, x + xsn, y + ysn) == inside) {
			xstep = xsn;
			ystep = ysn;
		}
	}
}


static inline bool
pixel_in_bounds (struct mandel_renderer *renderer, int x, int y)
{
	return x >= 0 && x < renderer->w && y >= 0 && y < renderer->h;
}


static void
render_btrace_test (struct mandel_renderer *md, int x0, int y0, int xstep0, int ystep0, GQueue *queue, unsigned char *flags, bool fill_mode)
{
	int x = x0, y = y0;
	int xstep = xstep0, ystep = ystep0;
	unsigned inside = mandel_render_pixel (md, x0, y0);

	int turns = 0;
	while (!md->terminate) {
		if (fill_mode)
			flags[x * md->h + y] = 1;
		if (!pixel_in_bounds (md, x + xstep, y + ystep) || mandel_render_pixel (md, x + xstep, y + ystep) != inside) {
			if (fill_mode && pixel_in_bounds (md, x + xstep, y + ystep))
				btrace_queue_push (queue, x + xstep, y + ystep, -ystep, xstep);
			/* can't move forward, turn left */
			bt_turn_left (xstep, ystep, &xstep, &ystep);
			if (++turns == 4)
				break;
			continue;
		}
		/* Do the filling if we're looking into the oppisite of the initial
		 * direction, or right of it. It's not exactly clear why this works
		 * (and filling in other directions doesn't), this was determined
		 * empirically... */
		if (fill_mode && ((xstep == -xstep0 && ystep == -ystep0) || (xstep == -ystep0 && ystep == xstep0))) {
			int xfs, yfs;
			bt_turn_left (xstep, ystep, &xfs, &yfs);
			int xf = x + xfs, yf = y + yfs;
			while (pixel_in_bounds (md, xf, yf) && is_inside (md, xf, yf, inside)) {
				flags[xf * md->h + yf] = 1;
				mandel_put_pixel (md, xf, yf, inside);
				xf += xfs;
				yf += yfs;
			}
		}
		/* move forward */
		turns = 0;
		x += xstep;
		y += ystep;
		if (x == x0 && y == y0)
			break;
		int xsn, ysn;
		bt_turn_right (xstep, ystep, &xsn, &ysn);
		/* If we don't have a wall at the right, turn right. */
		if (pixel_in_bounds (md, x + xsn, y + ysn) && mandel_render_pixel (md, x + xsn, y + ysn) == inside) {
			xstep = xsn;
			ystep = ysn;
		} else if (fill_mode && pixel_in_bounds (md, x + xsn, y + ysn))
			btrace_queue_push (queue, x + xsn, y + ysn, -xstep, -ystep);
	}
}


static void
bt_turn_right (int xs, int ys, int *xsn, int *ysn)
{
	*xsn = -ys;
	*ysn = xs;
}


static void
bt_turn_left (int xs, int ys, int *xsn, int *ysn)
{
	*xsn = ys;
	*ysn = -xs;
}


static unsigned *
pascal_triangle (unsigned n)
{
	unsigned i, j, *v = malloc ((n + 1) * sizeof (*v));
	v[0] = 1;
	for (i = 1; i <= n; i++)
		v[i] = 0;
	for (i = 1; i <= n; i++) {
		unsigned oldv = v[0];
		for (j = 1; j < i + 1; j++) {
			unsigned tmp = v[j];
			v[j] += oldv;
			oldv = tmp;
		}
	}
	return v;
}


static inline mandel_fp_t
stored_power_fp (mandel_fp_t x, unsigned n, mandel_fp_t *powers)
{
	switch (n) {
		case 0:
			return 1.0;
		case 1:
			return x;
		default:
			return powers[n - 2];
	}
}


static void
store_powers_fp (mandel_fp_t *powers, mandel_fp_t x, unsigned n)
{
	int i;
	powers[0] = x * x;
	for (i = 1; i < n - 1; i++) {
		powers[i] = powers[i - 1] * x;
	}
}


static void
complex_pow_fp (mandel_fp_t xreal, mandel_fp_t ximag, unsigned n, mandel_fp_t *rreal, mandel_fp_t *rimag, unsigned *pascal)
{
	mandel_fp_t real_powers[n - 1], imag_powers[n - 1];
	store_powers_fp (real_powers, xreal, n);
	store_powers_fp (imag_powers, ximag, n);
	mandel_fp_t real = 0.0, imag = 0.0;
	unsigned j;
	for (j = 0; j <= n; j++) {
		mandel_fp_t cur = stored_power_fp (xreal, n - j, real_powers) * stored_power_fp (ximag, j, imag_powers) * pascal[j];
		switch (j % 4) {
			case 0: /* i^0 = 1: add to real */
				real += cur;
				break;
			case 1: /* i^1 = i: add to imag */
				imag += cur;
				break;
			case 2: /* i^2 = -1: subtract from real */
				real -= cur;
				break;
			case 3: /* i^3 = -i: subtract from imag */
				imag -= cur;
				break;
		}
	}
	*rreal = real;
	*rimag = imag;
}


static void
store_powers (mp_limb_t *powers, bool *signs, mp_limb_t *x, bool xsign, unsigned n, unsigned frac_limbs)
{
	unsigned total_limbs = frac_limbs + INT_LIMBS;
	int i;
	/* set x^0 to 1 */
	for (i = 0; i < total_limbs; i++)
		powers[i] = 0;
	powers[frac_limbs] = 1;
	signs[0] = false;

	/* set x^1 to x */
	memcpy (powers + total_limbs, x, total_limbs * sizeof (mp_limb_t));
	signs[1] = xsign;

	for (i = 2; i <= n; i++) {
		my_mpn_mul_fast (powers + total_limbs * i, powers + total_limbs * (i - 1), x, frac_limbs);
		signs[i] = xsign && !signs[i - 1];
	}
}


static void
complex_pow (mp_limb_t *xreal, bool xreal_sign, mp_limb_t *ximag, bool ximag_sign, unsigned n, mp_limb_t *real, bool *rreal_sign, mp_limb_t *imag, bool *rimag_sign, unsigned frac_limbs, unsigned *pascal)
{
	unsigned total_limbs = INT_LIMBS + frac_limbs;
	mp_limb_t real_powers[(n + 1) * total_limbs], imag_powers[(n + 1) * total_limbs];
	bool real_psigns[n + 1], imag_psigns[n + 1];
	int i;
	bool real_sign = false, imag_sign = false;

	store_powers (real_powers, real_psigns, xreal, xreal_sign, n, frac_limbs);
	store_powers (imag_powers, imag_psigns, ximag, ximag_sign, n, frac_limbs);

	for (i = 0; i < total_limbs; i++)
		real[i] = imag[i] = 0;

	unsigned j;
	for (j = 0; j <= n; j++) {
		mp_limb_t cur[total_limbs], tmp1[total_limbs];
		bool cur_sign;
		my_mpn_mul_fast (tmp1, real_powers + (n - j) * total_limbs, imag_powers + j * total_limbs, frac_limbs);
		cur_sign = real_psigns[n - j] != imag_psigns[j];
		/* we could do mpn_mul_1() in-place, but performance may be better if no overlap */
		mpn_mul_1 (cur, tmp1, total_limbs, (mp_limb_t) pascal[j]);
		switch (j % 4) {
			case 0: /* i^0 = 1: add to real */
				real_sign = my_mpn_add_signed (real, real, real_sign, cur, cur_sign, frac_limbs);
				break;
			case 1: /* i^1 = i: add to imag */
				imag_sign = my_mpn_add_signed (imag, imag, imag_sign, cur, cur_sign, frac_limbs);
				break;
			case 2: /* i^2 = -1: subtract from real */
				real_sign = my_mpn_add_signed (real, real, real_sign, cur, !cur_sign, frac_limbs);
				break;
			case 3: /* i^3 = -i: subtract from imag */
				imag_sign = my_mpn_add_signed (imag, imag, imag_sign, cur, !cur_sign, frac_limbs);
				break;
		}
	}
	*rreal_sign = real_sign;
	*rimag_sign = imag_sign;
}


unsigned
mandel_julia (const struct mandel_renderer *renderer, mp_limb_t *x0, bool x0_sign, mp_limb_t *y0, bool y0_sign, mp_limb_t *preal, bool preal_sign, mp_limb_t *pimag, bool pimag_sign, unsigned maxiter, unsigned frac_limbs)
{
	if (renderer->md->zpower == 2)
		return mandel_julia_z2 (x0, x0_sign, y0, y0_sign, preal, preal_sign, pimag, pimag_sign, maxiter, frac_limbs);
	else
		return mandel_julia_zpower (renderer, x0, x0_sign, y0, y0_sign, preal, preal_sign, pimag, pimag_sign, maxiter, frac_limbs);
}


unsigned
mandel_julia_fp (const struct mandel_renderer *renderer, mandel_fp_t x0, mandel_fp_t y0, mandel_fp_t preal, mandel_fp_t pimag, unsigned maxiter)
{
	if (renderer->md->zpower == 2)
		return mandel_julia_z2_fp (x0, y0, preal, pimag, maxiter);
	else
		return mandel_julia_zpower_fp (renderer, x0, y0, preal, pimag, maxiter);
}


static void
mandeldata_init_mpvars (struct mandeldata *md)
{
	mpf_init (md->cx);
	mpf_init (md->cy);
	mpf_init (md->magf);
}


void
mandeldata_init (struct mandeldata *md)
{
	memset (md, 0, sizeof (*md));
	mandeldata_init_mpvars (md);
}


void
mandeldata_clear (struct mandeldata *md)
{
	mpf_clear (md->cx);
	mpf_clear (md->cy);
	mpf_clear (md->magf);
}


void
mandeldata_clone (struct mandeldata *clone, const struct mandeldata *orig)
{
	memcpy (clone, orig, sizeof (*clone));
	mandeldata_init_mpvars (clone);
	mpf_set (clone->cx, orig->cx);
	mpf_set (clone->cy, orig->cy);
	mpf_set (clone->magf, orig->magf);
}


static void
btrace_queue_push (GQueue *queue, int x, int y, int xstep, int ystep)
{
	struct btrace_q_entry *entry = malloc (sizeof (*entry));
	entry->x = x;
	entry->y = y;
	entry->xstep = xstep;
	entry->ystep = ystep;
	g_queue_push_tail (queue, entry);
}


static void
btrace_queue_pop (GQueue *queue, int *x, int *y, int *xstep, int *ystep)
{
	struct btrace_q_entry *entry = g_queue_pop_head (queue);
	*x = entry->x;
	*y = entry->y;
	*xstep = entry->xstep;
	*ystep = entry->ystep;
	free (entry);
}
