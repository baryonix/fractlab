#include <stdio.h>
#include <string.h>

#include <gmp.h>

#include "file.h"
#include "util.h"

typedef void *yyscan_t;
int coord_lex_init (yyscan_t *scanner);
void coord_restart (FILE *input_file, yyscan_t yyscanner);
void coord_lex_destroy (yyscan_t yyscanner);
int coord_parse (yyscan_t scanner, struct mandeldata *md, char *errbuf, int errbsize);

static bool fread_mpf (FILE *f, mpf_ptr val);
static bool fread_mandel_point (FILE *f, struct mandel_point *point);
static bool fread_mandel_area (FILE *f, struct mandel_area *area);
static bool fread_mandel_julia (FILE *f, struct mandel_julia_param *param);
static bool fwrite_mandel_julia (FILE *f, const struct mandel_julia_param *param);


/* XXX some error checking must be performed here */
static bool
fread_mpf (FILE *f, mpf_ptr val)
{
	char buf[1024];
	fgets (buf, sizeof (buf), f);
	mpf_set_str (val, buf, 10);
	return true;
}


static bool
fread_mandel_point (FILE *f, struct mandel_point *point)
{
	if (!fread_mpf (f, point->real))
		return false;
	if (!fread_mpf (f, point->imag))
		return false;
	return true;
}


static bool
fread_mandel_area (FILE *f, struct mandel_area *area)
{
	if (!fread_mandel_point (f, &area->center))
		return false;
	if (!fread_mpf (f, area->magf))
		return false;
	return true;
}


static bool
fread_mandel_julia (FILE *f, struct mandel_julia_param *param)
{
	int r;
	r = fscanf (f, "%u\n", &param->zpower);
	if (r == EOF || r < 1)
		return false;
	r = fscanf (f, "%u\n", &param->maxiter);
	if (r == EOF || r < 1)
		return false;
	return true;
}


/* XXX some more error checking must be performed here */
bool
fread_mandeldata_legacy (FILE *f, struct mandeldata *md)
{
	int r;
	char buf[128];
	const struct fractal_type *type;
	fgets (buf, sizeof (buf), f);
	buf[strlen (buf) - 1] = 0; /* cut off newline */
	type = fractal_type_by_name (buf);
	if (type == NULL)
		return false;

	mandeldata_init (md, type);
	if (!fread_mandel_area (f, &md->area))
		return false;

	fgets (buf, sizeof (buf), f);
	if (strcmp (buf, "escape\n") == 0)
		md->repres.repres = REPRES_ESCAPE;
	else if (strcmp (buf, "escape-log\n") == 0) {
		md->repres.repres = REPRES_ESCAPE_LOG;
		r = fscanf (f, "%lf\n", &md->repres.params.log_base);
		if (r == EOF || r < 1)
			return false;
	} else if (strcmp (buf, "distance\n") == 0)
		md->repres.repres = REPRES_DISTANCE;
	else
		return false;

	switch (type->type) {
		case FRACTAL_MANDELBROT: {
			if (!fread_mandel_julia (f, md->type_param))
				return false;
			break;
		}
		case FRACTAL_JULIA: {
			if (!fread_mandel_julia (f, md->type_param))
				return false;
			struct julia_param *jparam = (struct julia_param *) md->type_param;
			if (!fread_mandel_point (f, &jparam->param))
				return false;
			break;
		}
		default: {
			fprintf (stderr, "* BUG: Unknown fractal type in %s line %d\n", __FILE__, __LINE__);
			return false;
		}
	}

	return true;
}


bool
fread_mandeldata (FILE *f, struct mandeldata *md)
{
	bool res;
	yyscan_t scanner;
	char errbuf[1024];
	coord_lex_init (&scanner);
	coord_restart (f, scanner);
	res = coord_parse (scanner, md, errbuf, sizeof (errbuf)) == 0;
	coord_lex_destroy (scanner);
	if (!res)
		fprintf (stderr, "* Error parsing coords: %s\n", errbuf);
	return res;
}


/* XXX some more error checking must be performed here */
bool
fwrite_mandeldata (FILE *f, const struct mandeldata *md)
{
	char creal[1024], cimag[1024], magf[1024];
	if (center_coords_to_string (md->area.center.real, md->area.center.imag, md->area.magf, creal, cimag, magf, 1024) < 0)
		return false;
	fprintf (f, "coord-v1 {\n\tarea %s/%s/%s;\n\trepresentation ", creal, cimag, magf);
	switch (md->repres.repres) {
		case REPRES_ESCAPE:
			fprintf (f, "escape");
			break;
		case REPRES_ESCAPE_LOG:
			fprintf (f, "escape-log {\n\t\tbase %f;\n\t}", md->repres.params.log_base);
			break;
		case REPRES_DISTANCE:
			fprintf (f, "distance");
			break;
		default:
			return false;
	}
	fprintf (f, ";\n\ttype %s {\n", md->type->name);
	switch (md->type->type) {
		case FRACTAL_MANDELBROT: {
			if (!fwrite_mandel_julia (f, md->type_param))
				return false;
			break;
		}
		case FRACTAL_JULIA: {
			if (!fwrite_mandel_julia (f, md->type_param))
				return false;
			const struct julia_param *jparam = (const struct julia_param *) md->type_param;
			gmp_fprintf (f, "\t\tparameter %.20Ff/%.20Ff;\n", jparam->param.real, jparam->param.imag);
			break;
		}
		default: {
			fprintf (stderr, "* BUG: unknown fractal type at %s line %d\n", __FILE__, __LINE__);
			return false;
		}
	}
	fprintf (f, "\t};\n};\n");
	return true;
}


static bool
fwrite_mandel_julia (FILE *f, const struct mandel_julia_param *param)
{
	fprintf (f, "\t\tzpower %u;\n\t\tmaxiter %u;\n", param->zpower, param->maxiter);
	return true;
}
