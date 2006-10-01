%{
	#include <stdio.h>
%}

%union {
	int integer;
	double floating_point;
}

%type <floating_point> floating_point
%token <integer> TOKEN_INT
%token <floating_point> TOKEN_FLOAT
%token TOKEN_MAXITER
%token TOKEN_SPACE
%token TOKEN_CORNERS
%token TOKEN_CENTER_MAG

%start params

%%

floating_point		: TOKEN_INT { $$ = $1; }
					| TOKEN_FLOAT { $$ = $1; }
					;

params				:
					| param '\n' params
					;

param				: TOKEN_MAXITER TOKEN_SPACE TOKEN_INT {
						printf ("maxiter=%d\n", $3);
					}
					| TOKEN_CORNERS TOKEN_SPACE floating_point TOKEN_SPACE floating_point TOKEN_SPACE floating_point TOKEN_SPACE floating_point {
						printf ("corners=%f %f %f %f\n", $3, $5, $7, $9);
					}
					| TOKEN_CENTER_MAG TOKEN_SPACE floating_point TOKEN_SPACE floating_point TOKEN_SPACE floating_point {
						printf ("center=%f %f mag=%f\n", $3, $5, $7);
					}
					;
