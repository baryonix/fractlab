#ifndef lint
static char const 
yyrcsid[] = "$FreeBSD: src/usr.bin/yacc/skeleton.c,v 1.28 2000/01/17 02:04:06 bde Exp $";
#endif
#include <stdlib.h>
#define YYBYACC 1
#define YYMAJOR 1
#define YYMINOR 9
#define YYLEX yylex()
#define YYEMPTY -1
#define yyclearin (yychar=(YYEMPTY))
#define yyerrok (yyerrflag=0)
#define YYRECOVERING() (yyerrflag!=0)
static int yygrowstack();
#define yyparse coord_parse
#define yylex coord_lex
#define yyerror coord_error
#define yychar coord_char
#define yyval coord_val
#define yylval coord_lval
#define yydebug coord_debug
#define yynerrs coord_nerrs
#define yyerrflag coord_errflag
#define yyss coord_ss
#define yyssp coord_ssp
#define yyvs coord_vs
#define yyvsp coord_vsp
#define yylhs coord_lhs
#define yylen coord_len
#define yydefred coord_defred
#define yydgoto coord_dgoto
#define yysindex coord_sindex
#define yyrindex coord_rindex
#define yygindex coord_gindex
#define yytable coord_table
#define yycheck coord_check
#define yyname coord_name
#define yyrule coord_rule
#define yysslim coord_sslim
#define yystacksize coord_stacksize
#define YYPREFIX "coord_"
#line 2 "coord_parse.y"
#include <stdio.h>
#include <string.h>
#include "mandelbrot.h"

struct mdparam;
struct coordparam;

#line 11 "coord_parse.y"
typedef union {
	char *string;
	struct mandel_point mandel_point;
	struct mandel_area mandel_area;
	struct mandeldata *mandeldata;
	struct mdparam *mdparam;
	struct coordparam *coordparam;
	struct mandel_repres *repres;
} YYSTYPE;
#line 22 "coord_parse.y"
typedef void (*set_func_t) (struct mandeldata *md, struct mdparam *param);

struct mandeldata *coord_parser_mandeldata;

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

#line 152 "coord_parse.tab.c"
#define YYERRCODE 256
#define TOKEN_INT 257
#define TOKEN_REAL 258
#define TOKEN_COORD_V1 259
#define TOKEN_TYPE 260
#define TOKEN_MANDELBROT 261
#define TOKEN_JULIA 262
#define TOKEN_AREA 263
#define TOKEN_ZPOWER 264
#define TOKEN_MAXITER 265
#define TOKEN_PARAMETER 266
#define TOKEN_REPRESENTATION 267
#define TOKEN_ESCAPE 268
#define TOKEN_ESCAPE_LOG 269
#define TOKEN_DISTANCE 270
#define TOKEN_BASE 271
#define TOKEN_IDENTIFIER 272
const short coord_lhs[] = {                                        -1,
    1,    1,    0,    4,    4,    4,    4,    5,    5,    7,
    7,    6,    9,    9,    8,    8,   10,   10,    2,    3,
   11,   11,   11,   12,   12,
};
const short coord_len[] = {                                         2,
    1,    1,    5,    0,    7,    7,    3,    2,    2,    0,
    3,    1,    0,    3,    1,    2,    2,    2,    3,    3,
    1,    4,    1,    0,    4,
};
const short coord_defred[] = {                                      0,
    0,    0,    4,    0,    0,    0,    0,    0,    0,    0,
    0,    1,    2,    0,    0,    8,   21,    0,   23,    9,
    3,    7,   10,   13,    0,    0,   24,    0,    0,   19,
   20,    0,    0,    0,    0,    0,   12,    0,    0,    0,
   15,    0,   22,   17,   18,    5,   11,   16,    6,   14,
    0,   25,
};
const short coord_dgoto[] = {                                       2,
   14,   15,   16,    4,    9,   36,   28,   40,   29,   37,
   20,   32,
};
const short coord_sindex[] = {                                   -245,
 -110,    0,    0, -124, -252, -246, -263,  -44,  -43, -106,
 -105,    0,    0,  -28,  -26,    0,    0, -101,    0,    0,
    0,    0,    0,    0, -246, -246,    0, -123, -117,    0,
    0, -125, -234, -233,  -34,  -33,    0, -246,  -32,  -31,
    0, -246,    0,    0,    0,    0,    0,    0,    0,    0,
  -30,    0,
};
const short coord_rindex[] = {                                      0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,
};
const short coord_gindex[] = {                                      0,
  -22,   -8,    0,    0,    0,    0,    0,    0,    0,    2,
    0,    0,
};
#define YYTABLESIZE 149
const short coord_table[] = {                                      43,
    8,   35,   30,   31,   17,   18,   19,   39,   10,   11,
   12,   13,    3,    1,   21,   22,   23,   24,   25,   51,
   26,   27,   44,   45,   46,   47,   49,   50,   52,   48,
   41,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    5,    0,    0,    6,    0,
   33,   34,    7,    0,    0,   42,   33,   34,   38,
};
const short coord_check[] = {                                     125,
  125,  125,   25,   26,  268,  269,  270,  125,  261,  262,
  257,  258,  123,  259,   59,   59,  123,  123,   47,   42,
   47,  123,  257,  257,   59,   59,   59,   59,   59,   38,
   29,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,   -1,
   -1,   -1,   -1,   -1,   -1,  260,   -1,   -1,  263,   -1,
  264,  265,  267,   -1,   -1,  271,  264,  265,  266,
};
#define YYFINAL 2
#ifndef YYDEBUG
#define YYDEBUG 0
#endif
#define YYMAXTOKEN 272
#if YYDEBUG
const char * const coord_name[] = {
"end-of-file",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,"'/'",0,0,0,0,0,0,0,0,0,0,0,"';'",0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,"'{'",0,"'}'",0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,"TOKEN_INT",
"TOKEN_REAL","TOKEN_COORD_V1","TOKEN_TYPE","TOKEN_MANDELBROT","TOKEN_JULIA",
"TOKEN_AREA","TOKEN_ZPOWER","TOKEN_MAXITER","TOKEN_PARAMETER",
"TOKEN_REPRESENTATION","TOKEN_ESCAPE","TOKEN_ESCAPE_LOG","TOKEN_DISTANCE",
"TOKEN_BASE","TOKEN_IDENTIFIER",
};
const char * const coord_rule[] = {
"$accept : coord",
"real : TOKEN_INT",
"real : TOKEN_REAL",
"coord : TOKEN_COORD_V1 '{' coord_params '}' ';'",
"coord_params :",
"coord_params : coord_params TOKEN_TYPE TOKEN_MANDELBROT '{' mandelbrot_params '}' ';'",
"coord_params : coord_params TOKEN_TYPE TOKEN_JULIA '{' julia_params '}' ';'",
"coord_params : coord_params coord_param ';'",
"coord_param : TOKEN_AREA area_desc",
"coord_param : TOKEN_REPRESENTATION repres_desc",
"mandelbrot_params :",
"mandelbrot_params : mandelbrot_params mandelbrot_param ';'",
"mandelbrot_param : mandel_julia_param",
"julia_params :",
"julia_params : julia_params julia_param ';'",
"julia_param : mandel_julia_param",
"julia_param : TOKEN_PARAMETER point_desc",
"mandel_julia_param : TOKEN_ZPOWER TOKEN_INT",
"mandel_julia_param : TOKEN_MAXITER TOKEN_INT",
"point_desc : real '/' real",
"area_desc : point_desc '/' real",
"repres_desc : TOKEN_ESCAPE",
"repres_desc : TOKEN_ESCAPE_LOG '{' escape_log_params '}'",
"repres_desc : TOKEN_DISTANCE",
"escape_log_params :",
"escape_log_params : escape_log_params TOKEN_BASE real ';'",
};
#endif
#if YYDEBUG
#include <stdio.h>
#endif
#ifdef YYSTACKSIZE
#undef YYMAXDEPTH
#define YYMAXDEPTH YYSTACKSIZE
#else
#ifdef YYMAXDEPTH
#define YYSTACKSIZE YYMAXDEPTH
#else
#define YYSTACKSIZE 10000
#define YYMAXDEPTH 10000
#endif
#endif
#define YYINITSTACKSIZE 200
int yydebug;
int yynerrs;
int yyerrflag;
int yychar;
short *yyssp;
YYSTYPE *yyvsp;
YYSTYPE yyval;
YYSTYPE yylval;
short *yyss;
short *yysslim;
YYSTYPE *yyvs;
int yystacksize;
/* allocate initial stack or double stack size, up to YYMAXDEPTH */
static int yygrowstack()
{
    int newsize, i;
    short *newss;
    YYSTYPE *newvs;

    if ((newsize = yystacksize) == 0)
        newsize = YYINITSTACKSIZE;
    else if (newsize >= YYMAXDEPTH)
        return -1;
    else if ((newsize *= 2) > YYMAXDEPTH)
        newsize = YYMAXDEPTH;
    i = yyssp - yyss;
    newss = yyss ? (short *)realloc(yyss, newsize * sizeof *newss) :
      (short *)malloc(newsize * sizeof *newss);
    if (newss == NULL)
        return -1;
    yyss = newss;
    yyssp = newss + i;
    newvs = yyvs ? (YYSTYPE *)realloc(yyvs, newsize * sizeof *newvs) :
      (YYSTYPE *)malloc(newsize * sizeof *newvs);
    if (newvs == NULL)
        return -1;
    yyvs = newvs;
    yyvsp = newvs + i;
    yystacksize = newsize;
    yysslim = yyss + newsize - 1;
    return 0;
}

#define YYABORT goto yyabort
#define YYREJECT goto yyabort
#define YYACCEPT goto yyaccept
#define YYERROR goto yyerrlab

#ifndef YYPARSE_PARAM
#if defined(__cplusplus) || __STDC__
#define YYPARSE_PARAM_ARG void
#define YYPARSE_PARAM_DECL
#else	/* ! ANSI-C/C++ */
#define YYPARSE_PARAM_ARG
#define YYPARSE_PARAM_DECL
#endif	/* ANSI-C/C++ */
#else	/* YYPARSE_PARAM */
#ifndef YYPARSE_PARAM_TYPE
#define YYPARSE_PARAM_TYPE void *
#endif
#if defined(__cplusplus) || __STDC__
#define YYPARSE_PARAM_ARG YYPARSE_PARAM_TYPE YYPARSE_PARAM
#define YYPARSE_PARAM_DECL
#else	/* ! ANSI-C/C++ */
#define YYPARSE_PARAM_ARG YYPARSE_PARAM
#define YYPARSE_PARAM_DECL YYPARSE_PARAM_TYPE YYPARSE_PARAM;
#endif	/* ANSI-C/C++ */
#endif	/* ! YYPARSE_PARAM */

int
yyparse (YYPARSE_PARAM_ARG)
    YYPARSE_PARAM_DECL
{
    register int yym, yyn, yystate;
#if YYDEBUG
    register const char *yys;

    if ((yys = getenv("YYDEBUG")))
    {
        yyn = *yys;
        if (yyn >= '0' && yyn <= '9')
            yydebug = yyn - '0';
    }
#endif

    yynerrs = 0;
    yyerrflag = 0;
    yychar = (-1);

    if (yyss == NULL && yygrowstack()) goto yyoverflow;
    yyssp = yyss;
    yyvsp = yyvs;
    *yyssp = yystate = 0;

yyloop:
    if ((yyn = yydefred[yystate])) goto yyreduce;
    if (yychar < 0)
    {
        if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, reading %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
    }
    if ((yyn = yysindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: state %d, shifting to state %d\n",
                    YYPREFIX, yystate, yytable[yyn]);
#endif
        if (yyssp >= yysslim && yygrowstack())
        {
            goto yyoverflow;
        }
        *++yyssp = yystate = yytable[yyn];
        *++yyvsp = yylval;
        yychar = (-1);
        if (yyerrflag > 0)  --yyerrflag;
        goto yyloop;
    }
    if ((yyn = yyrindex[yystate]) && (yyn += yychar) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yychar)
    {
        yyn = yytable[yyn];
        goto yyreduce;
    }
    if (yyerrflag) goto yyinrecovery;
#if defined(lint) || defined(__GNUC__)
    goto yynewerror;
#endif
yynewerror:
    yyerror("syntax error");
#if defined(lint) || defined(__GNUC__)
    goto yyerrlab;
#endif
yyerrlab:
    ++yynerrs;
yyinrecovery:
    if (yyerrflag < 3)
    {
        yyerrflag = 3;
        for (;;)
        {
            if ((yyn = yysindex[*yyssp]) && (yyn += YYERRCODE) >= 0 &&
                    yyn <= YYTABLESIZE && yycheck[yyn] == YYERRCODE)
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: state %d, error recovery shifting\
 to state %d\n", YYPREFIX, *yyssp, yytable[yyn]);
#endif
                if (yyssp >= yysslim && yygrowstack())
                {
                    goto yyoverflow;
                }
                *++yyssp = yystate = yytable[yyn];
                *++yyvsp = yylval;
                goto yyloop;
            }
            else
            {
#if YYDEBUG
                if (yydebug)
                    printf("%sdebug: error recovery discarding state %d\n",
                            YYPREFIX, *yyssp);
#endif
                if (yyssp <= yyss) goto yyabort;
                --yyssp;
                --yyvsp;
            }
        }
    }
    else
    {
        if (yychar == 0) goto yyabort;
#if YYDEBUG
        if (yydebug)
        {
            yys = 0;
            if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
            if (!yys) yys = "illegal-symbol";
            printf("%sdebug: state %d, error recovery discards token %d (%s)\n",
                    YYPREFIX, yystate, yychar, yys);
        }
#endif
        yychar = (-1);
        goto yyloop;
    }
yyreduce:
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: state %d, reducing by rule %d (%s)\n",
                YYPREFIX, yystate, yyn, yyrule[yyn]);
#endif
    yym = yylen[yyn];
    yyval = yyvsp[1-yym];
    switch (yyn)
    {
case 1:
#line 147 "coord_parse.y"
{ yyval.string = yyvsp[0].string; }
break;
case 2:
#line 148 "coord_parse.y"
{ yyval.string = yyvsp[0].string; }
break;
case 3:
#line 151 "coord_parse.y"
{
						mandeldata_init (coord_parser_mandeldata, fractal_type_by_id (yyvsp[-2].coordparam->type));
						mandeldata_set_defaults (coord_parser_mandeldata);
						yyvsp[-2].coordparam->param->set_func (coord_parser_mandeldata, yyvsp[-2].coordparam->param);
						free (yyvsp[-2].coordparam);
						YYACCEPT;
					}
break;
case 4:
#line 160 "coord_parse.y"
{
						yyval.coordparam = malloc (sizeof (*yyval.coordparam));
						yyval.coordparam->param = mdparam_new (set_compound);
					}
break;
case 5:
#line 164 "coord_parse.y"
{
						yyval.coordparam = yyvsp[-6].coordparam;
						yyval.coordparam->type = FRACTAL_MANDELBROT;
						add_to_compound (yyval.coordparam->param, yyvsp[-2].mdparam);
					}
break;
case 6:
#line 169 "coord_parse.y"
{
						yyval.coordparam = yyvsp[-6].coordparam;
						yyval.coordparam->type = FRACTAL_JULIA;
						add_to_compound (yyval.coordparam->param, yyvsp[-2].mdparam);
					}
break;
case 7:
#line 174 "coord_parse.y"
{
						yyval.coordparam = yyvsp[-2].coordparam;
						add_to_compound (yyval.coordparam->param, yyvsp[-1].mdparam);
					}
break;
case 8:
#line 180 "coord_parse.y"
{
						yyval.mdparam = mdparam_new (set_area);
						memcpy (&yyval.mdparam->data.mandel_area, &yyvsp[0].mandel_area, sizeof (yyval.mdparam->data.mandel_area));
					}
break;
case 9:
#line 184 "coord_parse.y"
{
						yyval.mdparam = mdparam_new (set_repres);
						yyval.mdparam->data.repres = yyvsp[0].repres;
					}
break;
case 10:
#line 190 "coord_parse.y"
{
						yyval.mdparam = mdparam_new (set_compound);
					}
break;
case 11:
#line 193 "coord_parse.y"
{
						yyval.mdparam = yyvsp[-2].mdparam;
						add_to_compound (yyval.mdparam, yyvsp[-1].mdparam);
					}
break;
case 12:
#line 199 "coord_parse.y"
{
						yyval.mdparam = yyvsp[0].mdparam;
					}
break;
case 13:
#line 204 "coord_parse.y"
{
						yyval.mdparam = mdparam_new (set_compound);
					}
break;
case 14:
#line 207 "coord_parse.y"
{
						yyval.mdparam = yyvsp[-2].mdparam;
						add_to_compound (yyval.mdparam, yyvsp[-1].mdparam);
					}
break;
case 15:
#line 213 "coord_parse.y"
{
						yyval.mdparam = yyvsp[0].mdparam;
					}
break;
case 16:
#line 216 "coord_parse.y"
{
						yyval.mdparam = mdparam_new (set_julia_parameter);
						memcpy (&yyval.mdparam->data.mandel_point, &yyvsp[0].mandel_point, sizeof (yyval.mdparam->data.mandel_point));
					}
break;
case 17:
#line 222 "coord_parse.y"
{
						yyval.mdparam = mdparam_new (set_zpower);
						yyval.mdparam->data.string = yyvsp[0].string;
					}
break;
case 18:
#line 226 "coord_parse.y"
{
						yyval.mdparam = mdparam_new (set_maxiter);
						yyval.mdparam->data.string = yyvsp[0].string;
					}
break;
case 19:
#line 232 "coord_parse.y"
{
						mandel_point_init (&yyval.mandel_point);
						mpf_set_str (yyval.mandel_point.real, yyvsp[-2].string, 10);
						mpf_set_str (yyval.mandel_point.imag, yyvsp[0].string, 10);
						free (yyvsp[-2].string);
						free (yyvsp[0].string);
					}
break;
case 20:
#line 241 "coord_parse.y"
{
						mandel_area_init (&yyval.mandel_area);
						mpf_set (yyval.mandel_area.center.real, yyvsp[-2].mandel_point.real);
						mpf_set (yyval.mandel_area.center.imag, yyvsp[-2].mandel_point.imag);
						mandel_point_clear (&yyvsp[-2].mandel_point);
						mpf_set_str (yyval.mandel_area.magf, yyvsp[0].string, 10);
						free (yyvsp[0].string);
					}
break;
case 21:
#line 251 "coord_parse.y"
{
						yyval.repres = malloc (sizeof (*yyval.repres));
						yyval.repres->repres = REPRES_ESCAPE;
					}
break;
case 22:
#line 255 "coord_parse.y"
{
						yyval.repres = yyvsp[-1].repres;
						yyval.repres->repres = REPRES_ESCAPE_LOG;
					}
break;
case 23:
#line 259 "coord_parse.y"
{
						yyval.repres = malloc (sizeof (*yyval.repres));
						yyval.repres->repres = REPRES_DISTANCE;
					}
break;
case 24:
#line 265 "coord_parse.y"
{
						yyval.repres = malloc (sizeof (*yyval.repres));
					}
break;
case 25:
#line 268 "coord_parse.y"
{
						yyval.repres = yyvsp[-3].repres;
						yyvsp[-3].repres->params.log_base = strtod (yyvsp[-1].string, NULL);
						free (yyvsp[-1].string);
					}
break;
#line 693 "coord_parse.tab.c"
    }
    yyssp -= yym;
    yystate = *yyssp;
    yyvsp -= yym;
    yym = yylhs[yyn];
    if (yystate == 0 && yym == 0)
    {
#if YYDEBUG
        if (yydebug)
            printf("%sdebug: after reduction, shifting from state 0 to\
 state %d\n", YYPREFIX, YYFINAL);
#endif
        yystate = YYFINAL;
        *++yyssp = YYFINAL;
        *++yyvsp = yyval;
        if (yychar < 0)
        {
            if ((yychar = yylex()) < 0) yychar = 0;
#if YYDEBUG
            if (yydebug)
            {
                yys = 0;
                if (yychar <= YYMAXTOKEN) yys = yyname[yychar];
                if (!yys) yys = "illegal-symbol";
                printf("%sdebug: state %d, reading %d (%s)\n",
                        YYPREFIX, YYFINAL, yychar, yys);
            }
#endif
        }
        if (yychar == 0) goto yyaccept;
        goto yyloop;
    }
    if ((yyn = yygindex[yym]) && (yyn += yystate) >= 0 &&
            yyn <= YYTABLESIZE && yycheck[yyn] == yystate)
        yystate = yytable[yyn];
    else
        yystate = yydgoto[yym];
#if YYDEBUG
    if (yydebug)
        printf("%sdebug: after reduction, shifting from state %d \
to state %d\n", YYPREFIX, *yyssp, yystate);
#endif
    if (yyssp >= yysslim && yygrowstack())
    {
        goto yyoverflow;
    }
    *++yyssp = yystate;
    *++yyvsp = yyval;
    goto yyloop;
yyoverflow:
    yyerror("yacc stack overflow");
yyabort:
    return (1);
yyaccept:
    return (0);
}
