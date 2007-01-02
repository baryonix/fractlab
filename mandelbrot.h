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
	fractal_type_t type;
	unsigned zpower;
	mpf_t cx, cy, magf;
	unsigned maxiter;
	double log_factor;
	mpf_t preal_f, pimag_f;
};


struct mandel_renderer {
	const struct mandeldata *md;
	unsigned w, h;
	unsigned *ptriangle;
	mpf_t xmin_f, xmax_f, ymin_f, ymax_f;
	mpz_t xmin, xmax, ymin, ymax;
	unsigned frac_limbs;
	double aspect;
	int *data; /* This is signed so we can represent not-yet-rendered pixels as -1 */
	render_method_t render_method;
	void *user_data;
	volatile bool terminate;
	mp_limb_t *preal, *pimag;
	bool preal_sign, pimag_sign;
	mandel_fp_t preal_float, pimag_float;
	unsigned thread_count;
	void (*display_pixel) (unsigned x, unsigned y, unsigned i, void *user_data);
	void (*display_rect) (unsigned x, unsigned y, unsigned w, unsigned h, unsigned i, void *user_data);
};


void mandel_convert_x (struct mandel_renderer *mandel, mpz_t rop, unsigned op);
void mandel_convert_y (struct mandel_renderer *mandel, mpz_t rop, unsigned op);
void mandel_convert_x_f (struct mandel_renderer *mandel, mpf_t rop, unsigned op);
void mandel_convert_y_f (struct mandel_renderer *mandel, mpf_t rop, unsigned op);

void mandel_set_pixel (struct mandel_renderer *mandel, int x, int y, unsigned iter);
void mandel_put_pixel (struct mandel_renderer *mandel, unsigned x, unsigned y, unsigned iter);

int mandel_get_pixel (const struct mandel_renderer *mandel, int x, int y);
bool mandel_all_neighbors_same (const struct mandel_renderer *mandel, unsigned x, unsigned y, unsigned d);
void my_mpn_mul_fast (mp_limb_t *p, mp_limb_t *f0, mp_limb_t *f1, unsigned frac_limbs);
bool my_mpn_add_signed (mp_limb_t *rop, mp_limb_t *op1, bool op1_sign, mp_limb_t *op2, bool op2_sign, unsigned frac_limbs);

unsigned mandel_julia (const struct mandel_renderer *md, mp_limb_t *x0, bool x0_sign, mp_limb_t *y0, bool y0_sign, mp_limb_t *preal, bool preal_sign, mp_limb_t *pimag, bool pimag_sign, unsigned maxiter, unsigned frac_limbs);
#ifdef MANDELBROT_FP_ASM
unsigned mandelbrot_fp (mandel_fp_t x0, mandel_fp_t y0, unsigned maxiter);
#endif
unsigned mandel_julia_fp (const struct mandel_renderer *md, mandel_fp_t x0, mandel_fp_t y0, mandel_fp_t preal, mandel_fp_t pimag, unsigned maxiter);
int mandel_render_pixel (struct mandel_renderer *mandel, int x, int y);
void calcpart (struct mandel_renderer *md, int x0, int y0, int x1, int y1);
void mandel_put_rect (struct mandel_renderer *mandel, int x, int y, int w, int h, unsigned iter);
void mandel_display_rect (struct mandel_renderer *mandel, int x, int y, int w, int h, unsigned iter);
void mandel_render (struct mandel_renderer *mandel);
void mandel_renderer_init (struct mandel_renderer *renderer, const struct mandeldata *md, unsigned w, unsigned h);
void mandel_renderer_clear (struct mandel_renderer *renderer);
unsigned mandel_get_precision (const struct mandel_renderer *mandel);

void mandeldata_init (struct mandeldata *md);
void mandeldata_clear (struct mandeldata *md);
void mandeldata_clone (struct mandeldata *clone, const struct mandeldata *orig);

#endif /* _MANDEL_MANDELBROT_H */
