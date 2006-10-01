%{
	#include "coord_parse.tab.h"
%}

DIGIT		[0-9]
INT			[-+]?{DIGIT}+
FLOAT		[-+]?{DIGIT}*\.{DIGIT}+([eE][-+]?{DIGIT}+)?

%%

{INT} {
	yylval.integer = atoi (yytext);
	return TOKEN_INT;
}

{FLOAT} {
	yylval.floating_point = atof (yytext);
	return TOKEN_FLOAT;
}

maxiter {
	return TOKEN_MAXITER;
}

corners {
	return TOKEN_CORNERS;
}

center_mag {
	return TOKEN_CENTER_MAG;
}

\n {
	return *yytext;
}

[[:space:]]+ {
	return TOKEN_SPACE;
}
