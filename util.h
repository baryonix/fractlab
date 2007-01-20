#ifndef _GTKMANDEL_UTIL_H
#define _GTKMANDEL_UTIL_H

#include "fpdefs.h"

static inline double
my_fmax (double a, double b)
{
	if (a > b)
		return a;
	else
		return b;
}

#define MY_MIN(a, b) ((a)<(b)?(a):(b))
#define MY_MAX(a, b) ((a)>(b)?(a):(b))

void corners_to_center (mpf_ptr cx, mpf_ptr cy, mpf_ptr magf, mpf_srcptr xmin, mpf_srcptr xmax, mpf_srcptr ymin, mpf_srcptr ymax);
void center_to_corners (mpf_ptr xmin, mpf_ptr xmax, mpf_ptr ymin, mpf_ptr ymax, mpf_srcptr cx, mpf_srcptr cy, mpf_srcptr magf, double aspect);

int coord_pair_to_string (mpf_srcptr a, mpf_srcptr b, char *abuf, char *bbuf, int buf_size);
int corner_coords_to_string (mpf_srcptr xmin, mpf_srcptr xmax, mpf_srcptr ymin, mpf_srcptr ymax, char *xmin_buf, char *xmax_buf, char *ymin_buf, char *ymax_buf, int buf_size);
int center_coords_to_string (mpf_srcptr cx, mpf_srcptr cy, mpf_srcptr magf, char *cx_buf, char *cy_buf, char *magf_buf, int buf_size);

void free_not_null (void *ptr);

#endif /* _GTKMANDEL_UTIL_H */
