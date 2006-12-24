#include <stdio.h>
#include <string.h>

#include <gmp.h>

#include "file.h"
#include "util.h"


bool
fread_coords_as_center (FILE *f, mpf_t xc, mpf_t yc, mpf_t magf)
{
	char buf[64];
	fgets (buf, 64, f);
	if (strcmp (buf, "center\n") == 0)
		return fread_center_coords (f, xc, yc, magf);
	else if (strcmp (buf, "corners\n") == 0) {
		mpf_t xmin, xmax, ymin, ymax;
		mpf_init (xmin);
		mpf_init (xmax);
		mpf_init (ymin);
		mpf_init (ymax);
		if (!fread_corner_coords (f, xmin, xmax, ymin, ymax)) {
			mpf_clear (xmin);
			mpf_clear (xmax);
			mpf_clear (ymin);
			mpf_clear (ymax);
			return false;
		}
		corners_to_center (xc, yc, magf, xmin, xmax, ymin, ymax);
		mpf_clear (xmin);
		mpf_clear (xmax);
		mpf_clear (ymin);
		mpf_clear (ymax);
		return true;
	} else
		return false;
}


bool
fread_coords_as_corners (FILE *f, mpf_t xmin, mpf_t xmax, mpf_t ymin, mpf_t ymax, double aspect)
{
	char buf[64];
	fgets (buf, 64, f);
	if (strcmp (buf, "center\n") == 0) {
		mpf_t cx, cy, magf;
		mpf_init (cx);
		mpf_init (cy);
		mpf_init (magf);
		if (!fread_center_coords (f, cx, cy, magf)) {
			mpf_clear (cx);
			mpf_clear (cy);
			mpf_clear (magf);
			return false;
		}
		center_to_corners (xmin, xmax, ymin, ymax, cx, cy, magf, aspect);
		return true;
	} else if (strcmp (buf, "corners\n") == 0)
		return fread_corner_coords (f, xmin, xmax, ymin, ymax);
	else
		return false;
}


bool
fread_center_coords (FILE *f, mpf_t xc, mpf_t yc, mpf_t magf)
{
	char buf[1024];
	fgets (buf, 1024, f);
	mpf_set_str (xc, buf, 10);
	fgets (buf, 1024, f);
	mpf_set_str (yc, buf, 10);
	fgets (buf, 1024, f);
	mpf_set_str (magf, buf, 10);
	return true;
}


bool
fread_corner_coords (FILE *f, mpf_t xmin, mpf_t xmax, mpf_t ymin, mpf_t ymax)
{
	char buf[1024];
	fgets (buf, 1024, f);
	mpf_set_str (xmin, buf, 10);
	fgets (buf, 1024, f);
	mpf_set_str (xmax, buf, 10);
	fgets (buf, 1024, f);
	mpf_set_str (ymin, buf, 10);
	fgets (buf, 1024, f);
	mpf_set_str (ymax, buf, 10);
	return true;
}


bool
fwrite_corner_coords (FILE *f, mpf_t xmin, mpf_t xmax, mpf_t ymin, mpf_t ymax)
{
	char xminc[1024], xmaxc[1024], yminc[1024], ymaxc[1024];
	if (corner_coords_to_string (xmin, xmax, ymin, ymax, xminc, xmaxc, yminc, ymaxc, 1024) < 0)
		return false;
	fprintf (f, "corners\n%s\n%s\n%s\n%s\n", xminc, xmaxc, yminc, ymaxc);
	return true;
}


bool
fwrite_center_coords (FILE *f, mpf_t cx, mpf_t cy, mpf_t magf)
{
	char cxc[1024], cyc[1024], magfc[1024];

	if (center_coords_to_string (cx, cy, magf, cxc, cyc, magfc, 1024) < 0)
		return false;
	fprintf (f, "center\n%s\n%s\n%s\n", cxc, cyc, magfc);
	return true;
}
