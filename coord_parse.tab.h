/* A Bison parser, made by GNU Bison 3.0.4.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

#ifndef YY_COORD_COORD_PARSE_TAB_H_INCLUDED
# define YY_COORD_COORD_PARSE_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int coord_debug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    TOKEN_INT = 258,
    TOKEN_REAL = 259,
    TOKEN_COORD_V1 = 260,
    TOKEN_TYPE = 261,
    TOKEN_MANDELBROT = 262,
    TOKEN_JULIA = 263,
    TOKEN_AREA = 264,
    TOKEN_ZPOWER = 265,
    TOKEN_MAXITER = 266,
    TOKEN_PARAMETER = 267,
    TOKEN_REPRESENTATION = 268,
    TOKEN_ESCAPE = 269,
    TOKEN_ESCAPE_LOG = 270,
    TOKEN_DISTANCE = 271,
    TOKEN_BASE = 272,
    TOKEN_IDENTIFIER = 273,
    TOKEN_LEX_ERROR = 274
  };
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED

union YYSTYPE
{
#line 24 "coord_parse.y" /* yacc.c:1909  */

	char *string;
	struct mandel_point mandel_point;
	struct mandel_area mandel_area;
	struct mandeldata *mandeldata;
	struct mdparam *mdparam;
	struct coordparam *coordparam;
	struct mandel_repres *repres;

#line 84 "coord_parse.tab.h" /* yacc.c:1909  */
};

typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif

/* Location type.  */
#if ! defined YYLTYPE && ! defined YYLTYPE_IS_DECLARED
typedef struct YYLTYPE YYLTYPE;
struct YYLTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
};
# define YYLTYPE_IS_DECLARED 1
# define YYLTYPE_IS_TRIVIAL 1
#endif



int coord_parse (void *scanner, struct mandeldata *md, char *errbuf, size_t errbsize);

#endif /* !YY_COORD_COORD_PARSE_TAB_H_INCLUDED  */
