#ifndef YYERRCODE
#define YYERRCODE 256
#endif

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
typedef union {
	char *string;
	struct mandel_point mandel_point;
	struct mandel_area mandel_area;
	struct mandeldata *mandeldata;
	struct mdparam *mdparam;
	struct coordparam *coordparam;
	struct mandel_repres *repres;
} YYSTYPE;
extern YYSTYPE coord_lval;
