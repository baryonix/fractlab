#ifndef _GTKMANDEL_UTIL_H
#define _GTKMANDEL_UTIL_H

void corners_to_center (mpf_t cx, mpf_t cy, mpf_t magf, mpf_t xmin, mpf_t xmax, mpf_t ymin, mpf_t ymax);
void center_to_corners (mpf_t xmin, mpf_t xmax, mpf_t ymin, mpf_t ymax, mpf_t cx, mpf_t cy, mpf_t magf, double aspect);

int coord_pair_to_string (mpf_t a, mpf_t b, char *abuf, char *bbuf, int buf_size);
int coords_to_string (mpf_t xmin, mpf_t xmax, mpf_t ymin, mpf_t ymax, char *xmin_buf, char *xmax_buf, char *ymin_buf, char *ymax_buf, int buf_size);

#endif /* _GTKMANDEL_UTIL_H */
