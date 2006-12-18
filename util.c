#include <gmp.h>

#include "util.h"

int
coord_pair_to_string (mpf_t a, mpf_t b, char *abuf, char *bbuf, int buf_size)
{
	int r, digits;
	long exponent;
	mpf_t d;
	mpf_init (d);
	mpf_sub (d, a, b);
	mpf_get_d_2exp (&exponent, d);
	/* We are using %f format, so the absolute difference between
	 * the min and max values dictates the required precision. */
	digits = -exponent / 3.3219 + 5;
	r = gmp_snprintf (abuf, buf_size, "%.*Ff", digits, a);
	if (r < 0 || r >= buf_size)
		return -1;
	r = gmp_snprintf (bbuf, buf_size, "%.*Ff", digits, b);
	if (r < 0 || r >= buf_size)
		return -1;
	return 0;
}

int
coords_to_string (mpf_t xmin, mpf_t xmax, mpf_t ymin, mpf_t ymax, char *xmin_buf, char *xmax_buf, char *ymin_buf, char *ymax_buf, int buf_size)
{
	if (coord_pair_to_string (xmin, xmax, xmin_buf, xmax_buf, buf_size) < 0)
		return -1;
	if (coord_pair_to_string (ymin, ymax, ymin_buf, ymax_buf, buf_size) < 0)
		return -1;
	return 0;
}
