#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#include <gmp.h>
#include <mpfr.h>

#include "fpdefs.h"
#include "misc-math.h"
#include "fractal-math.h"

struct mandel_julia_state;
struct mandelbrot_state;
struct julia_state;

struct mandel_julia_state {
	unsigned frac_limbs;
	fractal_type_flags_t flags;
};

struct mandelbrot_state {
	struct mandel_julia_state mjstate;
	const struct mandelbrot_param *param;
};

struct julia_state {
	struct mandel_julia_state mjstate;
	const struct julia_param *param;
	union {
		struct {
			mandel_fp_t preal_float, pimag_float;
		} fp;
	} mpvars;
};


static bool mandel_julia (struct mandel_julia_state *state, const struct mandel_julia_param *param, mpf_srcptr x0f, mpf_srcptr y0f, mpf_srcptr prealf, mpf_srcptr pimagf, unsigned *iter, mpfr_ptr distance);
#ifdef MANDELBROT_FP_ASM
unsigned mandelbrot_fp (mandel_fp_t x0, mandel_fp_t y0, unsigned maxiter);
#else
static unsigned mandelbrot_fp (mandel_fp_t x0, mandel_fp_t y0, unsigned maxiter);
#endif
static bool mandel_julia_fp (struct mandel_julia_state *state, const struct mandel_julia_param *param, mandel_fp_t x0, mandel_fp_t y0, mandel_fp_t preal, mandel_fp_t pimag, unsigned *iter, mandel_fp_t *distance);
static unsigned mandel_julia_z2 (struct mandel_julia_state *state, const struct mandel_julia_param *param, mpf_srcptr x0f, mpf_srcptr y0f, mpf_srcptr prealf, mpf_srcptr pimagf, mpfr_ptr distance);
static unsigned mandel_julia_zpower (struct mandel_julia_state *state, const struct mandel_julia_param *param, mpf_srcptr x0f, mpf_srcptr y0f, mpf_srcptr prealf, mpf_srcptr pimagf, mpfr_ptr distance);
static unsigned mandel_julia_z2_fp (struct mandel_julia_state *state, const struct mandel_julia_param *param, mandel_fp_t x0, mandel_fp_t y0, mandel_fp_t preal, mandel_fp_t pimag, mandel_fp_t *distance);
static unsigned mandel_julia_zpower_fp (struct mandel_julia_state *state, const struct mandel_julia_param *param, mandel_fp_t x0, mandel_fp_t y0, mandel_fp_t preal, mandel_fp_t pimag, mandel_fp_t *distance);

static void mandel_julia_state_init (struct mandel_julia_state *state, const struct mandel_julia_param *param);
static void mandel_julia_state_clear (struct mandel_julia_state *state);

static void *mandelbrot_param_new (void);
static void *mandelbrot_param_clone (const void *orig);
static void mandelbrot_param_free (void *param);
static void *mandelbrot_state_new (const void *md, fractal_type_flags_t flags, unsigned frac_limbs);
static void mandelbrot_state_free (void *state);
static bool mandelbrot_compute (void *state, mpf_srcptr real, mpf_srcptr imag, unsigned *iter, mpfr_ptr distance);
static bool mandelbrot_compute_fp (void *state, mandel_fp_t real, mandel_fp_t imag, unsigned *iter, mandel_fp_t *distance);

static void *julia_param_new (void);
static void *julia_param_clone (const void *orig);
static void julia_param_free (void *param);
static void *julia_state_new (const void *md, fractal_type_flags_t flags, unsigned frac_limbs);
static void julia_state_free (void *state);
static bool julia_compute (void *state, mpf_srcptr real, mpf_srcptr imag, unsigned *iter, mpfr_ptr distance);
static bool julia_compute_fp (void *state, mandel_fp_t real, mandel_fp_t imag, unsigned *iter, mandel_fp_t *distance);


static const struct fractal_type fractal_types[] = {
	{
		FRACTAL_MANDELBROT, "mandelbrot", "Mandelbrot Set",
		FRAC_TYPE_ESCAPE_ITER | FRAC_TYPE_DISTANCE,
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


static unsigned
mandel_julia_z2 (struct mandel_julia_state *state, const struct mandel_julia_param *param, mpf_srcptr x0f, mpf_srcptr y0f, mpf_srcptr prealf, mpf_srcptr pimagf, mpfr_ptr distance)
{
	const bool distance_est = (state->flags & FRAC_TYPE_DISTANCE) != 0;
	const unsigned frac_limbs = state->frac_limbs;
	const unsigned total_limbs = INT_LIMBS + frac_limbs;
	const unsigned maxiter = param->maxiter;
	mp_limb_t x0[total_limbs], y0[total_limbs], preal[total_limbs], pimag[total_limbs];
	bool x0_sign, y0_sign, preal_sign, pimag_sign;
	mp_limb_t x[total_limbs], y[total_limbs], xsqr[total_limbs], ysqr[total_limbs], sqrsum[total_limbs], four[INT_LIMBS];
	mp_limb_t cd_x[total_limbs], cd_y[total_limbs];
	mp_limb_t tmp1[total_limbs];
	unsigned i;
	mpf_t dx, dy, xf, yf, ftmp1, ftmp2, ftmp3;

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

	if (distance_est) {
		mpf_init2 (dx, total_limbs * GMP_NUMB_BITS);
		mpf_init2 (dy, total_limbs * GMP_NUMB_BITS);
		mpf_init2 (xf, total_limbs * GMP_NUMB_BITS);
		mpf_init2 (yf, total_limbs * GMP_NUMB_BITS);
		mpf_init2 (ftmp1, total_limbs * GMP_NUMB_BITS);
		mpf_init2 (ftmp2, total_limbs * GMP_NUMB_BITS);
		mpf_init2 (ftmp3, total_limbs * GMP_NUMB_BITS);
		mpf_set_ui (dx, 0);
		mpf_set_ui (dy, 0);
	}

	bool x_sign = x0_sign, y_sign = y0_sign;

	int k = 1, m = 1;
	i = 0;
	my_mpn_mul_fast (xsqr, x, x, frac_limbs);
	my_mpn_mul_fast (ysqr, y, y, frac_limbs);
	mpn_add_n (sqrsum, xsqr, ysqr, total_limbs);
	while (i < maxiter && mpn_cmp (sqrsum + frac_limbs, four, INT_LIMBS) < 0) {
		if (distance_est) {
			my_mpn_get_mpf (xf, x, x_sign, frac_limbs);
			my_mpn_get_mpf (yf, y, y_sign, frac_limbs);
			/* tmp1 = dx * x */
			mpf_mul (ftmp1, dx, xf);
			/* tmp2 = dy * y */
			mpf_mul (ftmp2, dy, yf);
			/* tmp1 = tmp1 - tmp2 */
			mpf_sub (ftmp1, ftmp1, ftmp2);
			/* tmp2 = 2 * tmp1 */
			mpf_mul_2exp (ftmp2, ftmp1, 1);
			/* tmp1 = tmp2 + 1 */
			mpf_add_ui (ftmp1, ftmp2, 1);
			/* tmp2 = dx * y */
			mpf_mul (ftmp2, dx, yf);
			/* tmp3 = x * dy */
			mpf_mul (ftmp3, xf, dy);
			/* tmp2 = tmp2 + tmp3 */
			mpf_add (ftmp2, ftmp2, ftmp3);
			/* dy = 2 * tmp2 */
			mpf_mul_2exp (dy, ftmp2, 1);
			/* dx = tmp1 */
			mpf_set (dx, ftmp1);
		}

		my_mpn_mul_fast (tmp1, x, y, frac_limbs);
		mpn_lshift (y, tmp1, total_limbs, 1);
		y_sign = my_mpn_add_signed (y, y, x_sign != y_sign, pimag, pimag_sign, frac_limbs);
		x_sign = my_mpn_add_signed (x, xsqr, false, ysqr, true, frac_limbs);
		x_sign = my_mpn_add_signed (x, x, x_sign, preal, preal_sign, frac_limbs);

		k--;
		/* FIXME we must compare the signs here! */
		if (mpn_cmp (x, cd_x, total_limbs) == 0 && mpn_cmp (y, cd_y, total_limbs) == 0) {
			//printf ("* Cycle of length %d detected after %u iterations.\n", m - k + 1, i);
			// XXX iter_saved += maxiter - i;
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

	if (distance_est) {
		my_mpn_get_mpf (xf, x, x_sign, frac_limbs);
		my_mpn_get_mpf (yf, y, y_sign, frac_limbs);
		mpf_mul (xf, xf, xf);
		mpf_mul (yf, yf, yf);
		mpf_add (xf, xf, yf);
		mpf_sqrt (xf, xf);
		//double zabs = mpf_get_d (xf);
		mpfr_t zabs;
		mpfr_init2 (zabs, 1024); /* XXX */
		mpfr_set_f (zabs, xf, GMP_RNDN);
		mpf_mul (dx, dx, dx);
		mpf_mul (dy, dy, dy);
		mpf_add (dx, dx, dy);
		mpf_sqrt (dx, dx);
		//double dzabs = mpf_get_d (dxf);
		mpfr_t dzabs;
		mpfr_init2 (dzabs, 1024); /* XXX */
		mpfr_set_f (dzabs, dx, GMP_RNDN);
		//double distance = log (zabs * zabs) * zabs / dzabs;
		mpfr_sqr (distance, zabs, GMP_RNDN);
		mpfr_log (distance, distance, GMP_RNDN);
		mpfr_mul (distance, distance, zabs, GMP_RNDN);
		mpfr_div (distance, distance, dzabs, GMP_RNDN);

		mpf_clear (xf);
		mpf_clear (yf);
		mpf_clear (dx);
		mpf_clear (dy);
		mpf_clear (ftmp1);
		mpf_clear (ftmp2);
		mpf_clear (ftmp3);
		mpfr_clear (zabs);
		mpfr_clear (dzabs);
	}

	return i;
}


static unsigned
mandel_julia_zpower (struct mandel_julia_state *state, const struct mandel_julia_param *param, mpf_srcptr x0f, mpf_srcptr y0f, mpf_srcptr prealf, mpf_srcptr pimagf, mpfr_ptr distance)
{
	const bool distance_est = (state->flags & FRAC_TYPE_DISTANCE) != 0;
	const unsigned frac_limbs = state->frac_limbs;
	const unsigned total_limbs = INT_LIMBS + frac_limbs;
	const unsigned maxiter = param->maxiter;
	const unsigned zpower = param->zpower;
	mp_limb_t x0[total_limbs], y0[total_limbs], preal[total_limbs], pimag[total_limbs];
	bool x0_sign, y0_sign, preal_sign, pimag_sign;
	mp_limb_t x[total_limbs], y[total_limbs], xsqr[total_limbs], ysqr[total_limbs], sqrsum[total_limbs], four[INT_LIMBS];
	mp_limb_t cd_x[total_limbs], cd_y[total_limbs];
	mpf_t dx, dy, new_dx, new_dy, ftmpreal, ftmpimag, ftmp1;
	unsigned i;

	if (distance_est) {
		mpf_init2 (dx, total_limbs * GMP_NUMB_BITS);
		mpf_init2 (dy, total_limbs * GMP_NUMB_BITS);
		mpf_init2 (new_dx, total_limbs * GMP_NUMB_BITS);
		mpf_init2 (new_dy, total_limbs * GMP_NUMB_BITS);
		mpf_init2 (ftmpreal, total_limbs * GMP_NUMB_BITS);
		mpf_init2 (ftmpimag, total_limbs * GMP_NUMB_BITS);
		mpf_init2 (ftmp1, total_limbs * GMP_NUMB_BITS);
		mpf_set_ui (dx, 0);
		mpf_set_ui (dy, 0);
	}

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
		mp_limb_t tmpreal[total_limbs], tmpimag[total_limbs], tmpreal2[total_limbs], tmpimag2[total_limbs], rtmp1[total_limbs];
		bool tmpreal_sign, tmpimag_sign, tmpreal2_sign, tmpimag2_sign, rtmp1_sign;
		if (distance_est) {
			complex_pow (x, x_sign, y, y_sign, zpower - 1, tmpreal2, &tmpreal2_sign, tmpimag2, &tmpimag2_sign, frac_limbs);

			my_mpn_get_mpf (ftmpreal, tmpreal2, tmpreal2_sign, frac_limbs);
			my_mpn_get_mpf (ftmpimag, tmpimag2, tmpimag2_sign, frac_limbs);
			mpf_mul (new_dx, ftmpreal, dx);
			mpf_mul (ftmp1, ftmpimag, dy);
			mpf_sub (new_dx, new_dx, ftmp1);
			mpf_mul_ui (new_dx, new_dx, zpower);
			mpf_add_ui (new_dx, new_dx, 1);
			mpf_mul (new_dy, ftmpimag, dx);
			mpf_mul (ftmp1, ftmpreal, dy);
			mpf_add (new_dy, new_dy, ftmp1);
			mpf_mul_ui (dy, new_dy, zpower);
			mpf_set (dx, new_dx);

			my_mpn_mul_fast (tmpreal, tmpreal2, x, frac_limbs);
			tmpreal_sign = tmpreal2_sign != x_sign;
			my_mpn_mul_fast (rtmp1, tmpimag2, y, frac_limbs);
			rtmp1_sign = tmpimag2_sign != y_sign;
			tmpreal_sign = my_mpn_add_signed (tmpreal, tmpreal, tmpreal_sign, rtmp1, !rtmp1_sign, frac_limbs);
			my_mpn_mul_fast (tmpimag, tmpimag2, x, frac_limbs);
			tmpimag_sign = tmpimag2_sign != x_sign;
			my_mpn_mul_fast (rtmp1, tmpreal2, y, frac_limbs);
			rtmp1_sign = tmpreal2_sign != y_sign;
			tmpimag_sign = my_mpn_add_signed (tmpimag, tmpimag, tmpimag_sign, rtmp1, rtmp1_sign, frac_limbs);
		} else
			complex_pow (x, x_sign, y, y_sign, zpower, tmpreal, &tmpreal_sign, tmpimag, &tmpimag_sign, frac_limbs);

		x_sign = my_mpn_add_signed (x, tmpreal, tmpreal_sign, preal, preal_sign, frac_limbs);
		y_sign = my_mpn_add_signed (y, tmpimag, tmpimag_sign, pimag, pimag_sign, frac_limbs);

		k--;
		/* FIXME we must compare the signs here! */
		if (mpn_cmp (x, cd_x, total_limbs) == 0 && mpn_cmp (y, cd_y, total_limbs) == 0) {
			//printf ("* Cycle of length %d detected after %u iterations.\n", m - k + 1, i);
			// XXX iter_saved += maxiter - i;
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
	if (distance_est) {
		mpf_t xf, yf;
		mpf_init2 (xf, total_limbs * GMP_NUMB_BITS);
		mpf_init2 (yf, total_limbs * GMP_NUMB_BITS);
		my_mpn_get_mpf (xf, x, x_sign, frac_limbs);
		my_mpn_get_mpf (yf, y, y_sign, frac_limbs);
		mpf_mul (xf, xf, xf);
		mpf_mul (yf, yf, yf);
		mpf_add (xf, xf, yf);
		mpf_sqrt (xf, xf);
		//double zabs = mpf_get_d (xf);
		mpfr_t zabs;
		mpfr_init2 (zabs, 1024); /* XXX */
		mpfr_set_f (zabs, xf, GMP_RNDN);
		mpf_mul (dx, dx, dx);
		mpf_mul (dy, dy, dy);
		mpf_add (dx, dx, dy);
		mpf_sqrt (dx, dx);
		//double dzabs = mpf_get_d (dxf);
		mpfr_t dzabs;
		mpfr_init2 (dzabs, 1024); /* XXX */
		mpfr_set_f (dzabs, dx, GMP_RNDN);
		//double distance = log (zabs * zabs) * zabs / dzabs;
		mpfr_sqr (distance, zabs, GMP_RNDN);
		mpfr_log (distance, distance, GMP_RNDN);
		mpfr_mul (distance, distance, zabs, GMP_RNDN);
		mpfr_div (distance, distance, dzabs, GMP_RNDN);

		mpf_clear (xf);
		mpf_clear (yf);
		mpf_clear (dx);
		mpf_clear (dy);
		mpf_clear (new_dx);
		mpf_clear (new_dy);
		mpf_clear (ftmpreal);
		mpf_clear (ftmpimag);
		mpf_clear (ftmp1);
		mpfr_clear (zabs);
		mpfr_clear (dzabs);
	}
	return i;
}


static unsigned
mandel_julia_z2_fp (struct mandel_julia_state *state, const struct mandel_julia_param *param, mandel_fp_t x0, mandel_fp_t y0, mandel_fp_t preal, mandel_fp_t pimag, mandel_fp_t *distance)
{
	const bool distance_est = (state->flags & FRAC_TYPE_DISTANCE) != 0;
	const unsigned maxiter = param->maxiter;
	unsigned i = 0, k = 1, m = 1;
	mandel_fp_t x = x0, y = y0, cd_x = x, cd_y = y, dx = 0.0, dy = 0.0;
	while (i < maxiter && x * x + y * y < 4.0) {
		if (distance_est) {
			mandel_fp_t dxnew = 2.0 * (dx * x - dy * y) + 1.0;
			dy = 2.0 * (dx * y + dy * x);
			dx = dxnew;
		}
		mandel_fp_t xold = x, yold = y;
		x = x * x - y * y + preal;
		y = 2 * xold * yold + pimag;

		k--;
		if (x == cd_x && y == cd_y) {
			// XXX iter_saved += maxiter - i;
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
	if (distance_est) {
		mandel_fp_t zabs = sqrt (x * x + y * y);
		mandel_fp_t dzabs = sqrt (dx * dx + dy * dy);
		*distance = log (zabs * zabs) * zabs / dzabs;
#if 0
		const mandel_fp_t kk = 12.353265; /* 256.0 / log (1e9) */
		int idx = ((int) round (-kk * log (fabs (distance)))) % 256;
		if (idx < 0)
			return idx + 256;
		else
			return idx;
#endif
	}
	return i;
}


static unsigned
mandel_julia_zpower_fp (struct mandel_julia_state *state, const struct mandel_julia_param *param, mandel_fp_t x0, mandel_fp_t y0, mandel_fp_t preal, mandel_fp_t pimag, mandel_fp_t *distance)
{
	const bool distance_est = (state->flags & FRAC_TYPE_DISTANCE) != 0;
	const unsigned maxiter = param->maxiter;
	const unsigned zpower = param->zpower;
	unsigned i = 0, k = 1, m = 1;
	mandel_fp_t x = x0, y = y0, cd_x = x, cd_y = y, dx = 0.0, dy = 0.0;
	while (i < maxiter && x * x + y * y < 4.0) {
		if (distance_est) {
			mandel_fp_t treal, timag;
			complex_pow_fp (x, y, zpower - 1, &treal, &timag);
			mandel_fp_t new_dx = (mandel_fp_t) zpower * (treal * dx - timag * dy) + 1.0;
			dy = (mandel_fp_t) zpower * (treal * dy + timag * dx);
			dx = new_dx;
			mandel_fp_t new_x = treal * x - timag * y;
			y = treal * y + timag * x;
			x = new_x;
		} else
			complex_pow_fp (x, y, zpower, &x, &y);

		x += preal;
		y += pimag;

		k--;
		if (x == cd_x && y == cd_y) {
			// XXX iter_saved += maxiter - i;
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
	if (distance_est) {
		mandel_fp_t zabs = sqrt (x * x + y * y);
		mandel_fp_t dzabs = sqrt (dx * dx + dy * dy);
		*distance = log (zabs * zabs) * zabs / dzabs;
	}
	return i;
}


static bool
mandel_julia (struct mandel_julia_state *state, const struct mandel_julia_param *param, mpf_srcptr x0f, mpf_srcptr y0f, mpf_srcptr prealf, mpf_srcptr pimagf, unsigned *iter, mpfr_ptr distance)
{
	unsigned my_iter = 0;
	if (param->zpower == 2)
		my_iter = mandel_julia_z2 (state, param, x0f, y0f, prealf, pimagf, distance);
	else
		my_iter = mandel_julia_zpower (state, param, x0f, y0f, prealf, pimagf, distance);
	if (state->flags & FRAC_TYPE_ESCAPE_ITER)
		*iter = my_iter;
	return my_iter == param->maxiter;
}


static bool
mandel_julia_fp (struct mandel_julia_state *state, const struct mandel_julia_param *param, mandel_fp_t x0, mandel_fp_t y0, mandel_fp_t preal, mandel_fp_t pimag, unsigned *iter, mandel_fp_t *distance)
{
	unsigned my_iter = 0;
	if (param->zpower == 2)
		my_iter = mandel_julia_z2_fp (state, param, x0, y0, preal, pimag, distance);
	else
		my_iter = mandel_julia_zpower_fp (state, param, x0, y0, preal, pimag, distance);
	if (state->flags & FRAC_TYPE_ESCAPE_ITER)
		*iter = my_iter;
	return my_iter == param->maxiter;
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


static void *
mandelbrot_param_new (void)
{
	struct mandelbrot_param *param = malloc (sizeof (*param));
	memset (param, 0, sizeof (*param));
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
mandelbrot_state_new (const void *param_, fractal_type_flags_t flags, unsigned frac_limbs)
{
	const struct mandelbrot_param *param = (struct mandelbrot_param *) param_;
	struct mandelbrot_state *state = malloc (sizeof (*state));
	memset (state, 0, sizeof (*state));
	state->mjstate.flags = flags;
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
	free (state);
}


static bool
mandelbrot_compute (void *state_, mpf_srcptr real, mpf_srcptr imag, unsigned *iter, mpfr_ptr distance)
{
	struct mandelbrot_state *state = (struct mandelbrot_state *) state_;
	const struct mandelbrot_param *param = state->param;
	return mandel_julia (&state->mjstate, &param->mjparam, real, imag, real, imag, iter, distance);
}


static bool
mandelbrot_compute_fp (void *state_, mandel_fp_t real, mandel_fp_t imag, unsigned *iter, mandel_fp_t *distance)
{
	struct mandelbrot_state *state = (struct mandelbrot_state *) state_;
	const struct mandelbrot_param *param = state->param;
	return mandel_julia_fp (&state->mjstate, &param->mjparam, real, imag, real, imag, iter, distance);
}


static void *
julia_param_new (void)
{
	struct julia_param *param = malloc (sizeof (*param));
	memset (param, 0, sizeof (*param));
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
julia_state_new (const void *param_, fractal_type_flags_t flags, unsigned frac_limbs)
{
	const struct julia_param *param = (struct julia_param *) param_;
	struct julia_state *state = malloc (sizeof (*state));
	state->mjstate.flags = flags;
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
	free (state);
}


static bool
julia_compute (void *state_, mpf_srcptr real, mpf_srcptr imag, unsigned *iter, mpfr_ptr distance)
{
	struct julia_state *state = (struct julia_state *) state_;
	const struct julia_param *param = state->param;
	return mandel_julia (&state->mjstate, &param->mjparam, real, imag, param->param.real, param->param.imag, iter, distance);
}


static bool
julia_compute_fp (void *state_, mandel_fp_t real, mandel_fp_t imag, unsigned *iter, mandel_fp_t *distance)
{
	struct julia_state *state = (struct julia_state *) state_;
	const struct julia_param *param = state->param;
	return mandel_julia_fp (&state->mjstate, &param->mjparam, real, imag, state->mpvars.fp.preal_float, state->mpvars.fp.pimag_float, iter, distance);
}


static void
mandel_julia_state_init (struct mandel_julia_state *state, const struct mandel_julia_param *param)
{
	/* Nothing to do. */
}


static void
mandel_julia_state_clear (struct mandel_julia_state *state)
{
	/* Nothing to do. */
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


