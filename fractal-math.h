#ifndef _GTKMANDEL_FRACTAL_MATH_H
#define _GTKMANDEL_FRACTAL_MATH_H

typedef enum fractal_type_enum {
	FRACTAL_MANDELBROT = 0,
	FRACTAL_JULIA = 1,
	FRACTAL_MAX = 2
} fractal_type_t;

typedef enum fractal_type_flags_enum {
	FRAC_TYPE_ESCAPE_ITER = 1 << 0,
	FRAC_TYPE_DISTANCE = 1 << 1
} fractal_type_flags_t;

struct mandel_point;
struct mandel_area;
struct fractal_type;
struct mandel_julia_param;
struct mandelbrot_param;
struct julia_param;

struct mandel_point {
	mpf_t real, imag;
};

struct mandel_area {
	struct mandel_point center;
	mpf_t magf;
};

struct fractal_type {
	fractal_type_t type;
	const char *name;
	const char *descr;
	fractal_type_flags_t flags;
	void *(*param_new) (void);
	void *(*param_clone) (const void *orig);
	void (*param_free) (void *param);
	void *(*state_new) (const void *param, fractal_type_flags_t flags, unsigned frac_limbs);
	void (*state_free) (void *state);
	bool (*compute) (void *state, mpf_srcptr real, mpf_srcptr imag, unsigned *iter, mpfr_ptr distance);
	bool (*compute_fp) (void *state, mandel_fp_t real, mandel_fp_t imag, unsigned *iter, mandel_fp_t *distance);
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

const struct fractal_type *fractal_type_by_id (fractal_type_t type);
const struct fractal_type *fractal_type_by_name (const char *name);

void mandel_point_init (struct mandel_point *point);
void mandel_point_clear (struct mandel_point *point);

void mandel_area_init (struct mandel_area *area);
void mandel_area_clear (struct mandel_area *area);

#endif /* _GTKMANDEL_FRACTAL_MATH_H */
