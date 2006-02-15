#include <stdio.h>

#include <gmp.h>

#include "file.h"


bool
read_cmag_coords_from_file (const char *filename, mpf_t xmin, mpf_t xmax, mpf_t ymin, mpf_t ymax)
{
	FILE *f = fopen (filename, "r");
	if (f == NULL)
		return false;

	mpf_t xc, yc, magf;

	mpf_init (xc);
	mpf_init (yc);
	mpf_init (magf);

	char buf[1024];
	fgets (buf, 1024, f);
	mpf_set_str (xc, buf, 10);
	fgets (buf, 1024, f);
	mpf_set_str (yc, buf, 10);
	fgets (buf, 1024, f);
	mpf_set_str (magf, buf, 10);

	fclose (f);

	mpf_ui_div (magf, 1, magf);

	mpf_sub (xmin, xc, magf);
	mpf_add (xmax, xc, magf);
	mpf_sub (ymin, yc, magf);
	mpf_add (ymax, yc, magf);

	mpf_clear (xc);
	mpf_clear (yc);
	mpf_clear (magf);

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
