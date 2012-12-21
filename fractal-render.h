#ifndef _MANDEL_MANDELBROT_H
#define _MANDEL_MANDELBROT_H

#include <stdbool.h>
#include <stdint.h>
#include <glib.h>

#include <gmp.h>
#include <mpfr.h>

#include "fpdefs.h"
#include "fractal-math.h"

#define DEFAULT_RENDER_METHOD RM_SUCCESSIVE_REFINE
#define MP_THRESHOLD 53
#define SR_CHUNK_SIZE 32

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


struct color {
	uint16_t r, g, b;
};


struct mandeldata;
struct mandel_renderer;
struct mandel_representation;


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
	struct color *palette;
	unsigned palette_size;
	void (*display_pixel) (unsigned x, unsigned y, unsigned i, void *user_data);
	void (*display_rect) (unsigned x, unsigned y, unsigned w, unsigned h, unsigned i, void *user_data);
};


extern const char *const render_method_names[];

int fractal_supported_representations (const struct fractal_type *type, fractal_repres_t *res);

void mandel_convert_x_f (const struct mandel_renderer *mandel, mpf_ptr rop, unsigned op);
void mandel_convert_y_f (const struct mandel_renderer *mandel, mpf_ptr rop, unsigned op);

void mandel_set_pixel (struct mandel_renderer *mandel, int x, int y, unsigned iter);
void mandel_put_pixel (struct mandel_renderer *mandel, unsigned x, unsigned y, unsigned iter);

int mandel_get_pixel (const struct mandel_renderer *mandel, int x, int y);

int mandel_render_pixel (struct mandel_renderer *mandel, int x, int y);
int mandel_pixel_value (const struct mandel_renderer *mandel, int x, int y);
void mandel_put_rect (struct mandel_renderer *mandel, int x, int y, int w, int h, unsigned iter);
void mandel_display_rect (struct mandel_renderer *mandel, int x, int y, int w, int h, unsigned iter);
void mandel_render (struct mandel_renderer *mandel);
void mandel_renderer_init (struct mandel_renderer *renderer, const struct mandeldata *md, unsigned w, unsigned h);
struct color *mandel_create_default_palette (unsigned size);
void mandel_renderer_clear (struct mandel_renderer *renderer);
unsigned mandel_get_precision (const struct mandel_renderer *mandel);
double mandel_renderer_progress (const struct mandel_renderer *renderer);

void mandeldata_init (struct mandeldata *md, const struct fractal_type *type);
void mandeldata_clear (struct mandeldata *md);
void mandeldata_set_defaults (struct mandeldata *md);
void mandeldata_clone (struct mandeldata *clone, const struct mandeldata *orig);

#endif /* _MANDEL_MANDELBROT_H */
