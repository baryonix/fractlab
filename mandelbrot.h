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


struct mandel_point {
	mpf_t real, imag;
};

struct mandel_area {
	struct mandel_point center;
	mpf_t magf;
};

struct mandeldata {
	fractal_type_t type;
	unsigned zpower;
	struct mandel_area area;
	unsigned maxiter;
	double log_factor;
	struct mandel_point param; /* parameter of Julia set */
	bool distance_est; /* Currently only works for Mandelbrot set! */
};


struct mandel_renderer {
	const struct mandeldata *md;
	unsigned w, h;
	volatile gint pixels_done;
	unsigned *ptriangle;
	mpf_t xmin_f, xmax_f, ymin_f, ymax_f;
	unsigned frac_limbs;
	double aspect;
	int *data; /* This is signed so we can represent not-yet-rendered pixels as -1 */
	render_method_t render_method;
	void *user_data;
	volatile bool terminate;
	mandel_fp_t preal_float, pimag_float;
	unsigned thread_count;
	void (*display_pixel) (unsigned x, unsigned y, unsigned i, void *user_data);
	void (*display_rect) (unsigned x, unsigned y, unsigned w, unsigned h, unsigned i, void *user_data);
};


void mandel_convert_x_f (const struct mandel_renderer *mandel, mpf_t rop, unsigned op);
void mandel_convert_y_f (const struct mandel_renderer *mandel, mpf_t rop, unsigned op);

void mandel_set_pixel (struct mandel_renderer *mandel, int x, int y, unsigned iter);
void mandel_put_pixel (struct mandel_renderer *mandel, unsigned x, unsigned y, unsigned iter);

int mandel_get_pixel (const struct mandel_renderer *mandel, int x, int y);
bool mandel_all_neighbors_same (const struct mandel_renderer *mandel, unsigned x, unsigned y, unsigned d);
void my_mpn_mul_fast (mp_limb_t *p, mp_limb_t *f0, mp_limb_t *f1, unsigned frac_limbs);
bool my_mpn_add_signed (mp_limb_t *rop, mp_limb_t *op1, bool op1_sign, mp_limb_t *op2, bool op2_sign, unsigned frac_limbs);
void my_mpn_invert (mp_limb_t *op, unsigned total_limbs);

unsigned mandel_julia (const struct mandel_renderer *md, const mpf_t x0f, const mpf_t y0f, const mpf_t prealf, const mpf_t pimagf, unsigned maxiter, unsigned frac_limbs);
#ifdef MANDELBROT_FP_ASM
unsigned mandelbrot_fp (mandel_fp_t x0, mandel_fp_t y0, unsigned maxiter);
#endif
unsigned mandel_julia_fp (const struct mandel_renderer *md, mandel_fp_t x0, mandel_fp_t y0, mandel_fp_t preal, mandel_fp_t pimag, unsigned maxiter, mandel_fp_t *distance);
int mandel_render_pixel (struct mandel_renderer *mandel, int x, int y);
int mandel_pixel_value (const struct mandel_renderer *mandel, int x, int y);
void calcpart (struct mandel_renderer *md, int x0, int y0, int x1, int y1);
void mandel_put_rect (struct mandel_renderer *mandel, int x, int y, int w, int h, unsigned iter);
void mandel_display_rect (struct mandel_renderer *mandel, int x, int y, int w, int h, unsigned iter);
void mandel_render (struct mandel_renderer *mandel);
void mandel_renderer_init (struct mandel_renderer *renderer, const struct mandeldata *md, unsigned w, unsigned h);
void mandel_renderer_clear (struct mandel_renderer *renderer);
unsigned mandel_get_precision (const struct mandel_renderer *mandel);
double mandel_renderer_progress (const struct mandel_renderer *renderer);

void mandeldata_init (struct mandeldata *md);
void mandeldata_clear (struct mandeldata *md);
void mandeldata_clone (struct mandeldata *clone, const struct mandeldata *orig);

void mandel_point_init (struct mandel_point *point);
void mandel_point_clear (struct mandel_point *point);

void mandel_area_init (struct mandel_area *area);
void mandel_area_clear (struct mandel_area *area);

#endif /* _MANDEL_MANDELBROT_H */
