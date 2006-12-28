#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <gmp.h>

#include "cmdline.h"
#include "mandelbrot.h"
#include "util.h"


struct sr_state {
	struct mandeldata *md;
	int y, chunk_size;
	GStaticMutex mutex;
};


struct ms_state {
	struct mandeldata *md;
	GMutex *mutex;
	GQueue *queue;
	GCond *cond;
	int idle_threads;
};

struct ms_q_entry {
	int x0, y0, x1, y1;
};


static void calc_sr_row (struct mandeldata *mandel, int y, int chunk_size);
static void calc_sr_mt_pass (struct mandeldata *mandel, int chunk_size);
static gpointer sr_mt_thread_func (gpointer data);
static void calc_ms_mt (struct mandeldata *mandel);
static gpointer ms_mt_thread_func (gpointer data);
static void ms_queue_push (struct ms_state *state, int x0, int y0, int x1, int y1);
static void ms_do_work (struct mandeldata *md, int x0, int y0, int x1, int y1, void (*enqueue) (int, int, int, int, void *), void *data);
static void ms_enqueue (int x0, int y0, int x1, int y1, void *data);
static void ms_mt_enqueue (int x0, int y0, int x1, int y1, void *data);


const char *render_method_names[] = {
	"Successive Refinement",
	"Mariani-Silver"
};


void
mandel_convert_x (struct mandeldata *mandel, mpz_t rop, unsigned op)
{
	mpz_sub (rop, mandel->xmax, mandel->xmin);
	mpz_mul_ui (rop, rop, op);
	mpz_div_ui (rop, rop, mandel->w);
	mpz_add (rop, rop, mandel->xmin);
}


void
mandel_convert_y (struct mandeldata *mandel, mpz_t rop, unsigned op)
{
	mpz_sub (rop, mandel->ymin, mandel->ymax);
	mpz_mul_ui (rop, rop, op);
	mpz_div_ui (rop, rop, mandel->h);
	mpz_add (rop, rop, mandel->ymax);
}


void
mandel_convert_x_f (struct mandeldata *mandel, mpf_t rop, unsigned op)
{
	mpf_sub (rop, mandel->xmax_f, mandel->xmin_f);
	mpf_mul_ui (rop, rop, op);
	mpf_div_ui (rop, rop, mandel->w);
	mpf_add (rop, rop, mandel->xmin_f);
}


void
mandel_convert_y_f (struct mandeldata *mandel, mpf_t rop, unsigned op)
{
	mpf_sub (rop, mandel->ymin_f, mandel->ymax_f);
	mpf_mul_ui (rop, rop, op);
	mpf_div_ui (rop, rop, mandel->h);
	mpf_add (rop, rop, mandel->ymax_f);
}

void
mandel_set_pixel (struct mandeldata *mandel, int x, int y, unsigned iter)
{
	mandel->data[x * mandel->h + y] = iter;
}


void
mandel_put_pixel (struct mandeldata *mandel, unsigned x, unsigned y, unsigned iter)
{
	mandel_set_pixel (mandel, x, y, iter);
	if (mandel->display_pixel != NULL)
		mandel->display_pixel (x, y, iter, mandel->user_data);
}


unsigned
mandel_get_pixel (const struct mandeldata *mandel, int x, int y)
{
	return mandel->data[x * mandel->h + y];
}

bool
mandel_all_neighbors_same (struct mandeldata *mandel, unsigned x, unsigned y, unsigned d)
{
	int px = mandel_get_pixel (mandel, x, y);
	return x >= d && y >= d && x < mandel->w - d && y < mandel->h - d
		&& mandel_get_pixel (mandel, x - d, y - d) == px
		&& mandel_get_pixel (mandel, x - d, y    ) == px
		&& mandel_get_pixel (mandel, x - d, y + d) == px
		&& mandel_get_pixel (mandel, x    , y - d) == px
		&& mandel_get_pixel (mandel, x    , y + d) == px
		&& mandel_get_pixel (mandel, x + d, y - d) == px
		&& mandel_get_pixel (mandel, x + d, y    ) == px
		&& mandel_get_pixel (mandel, x + d, y + d) == px;
}


void
my_mpn_mul_fast (mp_limb_t *p, mp_limb_t *f0, mp_limb_t *f1, unsigned frac_limbs)
{
	unsigned total_limbs = INT_LIMBS + frac_limbs;
	mp_limb_t tmp[total_limbs * 2];
	int i;
	mpn_mul_n (tmp, f0, f1, total_limbs);
	for (i = 0; i < total_limbs; i++)
		p[i] = tmp[frac_limbs + i];
}

bool
my_mpn_add_signed (mp_limb_t *rop, mp_limb_t *op1, bool op1_sign, mp_limb_t *op2, bool op2_sign, unsigned frac_limbs)
{
	unsigned total_limbs = INT_LIMBS + frac_limbs;
	if (op1_sign == op2_sign) {
		mpn_add_n (rop, op1, op2, total_limbs);
		return op1_sign;
	} else {
		if (mpn_cmp (op1, op2, total_limbs) > 0) {
			mpn_sub_n (rop, op1, op2, total_limbs);
			return op1_sign;
		} else {
			mpn_sub_n (rop, op2, op1, total_limbs);
			return op2_sign;
		}
	}
}


unsigned iter_saved = 0;


unsigned
mandel_julia (mp_limb_t *x0, bool x0_sign, mp_limb_t *y0, bool y0_sign, mp_limb_t *preal, bool preal_sign, mp_limb_t *pimag, bool pimag_sign, unsigned maxiter, unsigned frac_limbs)
{
	unsigned total_limbs = INT_LIMBS + frac_limbs;
	mp_limb_t x[total_limbs], y[total_limbs], xsqr[total_limbs], ysqr[total_limbs], sqrsum[total_limbs], four[total_limbs];
	mp_limb_t cd_x[total_limbs], cd_y[total_limbs];
	unsigned i;

	for (i = 0; i < total_limbs; i++)
		four[i] = 0;
	four[frac_limbs] = 4;

	memcpy (x, x0, sizeof (x));
	memcpy (cd_x, x0, sizeof (cd_x));
	memcpy (y, y0, sizeof (y));
	memcpy (cd_y, y0, sizeof (cd_y));

	bool x_sign = x0_sign, y_sign = y0_sign;

	int k = 1, m = 1;
	i = 0;
	my_mpn_mul_fast (xsqr, x, x, frac_limbs);
	my_mpn_mul_fast (ysqr, y, y, frac_limbs);
	mpn_add_n (sqrsum, xsqr, ysqr, total_limbs);
	while (i < maxiter && mpn_cmp (sqrsum, four, total_limbs) < 0) {
		mp_limb_t tmp1[total_limbs];
		my_mpn_mul_fast (tmp1, x, y, frac_limbs);
		mpn_lshift (y, tmp1, total_limbs, 1);
		y_sign = my_mpn_add_signed (y, y, x_sign != y_sign, pimag, pimag_sign, frac_limbs);

		if (mpn_cmp (xsqr, ysqr, total_limbs) > 0) {
			mpn_sub_n (x, xsqr, ysqr, total_limbs);
			x_sign = false;
		} else {
			mpn_sub_n (x, ysqr, xsqr, total_limbs);
			x_sign = true;
		}
		x_sign = my_mpn_add_signed (x, x, x_sign, preal, preal_sign, frac_limbs);

		k--;
		/* FIXME we must compare the signs here! */
		if (mpn_cmp (x, cd_x, total_limbs) == 0 && mpn_cmp (y, cd_y, total_limbs) == 0) {
			//printf ("* Cycle of length %d detected after %u iterations.\n", m - k + 1, i);
			iter_saved += maxiter - i;
			i = maxiter;
			break;
		}
		if (k == 0) {
			k = m <<= 1;
			memcpy (cd_x, x, sizeof (x));
			memcpy (cd_y, y, sizeof (y));
		}

		my_mpn_mul_fast (xsqr, x, x, frac_limbs);
		my_mpn_mul_fast (ysqr, y, y, frac_limbs);
		mpn_add_n (sqrsum, xsqr, ysqr, total_limbs);

		i++;
	}
	return i;
}


unsigned
mandel_julia_fp (mandel_fp_t x0, mandel_fp_t y0, mandel_fp_t preal, mandel_fp_t pimag, unsigned maxiter)
{
	unsigned i = 0, k = 1, m = 1;
	mandel_fp_t x = x0, y = y0, cd_x = x, cd_y = y;
	while (i < maxiter && x * x + y * y < 4.0) {
		mandel_fp_t xold = x, yold = y;
		x = x * x - y * y + preal;
		y = 2 * xold * yold + pimag;

		k--;
		if (x == cd_x && y == cd_y) {
			iter_saved += maxiter - i;
			i = maxiter;
			break;
		}

		if (k == 0) {
			k = m <<= 1;
			cd_x = x;
			cd_y = y;
		}

		i++;
	}
	return i;
}


void
mandel_render_pixel (struct mandeldata *mandel, int x, int y)
{
	unsigned i = 0;
	if (mandel->frac_limbs == 0) {
		// FP
		mandel_fp_t xmin = mpf_get_mandel_fp (mandel->xmin_f);
		mandel_fp_t xmax = mpf_get_mandel_fp (mandel->xmax_f);
		mandel_fp_t ymin = mpf_get_mandel_fp (mandel->ymin_f);
		mandel_fp_t ymax = mpf_get_mandel_fp (mandel->ymax_f);
		mandel_fp_t xf = x * (xmax - xmin) / mandel->w + xmin;
		mandel_fp_t yf = y * (ymin - ymax) / mandel->h + ymax;
		switch (mandel->type) {
			case FRACTAL_MANDELBROT: {
#ifdef MANDELBROT_FP_ASM
				i = mandelbrot_fp (xf, yf, mandel->maxiter);
#else
				i = mandel_julia_fp (xf, yf, xf, yf, mandel->maxiter);
#endif
				break;
			}
			case FRACTAL_JULIA: {
				i = mandel_julia_fp (xf, yf, mandel->preal_float, mandel->pimag_float, mandel->maxiter);
				break;
			}
			default: {
				fprintf (stderr, "* BUG: Unknown fractal type %d\n", mandel->type);
				return;
			}
		}
	} else {
		// MP
		mpz_t z;
		int j;
		unsigned total_limbs = INT_LIMBS + mandel->frac_limbs;
		mp_limb_t x0[total_limbs], y0[total_limbs];
		bool x0_sign, y0_sign;
		mpz_init (z);

		mandel_convert_x (mandel, z, x);
		for (j = 0; j < total_limbs; j++)
			x0[j] = mpz_getlimbn (z, j);
		x0_sign = mpz_sgn (z) < 0;

		mandel_convert_y (mandel, z, y);
		for (j = 0; j < total_limbs; j++)
			y0[j] = mpz_getlimbn (z, j);
		y0_sign = mpz_sgn (z) < 0;

		mpz_clear (z);
		switch (mandel->type) {
			case FRACTAL_MANDELBROT: {
				i = mandel_julia (x0, x0_sign, y0, y0_sign, x0, x0_sign, y0, y0_sign, mandel->maxiter, mandel->frac_limbs);
				break;
			}
			case FRACTAL_JULIA: {
				i = mandel_julia (x0, x0_sign, y0, y0_sign, mandel->preal, mandel->preal_sign, mandel->pimag, mandel->pimag_sign, mandel->maxiter, mandel->frac_limbs);
				break;
			}
			default: {
				fprintf (stderr, "* BUG: Unknown fractal type %d\n", mandel->type);
				return;
			}
		}
	}
	if (mandel->log_factor != 0.0)
		i = mandel->log_factor * log (i);
	mandel_put_pixel (mandel, x, y, i);
}



void
mandel_put_rect (struct mandeldata *mandel, int x, int y, int w, int h, unsigned iter)
{
	int xc, yc;
	for (xc = x; xc < x + w; xc++)
		for (yc = y; yc < y + h; yc++)
			mandel_set_pixel (mandel, xc, yc, iter);
	if (mandel->display_rect != NULL)
		mandel->display_rect (x, y, w, h, iter, mandel->user_data);
	else if (mandel->display_pixel != NULL)
		mandel->display_pixel (x, y, iter, mandel->user_data);
}


void
mandel_init_coords (struct mandeldata *mandel)
{
	if (!mandel->allocate_done) {
		mpf_init (mandel->xmin_f);
		mpf_init (mandel->xmax_f);
		mpf_init (mandel->ymin_f);
		mpf_init (mandel->ymax_f);
		mpz_init (mandel->xmin);
		mpz_init (mandel->xmax);
		mpz_init (mandel->ymin);
		mpz_init (mandel->ymax);
		mandel->allocate_done = TRUE;
	}

	mandel->aspect = (double) mandel->w / mandel->h;
	center_to_corners (mandel->xmin_f, mandel->xmax_f, mandel->ymin_f, mandel->ymax_f, mandel->cx, mandel->cy, mandel->magf, mandel->aspect);

	// Determine the required precision.
	mpf_t dx;
	mpf_init (dx);

	mpf_sub (dx, mandel->xmax_f, mandel->xmin_f);
	mpf_div_ui (dx, dx, mandel->w);

	long exponent;
	mpf_get_d_2exp (&exponent, dx);

	mpf_clear (dx);

	if (exponent > 0)
		exponent = 0;

	// We add a minimum of 2 extra bits of precision, that should do.
	int required_bits = 2 - exponent;

	if (required_bits < MP_THRESHOLD)
		mandel->frac_limbs = 0;
	else
		mandel->frac_limbs = required_bits / mp_bits_per_limb + INT_LIMBS;

	unsigned frac_limbs = mandel->frac_limbs;
	unsigned total_limbs = INT_LIMBS + frac_limbs;

	// Convert coordinates to integer values.
	mpf_t f;
	mpf_init2 (f, total_limbs * mp_bits_per_limb);

	mpf_mul_2exp (f, mandel->xmin_f, frac_limbs * mp_bits_per_limb);
	mpz_set_f (mandel->xmin, f);

	mpf_mul_2exp (f, mandel->xmax_f, frac_limbs * mp_bits_per_limb);
	mpz_set_f (mandel->xmax, f);

	mpf_mul_2exp (f, mandel->ymin_f, frac_limbs * mp_bits_per_limb);
	mpz_set_f (mandel->ymin, f);

	mpf_mul_2exp (f, mandel->ymax_f, frac_limbs * mp_bits_per_limb);
	mpz_set_f (mandel->ymax, f);

	if (mandel->type == FRACTAL_JULIA) {
		if (frac_limbs > 0) {
			mandel->preal = malloc (total_limbs * sizeof (mp_limb_t));
			mandel->pimag = malloc (total_limbs * sizeof (mp_limb_t));
			mpz_t z;
			mpz_init (z);
			mpf_mul_2exp (f, mandel->preal_f, frac_limbs * mp_bits_per_limb);
			mpz_set_f (z, f);
			int i;
			for (i = 0; i < total_limbs; i++)
				mandel->preal[i] = mpz_getlimbn (z, i);
			mpf_mul_2exp (f, mandel->pimag_f, frac_limbs * mp_bits_per_limb);
			mpz_set_f (z, f);
			for (i = 0; i < total_limbs; i++)
				mandel->pimag[i] = mpz_getlimbn (z, i);
			mpz_clear (z);
		} else {
			mandel->preal_float = mpf_get_mandel_fp (mandel->preal_f);
			mandel->pimag_float = mpf_get_mandel_fp (mandel->pimag_f);
		}
	}

	mpf_clear (f);
}


void
mandel_render (struct mandeldata *mandel)
{
	switch (mandel->render_method) {
		case RM_MARIANI_SILVER: {
			int x, y;

			for (x = 0; !mandel->terminate && x < mandel->w; x++) {
				mandel_render_pixel (mandel, x, 0);
				mandel_render_pixel (mandel, x, mandel->h - 1);
			}

			for (y = 1; !mandel->terminate && y < mandel->h - 1; y++) {
				mandel_render_pixel (mandel, 0, y);
				mandel_render_pixel (mandel, mandel->w - 1, y);
			}

			if (mandel->terminate)
				break;

			if (thread_count > 1)
				calc_ms_mt (mandel);
			else
				calcpart (mandel, 0, 0, mandel->w - 1, mandel->h - 1);

			break;
		}

		case RM_SUCCESSIVE_REFINE: {
			unsigned y, chunk_size = SR_CHUNK_SIZE;

			while (!mandel->terminate && chunk_size != 0) {
				if (thread_count > 1)
					calc_sr_mt_pass (mandel, chunk_size);
				else
					for (y = 0; !mandel->terminate && y < mandel->h; y += chunk_size)
						calc_sr_row (mandel, y, chunk_size);
				chunk_size >>= 1;
			}

			break;
		}

		default: {
			fprintf (stderr, "* BUG: invalid case value at %s:%d\n", __FILE__, __LINE__);
			break;
		}
	}
}


void
calcpart (struct mandeldata *md, int x0, int y0, int x1, int y1)
{
	if (md->terminate)
		return;
	ms_do_work (md, x0, y0, x1, y1, ms_enqueue, md);
}


void
mandel_free (struct mandeldata *mandel)
{
	free (mandel->data);

	mpf_clear (mandel->xmin_f);
	mpf_clear (mandel->xmax_f);
	mpf_clear (mandel->ymin_f);
	mpf_clear (mandel->ymax_f);

	mpz_clear (mandel->xmin);
	mpz_clear (mandel->xmax);
	mpz_clear (mandel->ymin);
	mpz_clear (mandel->ymax);
}


static void
calc_sr_row (struct mandeldata *mandel, int y, int chunk_size)
{
	int x;

	for (x = 0; x < mandel->w && !mandel->terminate; x += chunk_size) {
		unsigned parent_x, parent_y;
		bool do_eval;
		if (x % (2 * chunk_size) == 0)
			parent_x = x;
		else
			parent_x = x - chunk_size;
		if (y % (2 * chunk_size) == 0)
			parent_y = y;
		else
			parent_y = y - chunk_size;

		if (chunk_size == SR_CHUNK_SIZE) // 1st pass
			do_eval = true;
		else if (parent_x == x && parent_y == y)
			do_eval = false;
		else if (mandel_all_neighbors_same (mandel, parent_x, parent_y, chunk_size << 1))
			do_eval = false;
		else
			do_eval = true;

		if (do_eval) {
			mandel_render_pixel (mandel, x, y);
			mandel_put_rect (mandel, x, y, MIN (chunk_size, mandel->w - x), MIN (chunk_size, mandel->h - y), mandel_get_pixel (mandel, x, y));
		} else {
			mandel_put_pixel (mandel, x, y, mandel_get_pixel (mandel, parent_x, parent_y));
		}
	}
}


static void
calc_sr_mt_pass (struct mandeldata *mandel, int chunk_size)
{
	struct sr_state state = {mandel, 0, chunk_size, G_STATIC_MUTEX_INIT};
	GThread *threads[thread_count];
	int i;

	for (i = 0; i < thread_count; i++)
		threads[i] = g_thread_create (sr_mt_thread_func, &state, TRUE, NULL);
	for (i = 0; i < thread_count; i++)
		g_thread_join (threads[i]);
}


static gpointer
sr_mt_thread_func (gpointer data)
{
	struct sr_state *state = (struct sr_state *) data;
	while (!state->md->terminate) {
		int y;
		g_static_mutex_lock (&state->mutex);
		y = state->y;
		state->y += state->chunk_size;
		g_static_mutex_unlock (&state->mutex);
		if (y >= state->md->h)
			break; /* done */
		calc_sr_row (state->md, y, state->chunk_size);
	}
	return NULL;
}


static void
calc_ms_mt (struct mandeldata *mandel)
{
	struct ms_state state = {mandel, g_mutex_new (), g_queue_new (), g_cond_new (), 0};
	GThread *threads[thread_count];
	int i;

	ms_queue_push (&state, 0, 0, mandel->w - 1, mandel->h - 1);

	for (i = 0; i < thread_count; i++)
		threads[i] = g_thread_create (ms_mt_thread_func, &state, TRUE, NULL);

	for (i = 0; i < thread_count; i++)
		g_thread_join (threads[i]);

	g_mutex_free (state.mutex);
	g_queue_free (state.queue);
	g_cond_free (state.cond);
}


static gpointer
ms_mt_thread_func (gpointer data)
{
	struct ms_state *state = (struct ms_state *) data;
	struct mandeldata *md = state->md;
	while (!md->terminate) {
		g_mutex_lock (state->mutex);
		state->idle_threads++;
		/* Notify all waiting threads about the increase of idle_threads */
		g_cond_broadcast (state->cond);
		while (g_queue_is_empty (state->queue)) {
			if (state->idle_threads == thread_count) {
				/* Queue is empty, all threads idle. We're done. */
				g_mutex_unlock (state->mutex);
				return NULL;
			}
			g_cond_wait (state->cond, state->mutex);
		}
		state->idle_threads--;
		struct ms_q_entry *entry = g_queue_pop_head (state->queue);
		g_mutex_unlock (state->mutex);

		int x0 = entry->x0, y0 = entry->y0, x1 = entry->x1, y1 = entry->y1;
		free (entry);

		ms_do_work (md, x0, y0, x1, y1, ms_mt_enqueue, state);
	}
	return NULL;
}


static void
ms_queue_push (struct ms_state *state, int x0, int y0, int x1, int y1)
{
	struct ms_q_entry *new_job = malloc (sizeof (struct ms_q_entry));
	new_job->x0 = x0;
	new_job->y0 = y0;
	new_job->x1 = x1;
	new_job->y1 = y1;
	g_mutex_lock (state->mutex);
	g_queue_push_tail (state->queue, new_job);
	g_cond_signal (state->cond);
	g_mutex_unlock (state->mutex);
}


static void
ms_do_work (struct mandeldata *md, int x0, int y0, int x1, int y1, void (*enqueue) (int, int, int, int, void *), void *data)
{
	int x, y;
	bool failed = false;
	unsigned p0 = mandel_get_pixel (md, x0, y0);

	for (x = x0; !failed && x <= x1; x++)
		failed = mandel_get_pixel (md, x, y0) != p0 || mandel_get_pixel (md, x, y1) != p0;

	for (y = y0; !failed && y <= y1; y++)
		failed = mandel_get_pixel (md, x0, y) != p0 || mandel_get_pixel (md, x1, y) != p0;

	if (failed) {
		if (x1 - x0 > y1 - y0) {
			unsigned xm = (x0 + x1) / 2;
			for (y = y0 + 1; y < y1; y++)
				mandel_render_pixel (md, xm, y);

			if (xm - x0 > 1)
				enqueue (x0, y0, xm, y1, data);
			if (x1 - xm > 1)
				enqueue (xm, y0, x1, y1, data);
		} else {
			unsigned ym = (y0 + y1) / 2;
			for (x = x0 + 1; x < x1; x++)
				mandel_render_pixel (md, x, ym);

			if (ym - y0 > 1)
				enqueue (x0, y0, x1, ym, data);
			if (y1 - ym > 1)
				enqueue (x0, ym, x1, y1, data);
		}
	} else {
		mandel_put_rect (md, x0 + 1, y0 + 1, x1 - x0 - 1, y1 - y0 - 1, p0);
	}
}


static void
ms_enqueue (int x0, int y0, int x1, int y1, void *data)
{
	struct mandeldata *md = (struct mandeldata *) data;
	calcpart (md, x0, y0, x1, y1);
}


static void
ms_mt_enqueue (int x0, int y0, int x1, int y1, void *data)
{
	struct ms_state *state = (struct ms_state *) data;
	ms_queue_push (state, x0, y0, x1, y1);
}


unsigned
get_precision (const struct mandeldata *mandel)
{
	if (mandel->frac_limbs == 0)
		return 0;
	else
		return (mandel->frac_limbs + INT_LIMBS) * mp_bits_per_limb;
}
