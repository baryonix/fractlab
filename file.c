#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <gmp.h>

#include "file.h"
#include "util.h"

typedef void *yyscan_t;
int coord_lex_init (yyscan_t *scanner);
void coord_restart (FILE *input_file, yyscan_t yyscanner);
void coord_lex_destroy (yyscan_t yyscanner);
int coord_parse (yyscan_t scanner, struct mandeldata *md, char *errbuf, size_t errbsize);

static bool generic_write_mandel_julia (struct io_stream *f, const struct mandel_julia_param *param, bool crlf, char *errbuf, size_t errbsize);


bool
fread_mandeldata (FILE *f, struct mandeldata *md, char *errbuf, size_t errbsize)
{
	bool res;
	yyscan_t scanner;
	if (coord_lex_init (&scanner) != 0) {
		my_safe_strcpy (errbuf, strerror (errno), errbsize);
		return false;
	}
	coord_restart (f, scanner);
	res = coord_parse (scanner, md, errbuf, errbsize) == 0;
	coord_lex_destroy (scanner);
	return res;
}


bool
read_mandeldata (const char *filename, struct mandeldata *md, char *errbuf, size_t errbsize)
{
	bool res;
	FILE *f = my_fopen (filename, "r", errbuf, errbsize);
	if (f == NULL)
		return false;
	res = fread_mandeldata (f, md, errbuf, errbsize);
	fclose (f);
	return res;
}


bool
generic_write_mandeldata (struct io_stream *f, const struct mandeldata *md, bool crlf, char *errbuf, size_t errbsize)
{
	const char *nl = crlf ? "\r\n" : "\n";
	char creal[1024], cimag[1024], magf[1024];
	if (center_coords_to_string (md->area.center.real, md->area.center.imag, md->area.magf, creal, cimag, magf, 1024) < 0) {
		my_safe_strcpy (errbuf, "Error converting coordinates", errbsize);
		return false;
	}
	if (my_printf (f, errbuf, errbsize, "coord-v1 {%s\tarea %s/%s/%s;%s\trepresentation ", nl, creal, cimag, magf, nl) < 0)
		return false;
	switch (md->repres.repres) {
		case REPRES_ESCAPE:
			if (my_printf (f, errbuf, errbsize, "escape") < 0)
				return false;
			break;
		case REPRES_ESCAPE_LOG:
			if (my_printf (f, errbuf, errbsize, "escape-log {%s\t\tbase %f;%s\t}", nl, md->repres.params.log_base, nl) < 0)
				return false;
			break;
		case REPRES_DISTANCE:
			if (my_printf (f, errbuf, errbsize, "distance") < 0)
				return false;
			break;
		default:
			snprintf (errbuf, errbsize, "Unknown representation type %d", (int) md->repres.repres);
			return false;
	}
	if (my_printf (f, errbuf, errbsize, ";%s\ttype %s {%s", nl, md->type->name, nl) < 0)
		return false;
	switch (md->type->type) {
		case FRACTAL_MANDELBROT: {
			if (!generic_write_mandel_julia (f, md->type_param, crlf, errbuf, errbsize))
				return false;
			break;
		}
		case FRACTAL_JULIA: {
			if (!generic_write_mandel_julia (f, md->type_param, crlf, errbuf, errbsize))
				return false;
			const struct julia_param *jparam = (const struct julia_param *) md->type_param;
			if (my_gmp_printf (f, errbuf, errbsize, "\t\tparameter %.20Ff/%.20Ff;%s", jparam->param.real, jparam->param.imag, nl) < 0)
				return false;
			break;
		}
		default: {
			snprintf (errbuf, errbsize, "Unknown fractal type %d", (int) md->type->type);
			return false;
		}
	}
	if (my_printf (f, errbuf, errbsize, "\t};%s};%s", nl, nl) < 0)
		return false;
	return true;
}


static bool
generic_write_mandel_julia (struct io_stream *f, const struct mandel_julia_param *param, bool crlf, char *errbuf, size_t errbsize)
{
	const char *nl = crlf ? "\r\n" : "\n";
	if (my_printf (f, errbuf, errbsize, "\t\tzpower %u;%s\t\tmaxiter %u;%s", param->zpower, nl, param->maxiter, nl) < 0)
		return false;
	return true;
}


bool
write_mandeldata (const char *filename, const struct mandeldata *md, bool crlf, char *errbuf, size_t errbsize)
{
	bool res;
	FILE *f = my_fopen (filename, "w", errbuf, errbsize);
	if (f == NULL)
		return false;
	res = fwrite_mandeldata (f, md, crlf, errbuf, errbsize);
	fclose (f);
	return res;
}


bool
fwrite_mandeldata (FILE *f, const struct mandeldata *md, bool crlf, char *errbuf, size_t errbsize)
{
	struct io_stream stream[1];
	io_stream_init_file (stream, f);
	return generic_write_mandeldata (stream, md, crlf, errbuf, errbsize);
}
