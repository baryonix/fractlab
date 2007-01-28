#ifndef _MANDEL_FILE_H
#define _MANDEL_FILE_H

#include <stdbool.h>
#include "mandelbrot.h"

bool fread_mandeldata (FILE *f, struct mandeldata *md);
bool fread_mandeldata_legacy (FILE *f, struct mandeldata *md);
bool fwrite_mandeldata (FILE *f, const struct mandeldata *md);

#endif /* _MANDEL_FILE_H */
