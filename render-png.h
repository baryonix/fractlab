#ifndef _GTKMANDEL_RENDER_PNG_H
#define _GTKMANDEL_RENDER_PNG_H

#include "fractal-render.h"


void write_png (const struct mandel_renderer *md, const char *filename, int compression);
void render_to_png (struct mandeldata *md, const char *filename, int compression, unsigned *bits, unsigned w, unsigned h, unsigned threads, unsigned aa_level);

#endif /* _GTKMANDEL_RENDER_PNG_H */
