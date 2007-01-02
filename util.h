#ifndef _GTKMANDEL_UTIL_H
#define _GTKMANDEL_UTIL_H

#define MY_MIN(a, b) ((a)<(b)?(a):(b))
#define MY_MAX(a, b) ((a)>(b)?(a):(b))

void corners_to_center (mpf_t cx, mpf_t cy, mpf_t magf, mpf_t xmin, mpf_t xmax, mpf_t ymin, mpf_t ymax);
void center_to_corners (mpf_t xmin, mpf_t xmax, mpf_t ymin, mpf_t ymax, const mpf_t cx, const mpf_t cy, const mpf_t magf, double aspect);

int coord_pair_to_string (mpf_t a, mpf_t b, char *abuf, char *bbuf, int buf_size);
int corner_coords_to_string (mpf_t xmin, mpf_t xmax, mpf_t ymin, mpf_t ymax, char *xmin_buf, char *xmax_buf, char *ymin_buf, char *ymax_buf, int buf_size);
int center_coords_to_string (mpf_t cx, mpf_t cy, mpf_t magf, char *cx_buf, char *cy_buf, char *magf_buf, int buf_size);

void free_not_null (void *ptr);

#endif /* _GTKMANDEL_UTIL_H */
