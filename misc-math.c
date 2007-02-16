#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <gmp.h>

#include "fpdefs.h"
#include "misc-math.h"


unsigned *
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


void
store_powers_fp (mandel_fp_t *powers, mandel_fp_t x, unsigned n)
{
	int i;
	powers[0] = x * x;
	for (i = 1; i < n - 1; i++) {
		powers[i] = powers[i - 1] * x;
	}
}


void
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


void
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


void
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


bool
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


void
my_mpn_get_mpf (mpf_ptr rop, mp_srcptr op, bool sign, unsigned frac_limbs)
{
	const unsigned total_limbs = frac_limbs + INT_LIMBS;
	int i;
	/* DIRTY! */
	mpf_set_prec (rop, total_limbs * mp_bits_per_limb);
	rop->_mp_size = total_limbs;
	rop->_mp_exp = INT_LIMBS;
	bool zero = true;
	for (i = total_limbs - 1; i >= 0; i--)
		if (zero && op[i] == 0) {
			rop->_mp_size--;
			rop->_mp_exp--;
		} else {
			zero = false;
			rop->_mp_d[i] = op[i];
		}
	if (sign)
		mpf_neg (rop, rop);
}


