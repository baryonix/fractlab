#include <stdio.h>
#include <string.h>

#include <gmp.h>

#include "file.h"
#include "util.h"


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
fread_mandeldata (FILE *f, struct mandeldata *md)
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

	r = fscanf (f, "%lf\n", &md->log_factor);
	if (r == EOF || r < 1)
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


/* XXX some more error checking must be performed here */
bool
fwrite_mandeldata (FILE *f, const struct mandeldata *md)
{
	char creal[1024], cimag[1024], magf[1024];
	if (center_coords_to_string (md->area.center.real, md->area.center.imag, md->area.magf, creal, cimag, magf, 1024) < 0)
		return false;
	fprintf (f, "%s\n%s\n%s\n%s\n%f\n", md->type->name, creal, cimag, magf, md->log_factor);
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
			gmp_fprintf (f, "%.20Ff\n%.20Ff\n", jparam->param.real, jparam->param.imag);
			break;
		}
		default: {
			fprintf (stderr, "* BUG: unknown fractal type at %s line %d\n", __FILE__, __LINE__);
			return false;
		}
	}
	return true;
}


static bool
fwrite_mandel_julia (FILE *f, const struct mandel_julia_param *param)
{
	fprintf (f, "%u\n%u\n", param->zpower, param->maxiter);
	return true;
}
