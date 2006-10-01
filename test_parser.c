#include <stdio.h>

int yyparse (void);

void
yyerror (const char *s)
{
	fprintf (stderr, "error %s\n", s);
}

int
main (void)
{
	yyparse ();
	/*printf ("%x\n", yylex ());
	printf ("%x\n", yylex ());
	printf ("%x\n", yylex ());
	printf ("%x\n", yylex ());
	printf ("%x\n", yylex ());*/
	return 0;
}
