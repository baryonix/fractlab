#ifndef _MANDEL_MANDELBROT_H
#define _MANDEL_MANDELBROT_H

#include <stdbool.h>
#include <glib.h>

#include <gmp.h>
#include <mpfr.h>

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

typedef enum fractal_type_flags_enum {
	FRAC_TYPE_ESCAPE_ITER = 1 << 0,
	FRAC_TYPE_DISTANCE = 1 << 1
} fractal_type_flags_t;

typedef enum render_method_enum {
	RM_SUCCESSIVE_REFINE = 0,
	RM_MARIANI_SILVER = 1,
	RM_BOUNDARY_TRACE = 2,
	RM_MAX = 3
} render_method_t;

typedef enum fractal_repres_enum {
	REPRES_ESCAPE = 0,
	REPRES_ESCAPE_LOG = 1,
	REPRES_DISTANCE = 2,
	REPRES_MAX = 3
} fractal_repres_t;


struct fractal_type;
struct mandel_point;
struct mandel_area;
struct mandeldata;
struct mandel_renderer;
struct mandelbrot_param;
struct mandelbrot_state;
struct mandel_representation;


struct fractal_type {
	fractal_type_t type;
	const char *name;
	const char *descr;
	fractal_type_flags_t flags;
	void *(*param_new) (void);
	void *(*param_clone) (const void *orig);
	void (*param_free) (void *param);
	void *(*state_new) (const void *param, unsigned frac_limbs);
	void (*state_free) (void *state);
	unsigned (*compute) (void *state, mpf_srcptr real, mpf_srcptr imag, mpfr_ptr distance);
	unsigned (*compute_fp) (void *state, mandel_fp_t real, mandel_fp_t imag, mandel_fp_t *distance);
	void (*set_defaults) (struct mandeldata *md);
};

struct mandel_point {
	mpf_t real, imag;
};

struct mandel_area {
	struct mandel_point center;
	mpf_t magf;
};

struct mandel_julia_param {
	unsigned zpower;
	unsigned maxiter;
};

struct mandelbrot_param {
	struct mandel_julia_param mjparam;
};

struct julia_param {
	struct mandel_julia_param mjparam;
	struct mandel_point param;
};

struct mandel_julia_state {
	unsigned frac_limbs;
	unsigned *ptriangle;
};

struct mandelbrot_state {
	struct mandel_julia_state mjstate;
	const struct mandelbrot_param *param;
};

struct julia_state {
	struct mandel_julia_state mjstate;
	const struct julia_param *param;
	union {
		struct {
			mandel_fp_t preal_float, pimag_float;
		} fp;
	} mpvars;
};


struct mandel_repres {
	fractal_repres_t repres;
	union {
		double log_base; /* for escape-iter logarithmic */
	} params;
};


struct mandeldata {
	const struct fractal_type *type;
	void *type_param;
	struct mandel_area area;
	struct mandel_repres repres;
};


struct mandel_renderer {
	const struct mandeldata *md;
	unsigned w, h;
	volatile gint pixels_done;
	mpf_t xmin_f, xmax_f, ymin_f, ymax_f;
	unsigned frac_limbs;
	double aspect;
	int *data; /* This is signed so we can represent not-yet-rendered pixels as -1 */
	render_method_t render_method;
	void *user_data;
	void *fractal_state;
	volatile bool terminate;
	unsigned thread_count;
	union {
		double log_factor;
		mpfr_t distance_est_k;
	} rep_state;
	void (*display_pixel) (unsigned x, unsigned y, unsigned i, void *user_data);
	void (*display_rect) (unsigned x, unsigned y, unsigned w, unsigned h, unsigned i, void *user_data);
};


extern const char *const render_method_names[];
extern const struct fractal_type fractal_types[];

const struct fractal_type *fractal_type_by_id (fractal_type_t type);
const struct fractal_type *fractal_type_by_name (const char *name);
int fractal_supported_representations (const struct fractal_type *type, fractal_repres_t *res);

/* This may be an external assembly routine. */
unsigned mandelbrot_fp (mandel_fp_t x0, mandel_fp_t y0, unsigned maxiter);

void mandel_convert_x_f (const struct mandel_renderer *mandel, mpf_ptr rop, unsigned op);
void mandel_convert_y_f (const struct mandel_renderer *mandel, mpf_ptr rop, unsigned op);

void mandel_set_pixel (struct mandel_renderer *mandel, int x, int y, unsigned iter);
void mandel_put_pixel (struct mandel_renderer *mandel, unsigned x, unsigned y, unsigned iter);

int mandel_get_pixel (const struct mandel_renderer *mandel, int x, int y);
bool mandel_all_neighbors_same (const struct mandel_renderer *mandel, unsigned x, unsigned y, unsigned d);
void my_mpn_mul_fast (mp_ptr p, mp_srcptr f0, mp_srcptr f1, unsigned frac_limbs);
bool my_mpn_add_signed (mp_ptr rop, mp_srcptr op1, bool op1_sign, mp_srcptr op2, bool op2_sign, unsigned frac_limbs);
void my_mpn_invert (mp_ptr op, unsigned total_limbs);

unsigned mandel_julia (struct mandel_julia_state *state, const struct mandel_julia_param *param, mpf_srcptr x0f, mpf_srcptr y0f, mpf_srcptr prealf, mpf_srcptr pimagf, mpfr_ptr distance);
#ifdef MANDELBROT_FP_ASM
unsigned mandelbrot_fp (mandel_fp_t x0, mandel_fp_t y0, unsigned maxiter);
#endif
unsigned mandel_julia_fp (struct mandel_julia_state *state, const struct mandel_julia_param *param, mandel_fp_t x0, mandel_fp_t y0, mandel_fp_t preal, mandel_fp_t pimag, mandel_fp_t *distance);
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

void mandeldata_init (struct mandeldata *md, const struct fractal_type *type);
void mandeldata_clear (struct mandeldata *md);
void mandeldata_set_defaults (struct mandeldata *md);
void mandeldata_clone (struct mandeldata *clone, const struct mandeldata *orig);

void mandel_point_init (struct mandel_point *point);
void mandel_point_clear (struct mandel_point *point);

void mandel_area_init (struct mandel_area *area);
void mandel_area_clear (struct mandel_area *area);

#endif /* _MANDEL_MANDELBROT_H */
