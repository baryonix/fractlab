%option noyywrap
%option reentrant
%option bison-bridge
%option bison-locations

%{
#include <string.h>
#include "mandelbrot.h"
#include "coord_parse.tab.h"

#define YY_USER_ACTION {yylloc->first_column = yylloc->last_column; yylloc->last_column += yyleng;}
%}

DIGIT		[0-9]
INT			[-+]?{DIGIT}+
REAL		[-+]?{DIGIT}+(\.{DIGIT}+)?([eE]{INT})?
IDENTIFIER	[A-Za-z_][-A-Za-z_0-9]*
COMMENT		\/\/[^\n]*

%%

{INT} {
	yylval->string = strdup (yytext);
	return TOKEN_INT;
}

{REAL} {
	yylval->string = strdup (yytext);
	return TOKEN_REAL;
}

coord-v1				return TOKEN_COORD_V1;
type					return TOKEN_TYPE;
mandelbrot				return TOKEN_MANDELBROT;
julia					return TOKEN_JULIA;
zpower					return TOKEN_ZPOWER;
maxiter					return TOKEN_MAXITER;
area					return TOKEN_AREA;
parameter				return TOKEN_PARAMETER;
representation			return TOKEN_REPRESENTATION;
escape					return TOKEN_ESCAPE;
escape-log				return TOKEN_ESCAPE_LOG;
distance				return TOKEN_DISTANCE;
base					return TOKEN_BASE;
{IDENTIFIER}			return TOKEN_IDENTIFIER;
\n						yylloc->first_line++; yylloc->last_column = 0;
[[:space:]]				/* do nothing */
[;\{\}/]				return *yytext;
{COMMENT}				/* do nothing */
.						return TOKEN_INVALID_CHAR;
