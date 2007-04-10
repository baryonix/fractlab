#include <stdio.h>
#include <stdlib.h>

#include "fractal-render.h"
#include "file.h"
#include "coord_parse.tab.h"

typedef void *yyscan_t;
int coord_lex_init (yyscan_t *scanner);
void coord_restart (FILE *input_file, yyscan_t yyscanner);
void coord_lex_destroy (yyscan_t yyscanner);
int coord_parse (yyscan_t scanner, struct mandeldata *md, char *errbuf, size_t errbsize);

int
main (int argc, char *argv[])
{
	char errbuf[128];
	mpf_set_default_prec (1024);
	struct mandeldata md[1];
	yyscan_t scanner;
	if (argc != 2) {
		fprintf (stderr, "* USAGE: %s <coord file>\n", argv[0]);
		return 1;
	}
	FILE *f = fopen (argv[1], "r");
	if (f == NULL) {
		perror ("fopen");
		return 1;
	}
	char filebuf[65536];
	filebuf[fread (filebuf, 1, 65536, f)] = 0;
	coord_lex_init (&scanner);
	coord__scan_string (filebuf, scanner);
	//coord_restart (f, scanner);
	if (coord_parse (scanner, md, errbuf, sizeof (errbuf)) != 0) {
		fprintf (stderr, "* ERROR: %s\n", errbuf);
		return 1;
	}
	struct io_buffer iob[1];
	io_buffer_init (iob, NULL, 1024);
	struct io_stream stream[1];
	io_stream_init_buffer (stream, iob);
	generic_write_mandeldata (stream, md, false, errbuf, sizeof (errbuf));
	printf ("[%s]\n", iob->buf);
#if 0
	mandeldata_clear (md);
	if (coord_parse (scanner, md, errbuf, sizeof (errbuf)) != 0) {
		fprintf (stderr, "* ERROR: %s\n", errbuf);
		return 1;
	}
	fwrite_mandeldata (stdout, md, false, errbuf, sizeof (errbuf));
#endif
	fgets (errbuf, sizeof (errbuf), f);
	printf ("after coords: [%s]\n", errbuf);
	mandeldata_clear (md);
	coord_lex_destroy (scanner);
	fclose (f);
	return 0;
}
