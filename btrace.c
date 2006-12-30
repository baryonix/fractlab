/*
 * Boundary trace algorithm.
 * Currently used to generate a nice Julia set animation by tracing the
 * boundary of the main area of the Mandelbrot set.
 * This is only a very quick shot. Some TODOs remain before this can be
 * incorporated into mandel_render():
 * - Handle hitting the edge of the area. It currently assumes it doesn't.
 * - Maybe we need to do something to prevent running around in circles (or,
 *   more precisely, in squares:-) -- unsure whether it's really a problem.
 * - Find a way to efficiently parallelize this algorithm.
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <gmp.h>

#include <png.h>


#include "mandelbrot.h"

#define COLORS 256

struct color {
	unsigned char r, g, b;
};

struct color colors[COLORS];

int compression = 9;
int thread_count = 1;

static void
write_png (const struct mandeldata *md, const char *filename)
{
	FILE *f = fopen (filename, "wb");

	png_structp png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	png_infop info_ptr = png_create_info_struct (png_ptr);

	if (setjmp (png_jmpbuf (png_ptr)))
		fprintf (stderr, "PNG longjmp!\n");

	//png_set_swap (png_ptr); /* FIXME this should only be done on little-endian systems */
	png_init_io (png_ptr, f);
	if (compression == 0)
		png_set_filter (png_ptr, 0, PNG_FILTER_NONE);
	if (compression >= 0)
		png_set_compression_level (png_ptr, compression);
	png_set_IHDR (png_ptr, info_ptr, md->w, md->h, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_write_info (png_ptr, info_ptr);

	unsigned char row[md->w * 3];
	png_bytep row_ptr[] = {(png_bytep) row};

	unsigned int x, y;
	//int opct = -1;

	for (y = 0; y < md->h; y++) {
		for (x = 0; x < md->w; x++) {
			unsigned int i = mandel_get_pixel (md, x, y);

			i %= 256;

			row[3 * x + 0] = colors[i].r;
			row[3 * x + 1] = colors[i].g;
			row[3 * x + 2] = colors[i].b;
		}
		png_write_rows (png_ptr, row_ptr, 1);
	}

	png_write_end (png_ptr, info_ptr);
	png_destroy_write_struct (&png_ptr, &info_ptr);

	fclose (f);
}

unsigned
maybe_render (struct mandeldata *md, int x, int y)
{
	if (mandel_get_pixel (md, x, y) == 0) /* FIXME */
		mandel_render_pixel (md, x, y);
	return mandel_get_pixel (md, x, y);
}

void
turn_right (int xs, int ys, int *xsn, int *ysn)
{
	*xsn = -ys;
	*ysn = xs;
}

void
turn_left (int xs, int ys, int *xsn, int *ysn)
{
	*xsn = ys;
	*ysn = -xs;
}

int
main (int argc, char *argv[])
{
	mpf_set_default_prec (256); /* ! */
	struct mandeldata md[1];
	memset (md, 0, sizeof (*md));

	int i;
	for (i = 0; i < COLORS; i++) {
		colors[i].r = (unsigned short) (sin (2 * M_PI * i / COLORS) * 127) + 128;
		colors[i].g = (unsigned short) (sin (4 * M_PI * i / COLORS) * 127) + 128;
		colors[i].b = (unsigned short) (sin (6 * M_PI * i / COLORS) * 127) + 128;
	}

	mpf_init (md->cx);
	mpf_init (md->cy);
	mpf_init (md->magf);

	md->type = FRACTAL_MANDELBROT;
	mpf_set_str (md->cx, "-.5", 10);
	mpf_set_str (md->cy, "0", 10);
	mpf_set_str (md->magf, ".5", 10);
	md->maxiter = 100;
	md->w = 400;
	md->h = 400;

	md->data = malloc (md->w * md->h * sizeof (*md->data));
	memset (md->data, 0, md->w * md->h * sizeof (*md->data));

	mandel_init_coords (md);
	int x = md->w - 1, y = md->h / 2;
	while (1) {
		mandel_render_pixel (md, x, y);
		if (mandel_get_pixel (md, x, y) == md->maxiter)
			break;
		x--;
	}

	int x0 = x, y0 = y;
	/* Correct choice of initial direction is important */
	int xstep = 0, ystep = -1;

	mpf_t preal, pimag;
	mpf_init (preal);
	mpf_init (pimag);

	while (1) {
		if (maybe_render (md, x + xstep, y + ystep) < md->maxiter) {
			/* can't move forward, turn left */
			turn_left (xstep, ystep, &xstep, &ystep);
			continue;
		}
		/* move forward */
		x += xstep;
		y += ystep;
		if (x == x0 && y == y0)
			break;
		mandel_convert_x_f (md, preal, x);
		mandel_convert_x_f (md, pimag, y);
		gmp_printf ("%.5Ff\n%.5Ff\n", preal, pimag);
		int xsn, ysn;
		turn_right (xstep, ystep, &xsn, &ysn);
		/* If we don't have a wall at the right, turn right. */
		if (maybe_render (md, x + xsn, y + ysn) == md->maxiter) {
			xstep = xsn;
			ystep = ysn;
		}
	}

	write_png (md, "foo.png");

	return 0;
}
