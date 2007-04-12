#ifndef _GTKMANDEL_RENDER_PNG_H
#define _GTKMANDEL_RENDER_PNG_H

#include "fractal-render.h"

struct color {
	unsigned char r, g, b;
};


void write_png (const struct mandel_renderer *md, const char *filename, int compression, struct color *colors);
void render_to_png (struct mandeldata *md, const char *filename, int compression, unsigned *bits, struct color *colors, unsigned w, unsigned h);

#endif /* _GTKMANDEL_RENDER_PNG_H */
