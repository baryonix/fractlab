#ifndef _MANDEL_MANDELBROT_H
#define _MANDEL_MANDELBROT_H

#include <stdbool.h>
#include <glib.h>

#include <gmp.h>

#include "fpdefs.h"

#define DEFAULT_RENDER_METHOD RM_SUCCESSIVE_REFINE
#define INT_LIMBS 1
#define MP_THRESHOLD 53
#define SR_CHUNK_SIZE 32

typedef enum fractal_type_enum {
	FRACTAL_MANDELBROT = 0,
	FRACTAL_JULIA = 1,
	FRACTAL_MAX = 2
} fractal_type_t;

typedef enum render_method_enum {
	RM_SUCCESSIVE_REFINE = 0,
	RM_MARIANI_SILVER = 1,
	RM_BOUNDARY_TRACE = 2,
	RM_MAX = 3
} render_method_t;

extern const char *render_method_names[];

unsigned mandelbrot_fp (mandel_fp_t x0, mandel_fp_t y0, unsigned maxiter);


struct mandeldata {
	bool configured;
	fractal_type_t type;
	unsigned zpower;
	unsigned *ptriangle;
	mpf_t cx, cy, magf;
	mpf_t xmin_f, xmax_f, ymin_f, ymax_f;
	mpz_t xmin, xmax, ymin, ymax;
	unsigned frac_limbs;
	unsigned w, h, maxiter;
	double aspect;
	int *data; /* This is signed so we can represent not-yet-rendered pixels as -1 */
	render_method_t render_method;
	double log_factor;
	void *user_data;
	volatile bool terminate;
	mpf_t preal_f, pimag_f;
	mp_limb_t *preal, *pimag;
	bool preal_sign, pimag_sign;
	mandel_fp_t preal_float, pimag_float;
	unsigned thread_count;
	void (*display_pixel) (unsigned x, unsigned y, unsigned i, void *user_data);
	void (*display_rect) (unsigned x, unsigned y, unsigned w, unsigned h, unsigned i, void *user_data);
};


void mandel_convert_x (struct mandeldata *mandel, mpz_t rop, unsigned op);
void mandel_convert_y (struct mandeldata *mandel, mpz_t rop, unsigned op);
void mandel_convert_x_f (struct mandeldata *mandel, mpf_t rop, unsigned op);
void mandel_convert_y_f (struct mandeldata *mandel, mpf_t rop, unsigned op);

void mandel_set_pixel (struct mandeldata *mandel, int x, int y, unsigned iter);
void mandel_put_pixel (struct mandeldata *mandel, unsigned x, unsigned y, unsigned iter);

int mandel_get_pixel (const struct mandeldata *mandel, int x, int y);
bool mandel_all_neighbors_same (struct mandeldata *mandel, unsigned x, unsigned y, unsigned d);
void my_mpn_mul_fast (mp_limb_t *p, mp_limb_t *f0, mp_limb_t *f1, unsigned frac_limbs);
bool my_mpn_add_signed (mp_limb_t *rop, mp_limb_t *op1, bool op1_sign, mp_limb_t *op2, bool op2_sign, unsigned frac_limbs);

unsigned mandel_julia (const struct mandeldata *md, mp_limb_t *x0, bool x0_sign, mp_limb_t *y0, bool y0_sign, mp_limb_t *preal, bool preal_sign, mp_limb_t *pimag, bool pimag_sign, unsigned maxiter, unsigned frac_limbs);
#ifdef MANDELBROT_FP_ASM
unsigned mandelbrot_fp (mandel_fp_t x0, mandel_fp_t y0, unsigned maxiter);
#endif
unsigned mandel_julia_fp (const struct mandeldata *md, mandel_fp_t x0, mandel_fp_t y0, mandel_fp_t preal, mandel_fp_t pimag, unsigned maxiter);
int mandel_render_pixel (struct mandeldata *mandel, int x, int y);
void calcpart (struct mandeldata *md, int x0, int y0, int x1, int y1);
void mandel_put_rect (struct mandeldata *mandel, int x, int y, int w, int h, unsigned iter);
void mandel_display_rect (struct mandeldata *mandel, int x, int y, int w, int h, unsigned iter);
void mandel_render (struct mandeldata *mandel);
void mandeldata_init (struct mandeldata *mandel);
void mandeldata_clear (struct mandeldata *mandel);
void mandeldata_configure (struct mandeldata *mandel);
unsigned mandeldata_get_precision (const struct mandeldata *mandel);

extern unsigned iter_saved;

#endif /* _MANDEL_MANDELBROT_H */
