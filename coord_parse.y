%error-verbose
%locations
%pure-parser

%parse-param {yyscan_t scanner}
%parse-param {struct mandeldata *md}
%parse-param {char *errbuf}
%parse-param {size_t errbsize}

%lex-param {yyscan_t scanner}

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mandelbrot.h"
#include "util.h"

struct mdparam;
struct coordparam;

%}

%union {
	char *string;
	struct mandel_point mandel_point;
	struct mandel_area mandel_area;
	struct mandeldata *mandeldata;
	struct mdparam *mdparam;
	struct coordparam *coordparam;
	struct mandel_repres *repres;
}

%{
#include "coord_lex.yy.h"

/*
 * We must access yychar (the lookahead token) and yylval for error reporting,
 * but in the pure world they are local variables within yyparse(). Thus, we
 * define yyerror() as a macro which passes them as arguments to the actual
 * error reporting function.
 */
#define coord_error(loc, scanner, md, errbuf, errbsize, msg) (coord_error_func (loc, scanner, md, errbuf, errbsize, msg, yychar, &yylval))

static void
coord_error_func (YYLTYPE *loc, yyscan_t scanner, struct mandeldata *md, char *errbuf, size_t errbsize, char const *msg, int lookahead, YYSTYPE *lval)
{
	switch (lookahead) {
		case TOKEN_LEX_ERROR:
			msg = lval->string;
			break;
		case 0:
			msg = "Unexpected EOF";
			break;
		default:
			break;
	}
	errbuf[0] = 0; /* default value in case snprintf barfs */
	if (loc->last_column > loc->first_column + 1)
		snprintf (errbuf, errbsize, "%s in line %d, columns %d-%d", msg, loc->first_line, loc->first_column + 1, loc->last_column);
	else
		snprintf (errbuf, errbsize, "%s in line %d, column %d", msg, loc->first_line, loc->first_column + 1);
	if (lookahead == TOKEN_LEX_ERROR)
		free (lval->string);
}


typedef void (*set_func_t) (struct mandeldata *md, struct mdparam *param);

struct coordparam {
	fractal_type_t type;
	struct mdparam *param;
};

struct mdparam {
	set_func_t set_func;
	struct mdparam *next;
	YYSTYPE data;
};

static struct mdparam *
mdparam_new (set_func_t set_func)
{
	struct mdparam *param = malloc (sizeof (*param));
	memset (param, 0, sizeof (*param));
	param->set_func = set_func;
	return param;
}

static void
set_zpower (struct mandeldata *md, struct mdparam *param)
{
	struct mandel_julia_param *mjparam = (struct mandel_julia_param *) md->type_param;
	mjparam->zpower = atoi (param->data.string);
	free (param->data.string);
	free (param);
}

static void
set_maxiter (struct mandeldata *md, struct mdparam *param)
{
	struct mandel_julia_param *mjparam = (struct mandel_julia_param *) md->type_param;
	mjparam->maxiter = atoi (param->data.string);
	free (param->data.string);
	free (param);
}

static void
set_julia_parameter (struct mandeldata *md, struct mdparam *param)
{
	struct julia_param *jparam = (struct julia_param *) md->type_param;
	mpf_set (jparam->param.real, param->data.mandel_point.real);
	mpf_set (jparam->param.imag, param->data.mandel_point.imag);
	mandel_point_clear (&param->data.mandel_point);
	free (param);
}

static void
set_area (struct mandeldata *md, struct mdparam *param)
{
	mpf_set (md->area.center.real, param->data.mandel_area.center.real);
	mpf_set (md->area.center.imag, param->data.mandel_area.center.imag);
	mpf_set (md->area.magf, param->data.mandel_area.magf);
	mandel_area_clear (&param->data.mandel_area);
	free (param);
}

static void
set_repres (struct mandeldata *md, struct mdparam *param)
{
	/* XXX */
	memcpy (&md->repres, param->data.repres, sizeof (md->repres));
	free (param->data.repres);
	free (param);
}

static void
set_compound (struct mandeldata *md, struct mdparam *param)
{
	struct mdparam *p = param->data.mdparam;
	while (p != NULL) {
		struct mdparam *newp = p->next;
		p->set_func (md, p);
		p = newp;
	}
	free (param);
}

static void
add_to_compound (struct mdparam *param, struct mdparam *child)
{
	child->next = param->data.mdparam;
	param->data.mdparam = child;
}

%}

%type <string> real
%type <mandel_point> point_desc
%type <mandel_area> area_desc
%type <coordparam> coord_params
%type <mdparam> coord_param
%type <mdparam> mandelbrot_param
%type <mdparam> mandelbrot_params
%type <mdparam> julia_param
%type <mdparam> julia_params
%type <mdparam> mandel_julia_param
%type <repres> repres_desc
%type <repres> escape_log_params
%token <string> TOKEN_INT
%token <string> TOKEN_REAL
%token TOKEN_COORD_V1
%token TOKEN_TYPE
%token TOKEN_MANDELBROT
%token TOKEN_JULIA
%token TOKEN_AREA
%token TOKEN_ZPOWER
%token TOKEN_MAXITER
%token TOKEN_PARAMETER
%token TOKEN_REPRESENTATION
%token TOKEN_ESCAPE
%token TOKEN_ESCAPE_LOG
%token TOKEN_DISTANCE
%token TOKEN_BASE
%token TOKEN_IDENTIFIER
%token <string> TOKEN_LEX_ERROR

%start coord

%%

real				: TOKEN_INT { $$ = $1; }
					| TOKEN_REAL { $$ = $1; }
					;

coord				: TOKEN_COORD_V1 '{' coord_params '}' ';' {
						mandeldata_init (md, fractal_type_by_id ($3->type));
						mandeldata_set_defaults (md);
						$3->param->set_func (md, $3->param);
						free ($3);
						YYACCEPT;
					}
					;

coord_params		: {
						$$ = malloc (sizeof (*$$));
						$$->param = mdparam_new (set_compound);
					}
					| coord_params TOKEN_TYPE TOKEN_MANDELBROT '{' mandelbrot_params '}' ';' {
						$$ = $1;
						$$->type = FRACTAL_MANDELBROT;
						add_to_compound ($$->param, $5);
					}
					| coord_params TOKEN_TYPE TOKEN_JULIA '{' julia_params '}' ';' {
						$$ = $1;
						$$->type = FRACTAL_JULIA;
						add_to_compound ($$->param, $5);
					}
					| coord_params coord_param ';' {
						$$ = $1;
						add_to_compound ($$->param, $2);
					}
					;

coord_param			: TOKEN_AREA area_desc {
						$$ = mdparam_new (set_area);
						memcpy (&$$->data.mandel_area, &$2, sizeof ($$->data.mandel_area));
					}
					| TOKEN_REPRESENTATION repres_desc {
						$$ = mdparam_new (set_repres);
						$$->data.repres = $2;
					}
					;

mandelbrot_params	: {
						$$ = mdparam_new (set_compound);
					}
					| mandelbrot_params mandelbrot_param ';' {
						$$ = $1;
						add_to_compound ($$, $2);
					}
					;

mandelbrot_param	: mandel_julia_param {
						$$ = $1;
					}
					;

julia_params		: {
						$$ = mdparam_new (set_compound);
					}
					| julia_params julia_param ';' {
						$$ = $1;
						add_to_compound ($$, $2);
					}
					;

julia_param			: mandel_julia_param {
						$$ = $1;
					}
					| TOKEN_PARAMETER point_desc {
						$$ = mdparam_new (set_julia_parameter);
						memcpy (&$$->data.mandel_point, &$2, sizeof ($$->data.mandel_point));
					}
					;

mandel_julia_param	: TOKEN_ZPOWER TOKEN_INT {
						$$ = mdparam_new (set_zpower);
						$$->data.string = $2;
					}
					| TOKEN_MAXITER TOKEN_INT {
						$$ = mdparam_new (set_maxiter);
						$$->data.string = $2;
					}
					;

point_desc			: real '/' real {
						mandel_point_init (&$$);
						mpf_set_str ($$.real, $1, 10);
						mpf_set_str ($$.imag, $3, 10);
						free ($1);
						free ($3);
					}
					;

area_desc			: point_desc '/' real {
						mandel_area_init (&$$);
						mpf_set ($$.center.real, $1.real);
						mpf_set ($$.center.imag, $1.imag);
						mandel_point_clear (&$1);
						mpf_set_str ($$.magf, $3, 10);
						free ($3);
					}
					;

repres_desc			: TOKEN_ESCAPE {
						$$ = malloc (sizeof (*$$));
						$$->repres = REPRES_ESCAPE;
					}
					| TOKEN_ESCAPE_LOG '{' escape_log_params '}' {
						$$ = $3;
						$$->repres = REPRES_ESCAPE_LOG;
					}
					| TOKEN_DISTANCE {
						$$ = malloc (sizeof (*$$));
						$$->repres = REPRES_DISTANCE;
					}
					;

escape_log_params	: {
						$$ = malloc (sizeof (*$$));
					}
					| escape_log_params TOKEN_BASE real ';' {
						$$ = $1;
						$1->params.log_base = strtod ($3, NULL);
						free ($3);
					}
					;
