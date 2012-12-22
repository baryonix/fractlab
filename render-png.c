#include <png.h>

#include "render-png.h"


void
write_png (const struct mandel_renderer *renderer, const char *filename, int compression)
{
	const unsigned width = mandel_renderer_width (renderer);
	const unsigned height = mandel_renderer_height (renderer);

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
	png_set_IHDR (png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_write_info (png_ptr, info_ptr);

	unsigned char row[width * 3];
	png_bytep row_ptr[] = {(png_bytep) row};

	unsigned int x, y;
	//int opct = -1;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			struct color px;
			mandel_get_pixel (renderer, x, y, &px);

			row[3 * x + 0] = px.r >> 8;
			row[3 * x + 1] = px.g >> 8;
			row[3 * x + 2] = px.b >> 8;
		}
		png_write_rows (png_ptr, row_ptr, 1);
	}

	png_write_end (png_ptr, info_ptr);
	png_destroy_write_struct (&png_ptr, &info_ptr);

	fclose (f);
}


void
render_to_png (struct mandeldata *md, const char *filename, int compression, unsigned *bits, unsigned w, unsigned h, unsigned threads, unsigned aa_level)
{
	struct mandel_renderer renderer[1];

	mandel_renderer_init (renderer, md, w, h, aa_level);
	if (threads > 1)
		renderer->render_method = RM_MARIANI_SILVER;
	else
		renderer->render_method = RM_BOUNDARY_TRACE;
	renderer->thread_count = threads;
	mandel_render (renderer);
	write_png (renderer, filename, compression);
	if (bits != NULL)
		*bits = mandel_get_precision (renderer);
	mandel_renderer_clear (renderer);
}
