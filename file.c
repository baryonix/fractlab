#include <stdio.h>
#include <string.h>

#include <gmp.h>

#include "file.h"
#include "util.h"


/* XXX some error checking must be performed here */
static bool
fread_mpf (FILE *f, mpf_t val)
{
	char buf[1024];
	fgets (buf, sizeof (buf), f);
	mpf_set_str (val, buf, 10);
	return true;
}


/* XXX some more error checking must be performed here */
bool
fread_mandeldata (FILE *f, struct mandeldata *md)
{
	int r;
	char buf[128];
	fgets (buf, sizeof (buf), f);
	if (strcmp (buf, "mandelbrot\n") == 0)
		md->type = FRACTAL_MANDELBROT;
	else if (strcmp (buf, "julia\n") == 0)
		md->type = FRACTAL_JULIA;
	else
		return false;

	r = fscanf (f, "%u\n", &md->zpower);
	if (r == EOF || r < 1)
		return false;

	if (!fread_mpf (f, md->area.center.real))
		return false;
	if (!fread_mpf (f, md->area.center.imag))
		return false;
	if (!fread_mpf (f, md->area.magf))
		return false;

	r = fscanf (f, "%u\n", &md->maxiter);
	if (r == EOF || r < 1)
		return false;
	r = fscanf (f, "%lf\n", &md->log_factor);
	if (r == EOF || r < 1)
		return false;

	if (md->type == FRACTAL_JULIA) {
		if (!fread_mpf (f, md->param.real))
			return false;
		if (!fread_mpf (f, md->param.imag))
			return false;
	}

	return true;
}


/* XXX some more error checking must be performed here */
bool
fwrite_mandeldata (FILE *f, struct mandeldata *md)
{
	const char *type;
	char creal[1024], cimag[1024], magf[1024];
	switch (md->type) {
		case FRACTAL_MANDELBROT:
			type = "mandelbrot";
			break;
		case FRACTAL_JULIA:
			type = "julia";
			break;
		default:
			return false;
	}
	if (center_coords_to_string (md->area.center.real, md->area.center.imag, md->area.magf, creal, cimag, magf, 1024) < 0)
		return false;
	fprintf (f, "%s\n%u\n%s\n%s\n%s\n%u\n%f\n", type, md->zpower, creal, cimag, magf, md->maxiter, md->log_factor);
	if (md->type == FRACTAL_JULIA)
		/* XXX what precision do we need here? */
		gmp_fprintf (f, "%.20Ff\n%.20Ff\n", md->param.real, md->param.imag);
	return true;
}
