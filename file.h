#ifndef _MANDEL_FILE_H
#define _MANDEL_FILE_H

#include <stdbool.h>

#include <gmp.h>

bool read_center_coords_from_file (const char *filename, mpf_t xc, mpf_t yc, mpf_t magf);
bool read_corner_coords_from_file (const char *filename, mpf_t xmin, mpf_t xmax, mpf_t ymin, mpf_t ymax);

#endif /* _MANDEL_FILE_H */
