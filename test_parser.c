#include <stdio.h>
#include <stdlib.h>

#include "mandelbrot.h"
#include "file.h"
#include "coord_parse.tab.h"

int coord_parse (void);
extern struct mandeldata *coord_parser_mandeldata;

void
coord_error (const char *s)
{
	fprintf (stderr, "* ERROR: %s\n", s);
	exit (2);
}

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
	coord_parse ();
	fclose (f);
	fwrite_mandeldata (stdout, md);
	mandeldata_clear (md);
	/*printf ("%x\n", yylex ());
	printf ("%x\n", yylex ());
	printf ("%x\n", yylex ());
	printf ("%x\n", yylex ());
	printf ("%x\n", yylex ());*/
	return 0;
}
