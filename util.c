#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>

#include <gmp.h>

#include "util.h"


static void absdiff (mpf_ptr d, mpf_srcptr a, mpf_srcptr b);
static void center (mpf_ptr c, mpf_srcptr a, mpf_srcptr b);
static int file_vprintf (void *data, char *errbuf, size_t errbsize, const char *format, va_list ap);
static int io_buffer_vprintf (void *data, char *errbuf, size_t errbsize, const char *format, va_list ap);


static void
absdiff (mpf_ptr d, mpf_srcptr a, mpf_srcptr b)
{
	mpf_sub (d, a, b);
	mpf_abs (d, d);
}


static void
center (mpf_ptr c, mpf_srcptr a, mpf_srcptr b)
{
	mpf_add (c, a, b);
	mpf_div_ui (c, c, 2);
}


void
corners_to_center (mpf_ptr cx, mpf_ptr cy, mpf_ptr magf, mpf_srcptr xmin, mpf_srcptr xmax, mpf_srcptr ymin, mpf_srcptr ymax)
{
	mpf_t dx, dy;
	mpf_init (dx);
	mpf_init (dy);
	absdiff (dx, xmin, xmax);
	absdiff (dy, ymin, ymax);
	if (mpf_cmp (dx, dy) < 0)
		mpf_ui_div (magf, 2, dx);
	else
		mpf_ui_div (magf, 2, dy);
	mpf_clear (dx);
	mpf_clear (dy);
	center (cx, xmin, xmax);
	center (cy, ymin, ymax);
}


void
center_to_corners (mpf_ptr xmin, mpf_ptr xmax, mpf_ptr ymin, mpf_ptr ymax, mpf_srcptr cx, mpf_srcptr cy, mpf_srcptr magf, double aspect)
{
	mpf_t dx, dy;
	mpf_init (dx);
	mpf_init (dy);
	if (aspect > 1.0) {
		mpf_ui_div (dy, 1, magf);
		mpf_set_d (dx, aspect);
		mpf_mul (dx, dx, dy);
	} else {
		mpf_ui_div (dx, 1, magf);
		mpf_set_d (dy, aspect);
		mpf_div (dy, dx, dy);
	}
	mpf_sub (xmin, cx, dx);
	mpf_add (xmax, cx, dx);
	mpf_sub (ymin, cy, dy);
	mpf_add (ymax, cy, dy);
	mpf_clear (dx);
	mpf_clear (dy);
}


int
coord_pair_to_string (mpf_srcptr a, mpf_srcptr b, char *abuf, char *bbuf, int buf_size)
{
	int r, digits;
	long exponent;
	mpf_t d;
	mpf_init (d);
	mpf_sub (d, a, b);
	mpf_get_d_2exp (&exponent, d);
	mpf_clear (d);
	/* We are using %f format, so the absolute difference between
	 * the min and max values dictates the required precision. */
	digits = -exponent / 3.3219 + 5;
	r = gmp_snprintf (abuf, buf_size, "%.*Ff", digits, a);
	if (r < 0 || r >= buf_size)
		return -1;
	r = gmp_snprintf (bbuf, buf_size, "%.*Ff", digits, b);
	if (r < 0 || r >= buf_size)
		return -1;
	return 0;
}


int
corner_coords_to_string (mpf_srcptr xmin, mpf_srcptr xmax, mpf_srcptr ymin, mpf_srcptr ymax, char *xmin_buf, char *xmax_buf, char *ymin_buf, char *ymax_buf, int buf_size)
{
	if (coord_pair_to_string (xmin, xmax, xmin_buf, xmax_buf, buf_size) < 0)
		return -1;
	if (coord_pair_to_string (ymin, ymax, ymin_buf, ymax_buf, buf_size) < 0)
		return -1;
	return 0;
}


int
center_coords_to_string (mpf_srcptr cx, mpf_srcptr cy, mpf_srcptr magf, char *cx_buf, char *cy_buf, char *magf_buf, int buf_size)
{
	long exponent;
	int digits, r;

	mpf_get_d_2exp (&exponent, magf);
	digits = exponent / 3.3219 + 5;

	r = gmp_snprintf (cx_buf, buf_size, "%.*Ff", digits, cx);
	if (r < 0 || r >= buf_size)
		return -1;
	r = gmp_snprintf (cy_buf, buf_size, "%.*Ff", digits, cy);
	if (r < 0 || r >= buf_size)
		return -1;
	r = gmp_snprintf (magf_buf, buf_size, "%.10Fg", magf);
	if (r < 0 || r >= buf_size)
		return -1;
	return 0;
}


void
free_not_null (void *ptr)
{
	if (ptr != NULL)
		free (ptr);
}


static int
file_vprintf (void *data, char *errbuf, size_t errbsize, const char *format, va_list ap)
{
	FILE *stream = (FILE *) data;
	int res = vfprintf (stream, format, ap);
	if (res < 0 && errbuf != NULL && errbsize > 0)
		my_safe_strcpy (errbuf, strerror (errno), errbsize);
	return res;
}


static int
file_gmp_vprintf (void *data, char *errbuf, size_t errbsize, const char *format, va_list ap)
{
	FILE *stream = (FILE *) data;
	int res = gmp_vfprintf (stream, format, ap);
	if (res < 0 && errbuf != NULL && errbsize > 0)
		my_safe_strcpy (errbuf, strerror (errno), errbsize);
	return res;
}


FILE *
my_fopen (const char *path, const char *mode, char *errbuf, size_t errbsize)
{
	FILE *f = fopen (path, mode);
	if (f == NULL)
		my_safe_strcpy (errbuf, strerror (errno), errbsize);
	return f;
}


bool
io_buffer_init (struct io_buffer *buf, char *store, size_t len)
{
	if (len <= 0)
		return false;
	buf->len = len;
	buf->pos = 0;
	buf->must_free = store == NULL;
	if (store != NULL)
		buf->buf = store;
	else
		buf->buf = malloc (buf->len);
	if (buf->buf == NULL)
		return false;
	buf->buf[0] = 0;
	return true;
}


void
io_buffer_clear (struct io_buffer *buf)
{
	if (buf->must_free)
		free (buf->buf);
	buf->buf = NULL;
}


static int
io_buffer_generic_vprintf (void *data, char *errbuf, size_t errbsize, const char *format, va_list ap, int (*vsnprintf_func) (char *, size_t, const char *, va_list))
{
	struct io_buffer *buf = (struct io_buffer *) data;
	size_t rem = buf->len - buf->pos;
	int res = vsnprintf_func (buf->buf + buf->pos, rem, format, ap);
	if (res < 0 && errbuf != NULL && errbsize > 0)
		my_safe_strcpy (errbuf, strerror (errno), errbsize);
	if (res >= rem && errbuf != NULL && errbsize > 0)
		my_safe_strcpy (errbuf, "Buffer too small", errbsize);
	if (res < 0 || res >= rem) {
		if (buf->pos < buf->len)
			buf->buf[buf->pos] = 0;
		return -1;
	}
	buf->pos += res;
	return res;
}


static int
io_buffer_vprintf (void *data, char *errbuf, size_t errbsize, const char *format, va_list ap)
{
	return io_buffer_generic_vprintf (data, errbuf, errbsize, format, ap, vsnprintf);
}


static int
io_buffer_gmp_vprintf (void *data, char *errbuf, size_t errbsize, const char *format, va_list ap)
{
	return io_buffer_generic_vprintf (data, errbuf, errbsize, format, ap, gmp_vsnprintf);
}


void
io_stream_init_file (struct io_stream *stream, FILE *file)
{
	stream->data = file;
	stream->vprintf = file_vprintf;
	stream->gmp_vprintf = file_gmp_vprintf;
}


void
io_stream_init_buffer (struct io_stream *stream, struct io_buffer *buf)
{
	stream->data = buf;
	stream->vprintf = io_buffer_vprintf;
	stream->gmp_vprintf = io_buffer_gmp_vprintf;
}


int
my_printf (struct io_stream *stream, char *errbuf, size_t errbsize, const char *format, ...)
{
	va_list ap;
	va_start (ap, format);
	int r = my_vprintf (stream, errbuf, errbsize, format, ap);
	va_end (ap);
	return r;
}


int
my_vprintf (struct io_stream *stream, char *errbuf, size_t errbsize, const char *format, va_list ap)
{
	return stream->vprintf (stream->data, errbuf, errbsize, format, ap);
}


int
my_gmp_printf (struct io_stream *stream, char *errbuf, size_t errbsize, const char *format, ...)
{
	va_list ap;
	va_start (ap, format);
	int r = my_gmp_vprintf (stream, errbuf, errbsize, format, ap);
	va_end (ap);
	return r;
}


int
my_gmp_vprintf (struct io_stream *stream, char *errbuf, size_t errbsize, const char *format, va_list ap)
{
	return stream->gmp_vprintf (stream->data, errbuf, errbsize, format, ap);
}
