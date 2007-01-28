/* A Bison parser, made by GNU Bison 2.3.  */

/* Skeleton interface for Bison's Yacc-like parsers in C

   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

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

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
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
     TOKEN_IDENTIFIER = 273
   };
#endif
/* Tokens.  */
#define TOKEN_INT 258
#define TOKEN_REAL 259
#define TOKEN_COORD_V1 260
#define TOKEN_TYPE 261
#define TOKEN_MANDELBROT 262
#define TOKEN_JULIA 263
#define TOKEN_AREA 264
#define TOKEN_ZPOWER 265
#define TOKEN_MAXITER 266
#define TOKEN_PARAMETER 267
#define TOKEN_REPRESENTATION 268
#define TOKEN_ESCAPE 269
#define TOKEN_ESCAPE_LOG 270
#define TOKEN_DISTANCE 271
#define TOKEN_BASE 272
#define TOKEN_IDENTIFIER 273




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
#line 12 "coord_parse.y"
{
	char *string;
	struct mandel_point mandel_point;
	struct mandel_area mandel_area;
	struct mandeldata *mandeldata;
	struct mdparam *mdparam;
	struct coordparam *coordparam;
	struct mandel_repres *repres;
}
/* Line 1489 of yacc.c.  */
#line 95 "coord_parse.tab.h"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE coord_lval;

