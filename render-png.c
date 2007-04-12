#include <png.h>

#include "render-png.h"


void
write_png (const struct mandel_renderer *renderer, const char *filename, int compression, struct color *colors)
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
	png_set_IHDR (png_ptr, info_ptr, renderer->w, renderer->h, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_write_info (png_ptr, info_ptr);

	unsigned char row[renderer->w * 3];
	png_bytep row_ptr[] = {(png_bytep) row};

	unsigned int x, y;
	//int opct = -1;

	for (y = 0; y < renderer->h; y++) {
		for (x = 0; x < renderer->w; x++) {
			unsigned int i = mandel_get_pixel (renderer, x, y);

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


void
render_to_png (struct mandeldata *md, const char *filename, int compression, unsigned *bits, struct color *colors, unsigned w, unsigned h)
{
	struct mandel_renderer renderer[1];

	mandel_renderer_init (renderer, md, w, h);
	renderer->render_method = RM_BOUNDARY_TRACE;
	renderer->thread_count = 1;
	mandel_render (renderer);
	write_png (renderer, filename, compression, colors);
	if (bits != NULL)
		*bits = mandel_get_precision (renderer);
	mandel_renderer_clear (renderer);
}
