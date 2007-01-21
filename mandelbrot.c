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
static void complex_pow_fp (mandel_fp_t xreal, mandel_fp_t ximag, unsigned n, mandel_fp_t *rreal, mandel_fp_t *rimag, const unsigned *pascal);
static void store_powers (mp_ptr powers, bool *signs, mp_srcptr x, bool xsign, unsigned n, unsigned frac_limbs);
static void complex_pow (mp_srcptr xreal, bool xreal_sign, mp_srcptr ximag, bool ximag_sign, unsigned n, mp_ptr real, bool *rreal_sign, mp_ptr imag, bool *rimag_sign, unsigned frac_limbs, const unsigned *pascal);
static unsigned mandel_julia_z2 (struct mandel_julia_state *state, const struct mandel_julia_param *param, mpf_srcptr x0f, mpf_srcptr y0f, mpf_srcptr prealf, mpf_srcptr pimagf);
static unsigned mandel_julia_zpower (struct mandel_julia_state *state, const struct mandel_julia_param *param, mpf_srcptr x0f, mpf_srcptr y0f, mpf_srcptr prealf, mpf_srcptr pimagf);
static unsigned mandel_julia_z2_fp (struct mandel_julia_state *state, const struct mandel_julia_param *param, mandel_fp_t x0, mandel_fp_t y0, mandel_fp_t preal, mandel_fp_t pimag);
static unsigned mandel_julia_zpower_fp (struct mandel_julia_state *state, const struct mandel_julia_param *param, mandel_fp_t x0, mandel_fp_t y0, mandel_fp_t preal, mandel_fp_t pimag);
static void mandeldata_init_mpvars (struct mandeldata *md);
static void btrace_queue_push (GQueue *queue, int x, int y, int xstep, int ystep);
static void btrace_queue_pop (GQueue *queue, int *x, int *y, int *xstep, int *ystep);
static bool my_mpf_get_mpn (mp_ptr rop, mpf_srcptr op, unsigned frac_limbs);

static void mandel_julia_state_init (struct mandel_julia_state *state, const struct mandel_julia_param *param);
static void mandel_julia_state_clear (struct mandel_julia_state *state);

static void *mandelbrot_param_new (void);
static void *mandelbrot_param_clone (const void *orig);
static void mandelbrot_param_free (void *param);
static void *mandelbrot_state_new (const void *param, unsigned frac_limbs);
static void mandelbrot_state_free (void *state);
static unsigned mandelbrot_compute (void *state, mpf_srcptr real, mpf_srcptr imag, mpfr_ptr distance);
static unsigned mandelbrot_compute_fp (void *state, mandel_fp_t real, mandel_fp_t imag, mandel_fp_t *distance);

static void *julia_param_new (void);
static void *julia_param_clone (const void *orig);
static void julia_param_free (void *param);
static void *julia_state_new (const void *param, unsigned frac_limbs);
static void julia_state_free (void *state);
static unsigned julia_compute (void *state, mpf_srcptr real, mpf_srcptr imag, mpfr_ptr distance);
static unsigned julia_compute_fp (void *state, mandel_fp_t real, mandel_fp_t imag, mandel_fp_t *distance);


const char *const render_method_names[] = {
	"Successive Refinement",
	"Mariani-Silver",
	"Boundary Tracing"
};


const struct fractal_type fractal_types[] = {
	{
		FRACTAL_MANDELBROT, "mandelbrot", "Mandelbrot Set",
		FRAC_TYPE_ESCAPE_ITER,
		mandelbrot_param_new,
		mandelbrot_param_clone,
		mandelbrot_param_free,
		mandelbrot_state_new,
		mandelbrot_state_free,
		mandelbrot_compute,
		mandelbrot_compute_fp
	},
	{
		FRACTAL_JULIA, "julia", "Julia Set",
		FRAC_TYPE_ESCAPE_ITER,
		julia_param_new,
		julia_param_clone,
		julia_param_free,
		julia_state_new,
		julia_state_free,
		julia_compute,
		julia_compute_fp
	}
};


void
mandel_convert_x_f (const struct mandel_renderer *mandel, mpf_ptr rop, unsigned op)
{
	mpf_sub (rop, mandel->xmax_f, mandel->xmin_f);
	mpf_mul_ui (rop, rop, op);
	mpf_div_ui (rop, rop, mandel->w);
	mpf_add (rop, rop, mandel->xmin_f);
}


void
mandel_convert_y_f (const struct mandel_renderer *mandel, mpf_ptr rop, unsigned op)
{
	mpf_sub (rop, mandel->ymin_f, mandel->ymax_f);
	mpf_mul_ui (rop, rop, op);
	mpf_div_ui (rop, rop, mandel->h);
	mpf_add (rop, rop, mandel->ymax_f);
}


void
mandel_set_pixel (struct mandel_renderer *mandel, int x, int y, unsigned iter)
{
	volatile int *px = mandel->data + x * mandel->h + y;
	if (*px < 0)
		g_atomic_int_inc (&mandel->pixels_done);
	*px = iter;
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
my_mpn_mul_fast (mp_ptr p, mp_srcptr f0, mp_srcptr f1, unsigned frac_limbs)
{
	unsigned total_limbs = INT_LIMBS + frac_limbs;
	mp_limb_t tmp[total_limbs * 2];
	int i;
	mpn_mul (tmp, f0, total_limbs, f1, total_limbs);
	for (i = 0; i < total_limbs; i++)
		p[i] = tmp[frac_limbs + i];
}


bool
my_mpn_add_signed (mp_ptr rop, mp_srcptr op1, bool op1_sign, mp_srcptr op2, bool op2_sign, unsigned frac_limbs)
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
my_mpn_invert (mp_ptr op, unsigned total_limbs)
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
mandel_julia_z2 (struct mandel_julia_state *state, const struct mandel_julia_param *param, mpf_srcptr x0f, mpf_srcptr y0f, mpf_srcptr prealf, mpf_srcptr pimagf)
{
	const unsigned frac_limbs = state->frac_limbs;
	const unsigned total_limbs = INT_LIMBS + frac_limbs;
	const unsigned maxiter = param->maxiter;
	mp_limb_t x0[total_limbs], y0[total_limbs], preal[total_limbs], pimag[total_limbs];
	bool x0_sign, y0_sign, preal_sign, pimag_sign;
	mp_limb_t x[total_limbs], y[total_limbs], xsqr[total_limbs], ysqr[total_limbs], sqrsum[total_limbs], four[INT_LIMBS];
	mp_limb_t cd_x[total_limbs], cd_y[total_limbs];
	unsigned i;

	x0_sign = my_mpf_get_mpn (x0, x0f, frac_limbs);
	y0_sign = my_mpf_get_mpn (y0, y0f, frac_limbs);
	preal_sign = my_mpf_get_mpn (preal, prealf, frac_limbs);
	pimag_sign = my_mpf_get_mpn (pimag, pimagf, frac_limbs);

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
mandel_julia_zpower (struct mandel_julia_state *state, const struct mandel_julia_param *param, mpf_srcptr x0f, mpf_srcptr y0f, mpf_srcptr prealf, mpf_srcptr pimagf)
{
	const unsigned frac_limbs = state->frac_limbs;
	const unsigned total_limbs = INT_LIMBS + frac_limbs;
	const unsigned maxiter = param->maxiter;
	const unsigned zpower = param->zpower;
	const unsigned *const ptri = state->ptriangle;
	mp_limb_t x0[total_limbs], y0[total_limbs], preal[total_limbs], pimag[total_limbs];
	bool x0_sign, y0_sign, preal_sign, pimag_sign;
	mp_limb_t x[total_limbs], y[total_limbs], xsqr[total_limbs], ysqr[total_limbs], sqrsum[total_limbs], four[INT_LIMBS];
	mp_limb_t cd_x[total_limbs], cd_y[total_limbs];
	unsigned i;

	x0_sign = my_mpf_get_mpn (x0, x0f, frac_limbs);
	y0_sign = my_mpf_get_mpn (y0, y0f, frac_limbs);
	preal_sign = my_mpf_get_mpn (preal, prealf, frac_limbs);
	pimag_sign = my_mpf_get_mpn (pimag, pimagf, frac_limbs);

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
mandel_julia_z2_fp (struct mandel_julia_state *state, const struct mandel_julia_param *param, mandel_fp_t x0, mandel_fp_t y0, mandel_fp_t preal, mandel_fp_t pimag)
{
	const unsigned maxiter = param->maxiter;
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
mandel_julia_zpower_fp (struct mandel_julia_state *state, const struct mandel_julia_param *param, mandel_fp_t x0, mandel_fp_t y0, mandel_fp_t preal, mandel_fp_t pimag)
{
	const unsigned maxiter = param->maxiter;
	const unsigned zpower = param->zpower;
	const unsigned *const ptri = state->ptriangle;
	unsigned i = 0, k = 1, m = 1;
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
		i = mandel->md->type->compute_fp (mandel->fractal_state, xf, yf, NULL);
	} else {
		// MP
		unsigned total_limbs = INT_LIMBS + mandel->frac_limbs;
		mpf_t x0, y0;
		mpf_init2 (x0, total_limbs * GMP_NUMB_BITS);
		mpf_init2 (y0, total_limbs * GMP_NUMB_BITS);

		mandel_convert_x_f (mandel, x0, x);
		mandel_convert_y_f (mandel, y0, y);

		i = mandel->md->type->compute (mandel->fractal_state, x0, y0, NULL);

		mpf_clear (x0);
		mpf_clear (y0);
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
	renderer->data = NULL;
	renderer->terminate = false;
	renderer->display_pixel = NULL;
	renderer->display_rect = NULL;
	mpf_init (renderer->xmin_f);
	mpf_init (renderer->xmax_f);
	mpf_init (renderer->ymin_f);
	mpf_init (renderer->ymax_f);

	renderer->md = md;
	renderer->w = w;
	renderer->h = h;
	g_atomic_int_set (&renderer->pixels_done, 0);

	renderer->aspect = (double) renderer->w / renderer->h;
	center_to_corners (renderer->xmin_f, renderer->xmax_f, renderer->ymin_f, renderer->ymax_f, renderer->md->area.center.real, renderer->md->area.center.imag, renderer->md->area.magf, renderer->aspect);

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
		renderer->frac_limbs = (required_bits + mp_bits_per_limb - 1) / mp_bits_per_limb;

	unsigned frac_limbs = renderer->frac_limbs;

	renderer->fractal_state = renderer->md->type->state_new (renderer->md->type_param, frac_limbs);

	renderer->data = malloc (renderer->w * renderer->h * sizeof (*renderer->data));
}


void
mandel_renderer_clear (struct mandel_renderer *renderer)
{
	if (renderer->fractal_state != NULL)
		renderer->md->type->state_free (renderer->fractal_state);
	free_not_null (renderer->data);
	mpf_clear (renderer->xmin_f);
	mpf_clear (renderer->xmax_f);
	mpf_clear (renderer->ymin_f);
	mpf_clear (renderer->ymax_f);
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
complex_pow_fp (mandel_fp_t xreal, mandel_fp_t ximag, unsigned n, mandel_fp_t *rreal, mandel_fp_t *rimag, const unsigned *pascal)
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
store_powers (mp_ptr powers, bool *signs, mp_srcptr x, bool xsign, unsigned n, unsigned frac_limbs)
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
complex_pow (mp_srcptr xreal, bool xreal_sign, mp_srcptr ximag, bool ximag_sign, unsigned n, mp_ptr real, bool *rreal_sign, mp_ptr imag, bool *rimag_sign, unsigned frac_limbs, const unsigned *pascal)
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
mandel_julia (struct mandel_julia_state *state, const struct mandel_julia_param *param, mpf_srcptr x0f, mpf_srcptr y0f, mpf_srcptr prealf, mpf_srcptr pimagf)
{
	if (param->zpower == 2)
		return mandel_julia_z2 (state, param, x0f, y0f, prealf, pimagf);
	else
		return mandel_julia_zpower (state, param, x0f, y0f, prealf, pimagf);
}


unsigned
mandel_julia_fp (struct mandel_julia_state *state, const struct mandel_julia_param *param, mandel_fp_t x0, mandel_fp_t y0, mandel_fp_t preal, mandel_fp_t pimag)
{
	if (param->zpower == 2)
		return mandel_julia_z2_fp (state, param, x0, y0, preal, pimag);
	else
		return mandel_julia_zpower_fp (state, param, x0, y0, preal, pimag);
}


static void
mandeldata_init_mpvars (struct mandeldata *md)
{
	mandel_area_init (&md->area);
}


void
mandeldata_init (struct mandeldata *md, const struct fractal_type *type)
{
	memset (md, 0, sizeof (*md));
	md->type = type;
	mandeldata_init_mpvars (md);
	md->type_param = type->param_new ();
}


void
mandeldata_clear (struct mandeldata *md)
{
	mandel_area_clear (&md->area);
}


void
mandeldata_clone (struct mandeldata *clone, const struct mandeldata *orig)
{
	memcpy (clone, orig, sizeof (*clone));
	mandeldata_init_mpvars (clone);
	mpf_set (clone->area.center.real, orig->area.center.real);
	mpf_set (clone->area.center.imag, orig->area.center.imag);
	mpf_set (clone->area.magf, orig->area.magf);
	clone->type_param = orig->type->param_clone (orig->type_param);
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


void
mandel_point_init (struct mandel_point *point)
{
	mpf_init (point->real);
	mpf_init (point->imag);
}


void
mandel_point_clear (struct mandel_point *point)
{
	mpf_clear (point->real);
	mpf_clear (point->imag);
}


void
mandel_area_init (struct mandel_area *area)
{
	mandel_point_init (&area->center);
	mpf_init (area->magf);
}


void
mandel_area_clear (struct mandel_area *area)
{
	mandel_point_clear (&area->center);
	mpf_clear (area->magf);
}


static bool
my_mpf_get_mpn (mp_ptr rop, mpf_srcptr op, unsigned frac_limbs)
{
	const unsigned total_limbs = INT_LIMBS + frac_limbs;
	int i;
	mpf_t f;
	mpz_t z;
	mpf_init2 (f, total_limbs * GMP_NUMB_BITS);
	mpz_init (z);
	mpf_mul_2exp (f, op, frac_limbs * GMP_NUMB_BITS);
	mpz_set_f (z, f);
	mpf_clear (f);
	for (i = 0; i < total_limbs; i++)
		rop[i] = mpz_getlimbn (z, i);
	mpz_clear (z);
	return mpf_sgn (op) < 0;
}


double
mandel_renderer_progress (const struct mandel_renderer *renderer)
{
	return (double) g_atomic_int_get (&renderer->pixels_done) / (renderer->w * renderer->h);
}


static void *
mandelbrot_param_new (void)
{
	struct mandelbrot_param *param = malloc (sizeof (*param));
	memset (param, 0, sizeof (*param));
	param->mjparam.zpower = 2;
	param->mjparam.maxiter = 1000;
	return (void *) param;
}


static void *
mandelbrot_param_clone (const void *orig)
{
	struct mandelbrot_param *param = malloc (sizeof (*param));
	memcpy (param, orig, sizeof (*param));
	return (void *) param;
}


static void
mandelbrot_param_free (void *param)
{
	free (param);
}


static void *
mandelbrot_state_new (const void *param_, unsigned frac_limbs)
{
	const struct mandelbrot_param *param = (struct mandelbrot_param *) param_;
	struct mandelbrot_state *state = malloc (sizeof (*state));
	memset (state, 0, sizeof (*state));
	state->mjstate.frac_limbs = frac_limbs;
	state->param = param;
	mandel_julia_state_init (&state->mjstate, &param->mjparam);
	return (void *) state;
}


static void
mandelbrot_state_free (void *state_)
{
	struct mandelbrot_state *state = (struct mandelbrot_state *) state_;
	mandel_julia_state_clear (&state->mjstate);
}


static unsigned
mandelbrot_compute (void *state_, mpf_srcptr real, mpf_srcptr imag, mpfr_ptr distance)
{
	struct mandelbrot_state *state = (struct mandelbrot_state *) state_;
	const struct mandelbrot_param *param = state->param;
	return mandel_julia (&state->mjstate, &param->mjparam, real, imag, real, imag);
}


static unsigned
mandelbrot_compute_fp (void *state_, mandel_fp_t real, mandel_fp_t imag, mandel_fp_t *distance)
{
	struct mandelbrot_state *state = (struct mandelbrot_state *) state_;
	const struct mandelbrot_param *param = state->param;
	return mandel_julia_fp (&state->mjstate, &param->mjparam, real, imag, real, imag);
}


static void *
julia_param_new (void)
{
	struct julia_param *param = malloc (sizeof (*param));
	memset (param, 0, sizeof (*param));
	param->mjparam.zpower = 2;
	param->mjparam.maxiter = 1000;
	mpf_init (param->param.real);
	mpf_init (param->param.imag);
	/* XXX initialize param to default value */
	return (void *) param;
}


static void *
julia_param_clone (const void *orig_)
{
	struct julia_param *param = malloc (sizeof (*param));
	const struct julia_param *orig = (const struct julia_param *) orig_;
	memcpy (param, orig, sizeof (*param));
	mpf_init (param->param.real);
	mpf_init (param->param.imag);
	mpf_set (param->param.real, orig->param.real);
	mpf_set (param->param.imag, orig->param.imag);
	return (void *) param;
}


static void
julia_param_free (void *param_)
{
	struct julia_param *param = (struct julia_param *) param_;
	mpf_clear (param->param.real);
	mpf_clear (param->param.imag);
	free (param);
}


static void *
julia_state_new (const void *param_, unsigned frac_limbs)
{
	const struct julia_param *param = (struct julia_param *) param_;
	struct julia_state *state = malloc (sizeof (*state));
	state->mjstate.frac_limbs = frac_limbs;
	memset (state, 0, sizeof (*state));
	state->param = param;
	mandel_julia_state_init (&state->mjstate, &param->mjparam);
	if (frac_limbs == 0) {
		state->mpvars.fp.preal_float = mpf_get_mandel_fp (param->param.real);
		state->mpvars.fp.pimag_float = mpf_get_mandel_fp (param->param.imag);
	}
	return (void *) state;
}


static void
julia_state_free (void *state_)
{
	struct julia_state *state = (struct julia_state *) state_;
	mandel_julia_state_clear (&state->mjstate);
}


static unsigned
julia_compute (void *state_, mpf_srcptr real, mpf_srcptr imag, mpfr_ptr distance)
{
	struct julia_state *state = (struct julia_state *) state_;
	const struct julia_param *param = state->param;
	return mandel_julia (&state->mjstate, &param->mjparam, real, imag, param->param.real, param->param.imag);
}


static unsigned
julia_compute_fp (void *state_, mandel_fp_t real, mandel_fp_t imag, mandel_fp_t *distance)
{
	struct julia_state *state = (struct julia_state *) state_;
	const struct julia_param *param = state->param;
	return mandel_julia_fp (&state->mjstate, &param->mjparam, real, imag, state->mpvars.fp.preal_float, state->mpvars.fp.pimag_float);
}


static void
mandel_julia_state_init (struct mandel_julia_state *state, const struct mandel_julia_param *param)
{
	if (param->zpower > 2)
		state->ptriangle = pascal_triangle (param->zpower);
}


static void
mandel_julia_state_clear (struct mandel_julia_state *state)
{
	if (state->ptriangle != NULL)
		free (state->ptriangle);
}


const struct fractal_type *
fractal_type_by_id (fractal_type_t id)
{
	return &fractal_types[id];
}


const struct fractal_type *
fractal_type_by_name (const char *name)
{
	fractal_type_t i;
	for (i = 0; i < FRACTAL_MAX; i++)
		if (strcmp (name, fractal_types[i].name) == 0)
			return &fractal_types[i];
	return NULL;
}
