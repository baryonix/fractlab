#include <stdio.h>

#include <gmp.h>

#include "file.h"
#include "util.h"


bool
read_center_coords_from_file (const char *filename, mpf_t xc, mpf_t yc, mpf_t magf)
{
	FILE *f = fopen (filename, "r");
	if (f == NULL)
		return false;

	char buf[1024];
	fgets (buf, 1024, f);
	mpf_set_str (xc, buf, 10);
	fgets (buf, 1024, f);
	mpf_set_str (yc, buf, 10);
	fgets (buf, 1024, f);
	mpf_set_str (magf, buf, 10);

	fclose (f);

	return true;
}


bool
read_corner_coords_from_file (const char *filename, mpf_t xmin, mpf_t xmax, mpf_t ymin, mpf_t ymax)
{
	FILE *f = fopen (filename, "r");
	if (f == NULL)
		return false;

	char buf[1024];
	fgets (buf, 1024, f);
	mpf_set_str (xmin, buf, 10);
	fgets (buf, 1024, f);
	mpf_set_str (xmax, buf, 10);
	fgets (buf, 1024, f);
	mpf_set_str (ymin, buf, 10);
	fgets (buf, 1024, f);
	mpf_set_str (ymax, buf, 10);

	fclose (f);

	return true;
}
