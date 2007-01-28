#include <stdio.h>
#include <stdlib.h>

#include "mandelbrot.h"
#include "file.h"
#include "coord_parse.tab.h"

int coord_parse (void);
extern struct mandeldata *coord_parser_mandeldata;
extern const char *coord_errstr;

int
main (int argc, char *argv[])
{
	mpf_set_default_prec (1024);
	struct mandeldata md[1];
	coord_parser_mandeldata = md;
	if (argc != 2) {
		fprintf (stderr, "* USAGE: %s <coord file>\n", argv[0]);
		return 1;
	}
	FILE *f = fopen (argv[1], "r");
	if (f == NULL) {
		perror ("fopen");
		return 1;
	}
	coord_restart (f);
	if (coord_parse () != 0) {
		fprintf (stderr, "* ERROR: %s\n", coord_errstr);
		return 1;
	}
	fwrite_mandeldata (stdout, md);
#if 1
	mandeldata_clear (md);
	if (coord_parse () != 0) {
		fprintf (stderr, "* ERROR: %s\n", coord_errstr);
		return 1;
	}
	fwrite_mandeldata (stdout, md);
#endif
	mandeldata_clear (md);
	fclose (f);
	return 0;
}
