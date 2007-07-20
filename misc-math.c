#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

#include <gmp.h>

#include "fpdefs.h"
#include "misc-math.h"


/* Currently unused. */
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
complex_pow_fp (mandel_fp_t xreal, mandel_fp_t ximag, unsigned n, mandel_fp_t *rreal, mandel_fp_t *rimag)
{
	if (n == 0) {
		*rreal = 1.0;
		*rimag = 0.0;
		return;
	}

	/*
	 * Use an integer of well-defined size, so we can safely check
	 * whether the highest bit is set.
	 */
	uint32_t m = n;
	unsigned bits = 32;
	while ((m & (1 << 31)) == 0) {
		m <<= 1;
		bits--;
	}
	/* Ignore the first 1-bit as we set c = x initially */
	m <<= 1;
	bits--;

	mandel_fp_t creal = xreal, cimag = ximag;
	mandel_fp_t tmp;
	int i;
	for (i = 0; i < bits; i++) {
		/* square */
		tmp = creal;
		creal = creal * creal - cimag * cimag;
		cimag = ldexp (tmp * cimag, 1);

		if ((m & (1 << 31)) != 0) {
			/* multiply by x */
			tmp = creal;
			creal = creal * xreal - cimag * ximag;
			cimag = tmp * ximag + cimag * xreal;
		}

		m <<= 1;
	}

	*rreal = creal;
	*rimag = cimag;
}


void
complex_pow (mp_srcptr xreal, bool xreal_sign, mp_srcptr ximag, bool ximag_sign, unsigned n, mp_ptr real, bool *rreal_sign, mp_ptr imag, bool *rimag_sign, unsigned frac_limbs)
{
	unsigned total_limbs = INT_LIMBS + frac_limbs;
	mp_limb_t real_buf[total_limbs], imag_buf[total_limbs], temp[total_limbs];
	bool src_real_sign = xreal_sign, src_imag_sign = ximag_sign, dst_real_sign, dst_imag_sign;
	mp_ptr src_real = real_buf, src_imag = imag_buf, dst_real = real, dst_imag = imag, temp_ptr;

	/* XXX This copy could be avoided. */
	memcpy (src_real, xreal, total_limbs * sizeof (*src_real));
	memcpy (src_imag, ximag, total_limbs * sizeof (*src_imag));

	/*
	 * Use an integer of well-defined size, so we can safely check
	 * whether the highest bit is set.
	 */
	uint32_t m = n;
	unsigned bits = 32;
	while ((m & (1 << 31)) == 0) {
		m <<= 1;
		bits--;
	}
	/* Ignore the first 1-bit as we set c = x initially */
	m <<= 1;
	bits--;

	int i;
	for (i = 0; i < bits; i++) {
		/* square */
		my_mpn_mul_fast (dst_real, src_real, src_real, frac_limbs);
		my_mpn_mul_fast (temp, src_imag, src_imag, frac_limbs);
		dst_real_sign = my_mpn_add_signed (dst_real, dst_real, false, temp, true, frac_limbs);

		my_mpn_mul_fast (dst_imag, src_real, src_imag, frac_limbs);
		mpn_lshift (dst_imag, dst_imag, total_limbs, 1);
		dst_imag_sign = src_real_sign != src_imag_sign;

		/* swap src <-> dst */
		temp_ptr = src_real;
		src_real = dst_real;
		dst_real = temp_ptr;

		temp_ptr = src_imag;
		src_imag = dst_imag;
		dst_imag = temp_ptr;

		src_real_sign = dst_real_sign;
		src_imag_sign = dst_imag_sign;

		if ((m & (1 << 31)) != 0) {
			/* multiply by x */
			my_mpn_mul_fast (dst_real, src_real, xreal, frac_limbs);
			my_mpn_mul_fast (temp, src_imag, ximag, frac_limbs);
			dst_real_sign = my_mpn_add_signed (dst_real, dst_real, src_real_sign != xreal_sign, temp, src_imag_sign == ximag_sign, frac_limbs);

			my_mpn_mul_fast (dst_imag, src_real, ximag, frac_limbs);
			my_mpn_mul_fast (temp, src_imag, xreal, frac_limbs);
			dst_imag_sign = my_mpn_add_signed (dst_imag, dst_imag, src_real_sign != ximag_sign, temp, src_imag_sign != xreal_sign, frac_limbs);

			/* swap src <-> dst */
			temp_ptr = src_real;
			src_real = dst_real;
			dst_real = temp_ptr;

			temp_ptr = src_imag;
			src_imag = dst_imag;
			dst_imag = temp_ptr;

			src_real_sign = dst_real_sign;
			src_imag_sign = dst_imag_sign;
		}

		m <<= 1;
	}

	*rreal_sign = src_real_sign;
	if (src_real != real)
		memcpy (real, src_real, total_limbs * sizeof (*real));

	*rimag_sign = src_imag_sign;
	if (src_imag != imag)
		memcpy (imag, src_imag, total_limbs * sizeof (*imag));
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


