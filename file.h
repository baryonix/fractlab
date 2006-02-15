#ifndef _MANDEL_FILE_H
#define _MANDEL_FILE_H

#include <stdbool.h>

#include <gmp.h>

bool read_coords_from_file (const char *filename, mpf_t xc, mpf_t yc, mpf_t magf);

#endif /* _MANDEL_FILE_H */
