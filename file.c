#include <stdio.h>

#include <gmp.h>

#include "file.h"


bool
read_coords_from_file (const char *filename, mpf_t xc, mpf_t yc, mpf_t magf)
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
