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

static bool fwrite_mandel_julia (FILE *f, const struct mandel_julia_param *param, char *errbuf, size_t errbsize);


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
	FILE *f = fopen (filename, "r");
	if (f == NULL) {
		my_safe_strcpy (errbuf, strerror (errno), errbsize);
		return false;
	}
	res = fread_mandeldata (f, md, errbuf, errbsize);
	fclose (f);
	return res;
}


bool
fwrite_mandeldata (FILE *f, const struct mandeldata *md, char *errbuf, size_t errbsize)
{
	char creal[1024], cimag[1024], magf[1024];
	if (center_coords_to_string (md->area.center.real, md->area.center.imag, md->area.magf, creal, cimag, magf, 1024) < 0) {
		my_safe_strcpy (errbuf, "Error converting coordinates", errbsize);
		return false;
	}
	if (my_fprintf (f, errbuf, errbsize, "coord-v1 {\n\tarea %s/%s/%s;\n\trepresentation ", creal, cimag, magf) < 0)
		return false;
	switch (md->repres.repres) {
		case REPRES_ESCAPE:
			if (my_fprintf (f, errbuf, errbsize, "escape") < 0)
				return false;
			break;
		case REPRES_ESCAPE_LOG:
			if (my_fprintf (f, errbuf, errbsize, "escape-log {\n\t\tbase %f;\n\t}", md->repres.params.log_base) < 0)
				return false;
			break;
		case REPRES_DISTANCE:
			if (my_fprintf (f, errbuf, errbsize, "distance") < 0)
				return false;
			break;
		default:
			snprintf (errbuf, errbsize, "Unknown representation type %d", (int) md->repres.repres);
			return false;
	}
	if (my_fprintf (f, errbuf, errbsize, ";\n\ttype %s {\n", md->type->name) < 0)
		return false;
	switch (md->type->type) {
		case FRACTAL_MANDELBROT: {
			if (!fwrite_mandel_julia (f, md->type_param, errbuf, errbsize))
				return false;
			break;
		}
		case FRACTAL_JULIA: {
			if (!fwrite_mandel_julia (f, md->type_param, errbuf, errbsize))
				return false;
			const struct julia_param *jparam = (const struct julia_param *) md->type_param;
			if (my_gmp_fprintf (f, errbuf, errbsize, "\t\tparameter %.20Ff/%.20Ff;\n", jparam->param.real, jparam->param.imag) < 0)
				return false;
			break;
		}
		default: {
			snprintf (errbuf, errbsize, "Unknown fractal type %d", (int) md->type->type);
			return false;
		}
	}
	if (my_fprintf (f, errbuf, errbsize, "\t};\n};\n") < 0)
		return false;
	return true;
}


static bool
fwrite_mandel_julia (FILE *f, const struct mandel_julia_param *param, char *errbuf, size_t errbsize)
{
	if (my_fprintf (f, errbuf, errbsize, "\t\tzpower %u;\n\t\tmaxiter %u;\n", param->zpower, param->maxiter) < 0)
		return false;
	return true;
}


bool
write_mandeldata (const char *filename, const struct mandeldata *md, char *errbuf, size_t errbsize)
{
	bool res;
	FILE *f = fopen (filename, "w");
	if (f == NULL) {
		my_safe_strcpy (errbuf, strerror (errno), errbsize);
		return false;
	}
	res = fwrite_mandeldata (f, md, errbuf, errbsize);
	fclose (f);
	return res;
}
