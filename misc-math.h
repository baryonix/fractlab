#ifndef _GTKMANDEL_MISC_MATH_H
#define _GTKMANDEL_MISC_MATH_H

/*
 * INT_LIMBS is the number of mp_limb_t's to use for the integer part of
 * the number in fixed-point MP math. It probably shouldn't be defined
 * statically.
 */
#define INT_LIMBS 1

unsigned *pascal_triangle (unsigned n);
void store_powers_fp (mandel_fp_t *powers, mandel_fp_t x, unsigned n);
void complex_pow_fp (mandel_fp_t xreal, mandel_fp_t ximag, unsigned n, mandel_fp_t *rreal, mandel_fp_t *rimag, const unsigned *pascal);
void store_powers (mp_ptr powers, bool *signs, mp_srcptr x, bool xsign, unsigned n, unsigned frac_limbs);
void complex_pow (mp_srcptr xreal, bool xreal_sign, mp_srcptr ximag, bool ximag_sign, unsigned n, mp_ptr real, bool *rreal_sign, mp_ptr imag, bool *rimag_sign, unsigned frac_limbs, const unsigned *pascal);

void my_mpn_get_mpf (mpf_ptr rop, mp_srcptr op, bool sign, unsigned frac_limbs);
bool my_mpf_get_mpn (mp_ptr rop, mpf_srcptr op, unsigned frac_limbs);

static inline mandel_fp_t stored_power_fp (mandel_fp_t x, unsigned n, mandel_fp_t *powers);
static inline void my_mpn_mul_fast (mp_ptr p, mp_srcptr f0, mp_srcptr f1, unsigned frac_limbs);
static inline void my_mpn_invert (mp_ptr op, unsigned total_limbs);

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

/*
 * FIXME: Despite the name, this routine isn't especially fast.
 * It should probably suffice to multiply only part of the operands,
 * ignoring a few of the least significant limbs, but when I tried this
 * resulted in a significant loss of precision.
 */
static inline void
my_mpn_mul_fast (mp_ptr p, mp_srcptr f0, mp_srcptr f1, unsigned frac_limbs)
{
	unsigned total_limbs = INT_LIMBS + frac_limbs;
	mp_limb_t tmp[total_limbs * 2];
	int i;
	mpn_mul (tmp, f0, total_limbs, f1, total_limbs);
	for (i = 0; i < total_limbs; i++)
		p[i] = tmp[frac_limbs + i];
}


static inline bool
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
static inline void
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


#endif /* _GTKMANDEL_MISC_MATH_H */
