#ifndef _MANDEL_FILE_H
#define _MANDEL_FILE_H

#include <stdbool.h>
#include "fractal-render.h"

bool read_mandeldata (const char *filename, struct mandeldata *md, char *errbuf, size_t errbsize);
bool fread_mandeldata (FILE *f, struct mandeldata *md, char *errbuf, size_t errbsize);
bool write_mandeldata (const char *filename, const struct mandeldata *md, char *errbuf, size_t errbsize);
bool fwrite_mandeldata (FILE *f, const struct mandeldata *md, char *errbuf, size_t errbsize);

#endif /* _MANDEL_FILE_H */
