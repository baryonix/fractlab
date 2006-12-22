#ifndef _MANDEL_FILE_H
#define _MANDEL_FILE_H

#include <stdbool.h>

#include <gmp.h>

bool fread_coords_as_center (FILE *f, mpf_t xc, mpf_t yc, mpf_t magf);
bool fread_coords_as_corners (FILE *f, mpf_t xmin, mpf_t xmax, mpf_t ymin, mpf_t ymax, double aspect);
bool fread_center_coords (FILE *f, mpf_t xc, mpf_t yc, mpf_t magf);
bool fread_corner_coords (FILE *f, mpf_t xmin, mpf_t xmax, mpf_t ymin, mpf_t ymax);

#endif /* _MANDEL_FILE_H */
