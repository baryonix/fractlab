#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include <gmp.h>
#include <mpfr.h>

#include "defs.h"
#include "fractal-render.h"
#include "util.h"
#include "misc-math.h"
#include "fractal-math.h"


struct sr_state {
	struct mandel_renderer *renderer;
	int y, chunk_size;
	GMutex *mutex;
};


struct ms_state {
	struct mandel_renderer *renderer;
	GMutex *mutex;
	GQueue *queue;
	GCond *cond;
	volatile int idle_threads;
};

struct ms_q_entry {
	int x0, y0, x1, y1;
};


struct btrace_q_entry {
	int x, y, xstep, ystep;
};


static void calc_sr_row (struct mandel_renderer *mandel, int y, int chunk_size);
static void calc_sr_mt_pass (struct mandel_renderer *mandel, int chunk_size);
static gpointer sr_mt_thread_func (gpointer data);
static void calc_ms_mt (struct mandel_renderer *mandel);
static gpointer ms_mt_thread_func (gpointer data);
static void ms_queue_push (struct ms_state *state, int x0, int y0, int x1, int y1);
static void ms_do_work (struct mandel_renderer *md, int x0, int y0, int x1, int y1, void (*enqueue) (int, int, int, int, void *), void *data);
static void ms_enqueue (int x0, int y0, int x1, int y1, void *data);
static void ms_mt_enqueue (int x0, int y0, int x1, int y1, void *data);
static void render_btrace (struct mandel_renderer *md, int x0, int y0, unsigned char *flags, bool fill_mode);
static void render_btrace_test (struct mandel_renderer *md, int x0, int y0, int xstep0, int ystep0, GQueue *queue, unsigned char *flags, bool fill_mode);
static void bt_turn_right (int xs, int ys, int *xsn, int *ysn);
static void bt_turn_left (int xs, int ys, int *xsn, int *ysn);
static void mandeldata_init_mpvars (struct mandeldata *md);
static void btrace_queue_push (GQueue *queue, int x, int y, int xstep, int ystep);
static void btrace_queue_pop (GQueue *queue, int *x, int *y, int *xstep, int *ystep);
static bool mandel_all_neighbors_same (const struct mandel_renderer *mandel, unsigned x, unsigned y, unsigned d);
static void calcpart (struct mandel_renderer *md, int x0, int y0, int x1, int y1);
static void notify_update (struct mandel_renderer *mandel, int x, int y, int w, int h);



const char *const render_method_names[] = {
	"Successive Refinement",
	"Mariani-Silver",
	"Boundary Tracing"
};


void
mandel_convert_x_f (const struct mandel_renderer *mandel, mpf_ptr rop, unsigned op, bool aa_subpixel)
{
	mpf_sub (rop, mandel->xmax_f, mandel->xmin_f);
	mpf_mul_ui (rop, rop, op);
	unsigned w = mandel->w;
	if (!aa_subpixel)
		w /= mandel->aa_level;
	mpf_div_ui (rop, rop, w);
	mpf_add (rop, rop, mandel->xmin_f);
}


void
mandel_convert_y_f (const struct mandel_renderer *mandel, mpf_ptr rop, unsigned op, bool aa_subpixel)
{
	mpf_sub (rop, mandel->ymin_f, mandel->ymax_f);
	mpf_mul_ui (rop, rop, op);
	unsigned h = mandel->h;
	if (!aa_subpixel)
		h /= mandel->aa_level;
	mpf_div_ui (rop, rop, h);
	mpf_add (rop, rop, mandel->ymax_f);
}


static void
notify_update (struct mandel_renderer *mandel, int x, int y, int w, int h)
{
	if (mandel->notify_update == NULL)
		return;

	int xr = (x + w - 1) / mandel->aa_level;
	int yr = (y + h - 1) / mandel->aa_level;
	x /= mandel->aa_level;
	y /= mandel->aa_level;

	//printf ("%d %d %d %d\n", x, y, xr, yr);

	assert (xr < mandel->w / mandel->aa_level);
	assert (yr < mandel->h / mandel->aa_level);
	mandel->notify_update (x, y, xr - x + 1, yr - y + 1, mandel->user_data);
	//mandel->notify_update (x, y, 1, 1, mandel->user_data);
}


void
mandel_set_point (struct mandel_renderer *mandel, int x, int y, unsigned iter)
{
	volatile int *px = mandel->data + x * mandel->h + y;
	if (*px < 0)
		g_atomic_int_inc (&mandel->pixels_done);
	*px = iter;
}


void
mandel_put_point (struct mandel_renderer *mandel, unsigned x, unsigned y, unsigned iter)
{
	mandel_set_point (mandel, x, y, iter);
	notify_update (mandel, x, y, 1, 1);
}


int
mandel_get_point (const struct mandel_renderer *mandel, int x, int y)
{
	return mandel->data[x * mandel->h + y];
}


void
mandel_get_pixel (const struct mandel_renderer *mandel, int x, int y, struct color *px)
{
	uint32_t r = 0, g = 0, b = 0;

	for (unsigned xi = 0; xi < mandel->aa_level; xi++) {
		for (unsigned yi = 0; yi < mandel->aa_level; yi++) {
			int pval = mandel_get_point (mandel, x * mandel->aa_level + xi, y * mandel->aa_level + yi);
			if (pval < 0)
				continue;
			struct color *color = &mandel->palette[pval % mandel->palette_size];
			r += color->r;
			g += color->g;
			b += color->b;
		}
	}

	unsigned npixels = mandel->aa_level * mandel->aa_level;
	unsigned npxhalf = npixels / 2 - 1;
	px->r = (r + npxhalf) / npixels;
	px->g = (g + npxhalf) / npixels;
	px->b = (b + npxhalf) / npixels;
}


static bool
mandel_all_neighbors_same (const struct mandel_renderer *mandel, unsigned x, unsigned y, unsigned d)
{
	const int px = mandel_get_point (mandel, x, y);
	const int w = mandel->w - d, h = mandel->h - d;
	return
		   (x <  d || y <  d || mandel_get_point (mandel, x - d, y - d) == px)
		&& (x <  d           || mandel_get_point (mandel, x - d, y    ) == px)
		&& (x <  d || y >= h || mandel_get_point (mandel, x - d, y + d) == px)
		&& (          y <  d || mandel_get_point (mandel, x    , y - d) == px)
		&& (          y >= h || mandel_get_point (mandel, x    , y + d) == px)
		&& (x >= w || y <  d || mandel_get_point (mandel, x + d, y - d) == px)
		&& (x >= w           || mandel_get_point (mandel, x + d, y    ) == px)
		&& (x >= w || y >= h || mandel_get_point (mandel, x + d, y + d) == px);
	/*return x >= d && y >= d && x < mandel->w - d && y < mandel->h - d
		&& mandel_get_point (mandel, x - d, y - d) == px
		&& mandel_get_point (mandel, x - d, y    ) == px
		&& mandel_get_point (mandel, x - d, y + d) == px
		&& mandel_get_point (mandel, x    , y - d) == px
		&& mandel_get_point (mandel, x    , y + d) == px
		&& mandel_get_point (mandel, x + d, y - d) == px
		&& mandel_get_point (mandel, x + d, y    ) == px
		&& mandel_get_point (mandel, x + d, y + d) == px;*/
}


int
mandel_pixel_value (const struct mandel_renderer *mandel, int x, int y)
{
	unsigned i = 0; /* might end up uninitialized */
	bool inside = false;
	if (mandel->frac_limbs == 0) {
		// FP
		mandel_fp_t distance;
		/* FIXME we shouldn't do this for every pixel */
		mandel_fp_t xmin = mpf_get_mandel_fp (mandel->xmin_f);
		mandel_fp_t xmax = mpf_get_mandel_fp (mandel->xmax_f);
		mandel_fp_t ymin = mpf_get_mandel_fp (mandel->ymin_f);
		mandel_fp_t ymax = mpf_get_mandel_fp (mandel->ymax_f);
		mandel_fp_t xf = x * (xmax - xmin) / mandel->w + xmin;
		mandel_fp_t yf = y * (ymin - ymax) / mandel->h + ymax;
		inside = mandel->md->type->compute_fp (mandel->fractal_state, xf, yf, &i, &distance);
		if (!inside && mandel->md->repres.repres == REPRES_DISTANCE) {
			/* XXX colors and "target" magf shouldn't be hardwired */
			const mandel_fp_t kk = (mandel_fp_t) COLORS / log (1e9); 
			int idx = ((int) round (-kk * log (fabs (distance)))) % COLORS;
			if (idx < 0)
				idx += COLORS;
			i = idx;
		}
	} else {
		// MP
		unsigned total_limbs = INT_LIMBS + mandel->frac_limbs;
		mpf_t x0, y0;
		mpfr_t distance;
		mpf_init2 (x0, total_limbs * GMP_NUMB_BITS);
		mpf_init2 (y0, total_limbs * GMP_NUMB_BITS);
		if (mandel->md->repres.repres == REPRES_DISTANCE)
			mpfr_init2 (distance, total_limbs * GMP_NUMB_BITS);

		mandel_convert_x_f (mandel, x0, x, true);
		mandel_convert_y_f (mandel, y0, y, true);

		inside = mandel->md->type->compute (mandel->fractal_state, x0, y0, &i, distance);
		mpf_clear (x0);
		mpf_clear (y0);

		if (!inside && mandel->md->repres.repres == REPRES_DISTANCE) {
			if (true) {
				mpfr_abs (distance, distance, GMP_RNDN);
				mpfr_log (distance, distance, GMP_RNDN);
				mpfr_mul (distance, distance, mandel->rep_state.distance_est_k, GMP_RNDN);

				//int idx = ((int) round (-kk * log (fabs (distance)))) % 256;
				int idx = ((int) round (mpfr_get_d (distance, GMP_RNDN))) % 256;
				if (idx < 0)
					idx += COLORS;
				i = idx;
			}
			mpfr_clear (distance);
		}
	}
	switch (mandel->md->repres.repres) {
		case REPRES_ESCAPE:
			break;
		case REPRES_ESCAPE_LOG:
			i = mandel->rep_state.log_factor * log (i);
			break;
		case REPRES_DISTANCE:
			break;
		default:
			fprintf (stderr, "* ERROR: Unknown representation type %d in %s line %d\n", (int) mandel->md->repres.repres, __FILE__, __LINE__);
			break;
	}
	return i;
}


int
mandel_render_pixel (struct mandel_renderer *mandel, int x, int y)
{
	int i = mandel_get_point (mandel, x, y);
	if (i >= 0)
		return i; /* pixel has been rendered previously */
	i = mandel_pixel_value (mandel, x, y);
	mandel_put_point (mandel, x, y, i);
	return i;
}



void
mandel_display_rect (struct mandel_renderer *mandel, int x, int y, int w, int h, unsigned iter)
{
	notify_update (mandel, x, y, w, h);
}


void
mandel_put_rect (struct mandel_renderer *mandel, int x, int y, int w, int h, unsigned iter)
{
	int xc, yc;
	for (xc = x; xc < x + w; xc++)
		for (yc = y; yc < y + h; yc++)
			mandel_set_point (mandel, xc, yc, iter);
	mandel_display_rect (mandel, x, y, w, h, iter);
}


void
mandel_renderer_init (struct mandel_renderer *renderer, const struct mandeldata *md, unsigned w, unsigned h, unsigned aa_level)
{
	memset (renderer, 0, sizeof (*renderer)); /* just to be safe... */
	renderer->data = NULL;
	renderer->terminate = false;
	renderer->notify_update = NULL;
	mpf_init (renderer->xmin_f);
	mpf_init (renderer->xmax_f);
	mpf_init (renderer->ymin_f);
	mpf_init (renderer->ymax_f);

	renderer->md = md;
	renderer->w = w * aa_level;
	renderer->h = h * aa_level;
	g_atomic_int_set (&renderer->pixels_done, 0);
	renderer->aa_level = aa_level;

	renderer->aspect = (double) renderer->w / renderer->h;
	center_to_corners (renderer->xmin_f, renderer->xmax_f, renderer->ymin_f, renderer->ymax_f, renderer->md->area.center.real, renderer->md->area.center.imag, renderer->md->area.magf, renderer->aspect);

	// Determine the required precision.
	mpf_t dx;
	mpf_init (dx);

	mpf_sub (dx, renderer->xmax_f, renderer->xmin_f);
	mpf_div_ui (dx, dx, renderer->w);

	long exponent;
	mpf_get_d_2exp (&exponent, dx);

	mpf_clear (dx);

	if (exponent > 0)
		exponent = 0;

	// We add a minimum of 4 extra bits of precision, that should do.
	int required_bits = 4 - exponent;

	if (required_bits < MP_THRESHOLD)
		renderer->frac_limbs = 0;
	else
		renderer->frac_limbs = (required_bits + mp_bits_per_limb - 1) / mp_bits_per_limb;

	const unsigned frac_limbs = renderer->frac_limbs;
	const unsigned total_limbs = frac_limbs + INT_LIMBS;

	renderer->data = malloc (renderer->w * renderer->h * sizeof (*renderer->data));

	renderer->palette = mandel_get_default_palette ();
	renderer->palette_size = COLORS;

	fractal_type_flags_t flags = 0;
	switch (renderer->md->repres.repres) {
		case REPRES_ESCAPE:
			flags = FRAC_TYPE_ESCAPE_ITER;
			break;
		case REPRES_ESCAPE_LOG:
			flags = FRAC_TYPE_ESCAPE_ITER;
			renderer->rep_state.log_factor = 1.0 / log (renderer->md->repres.params.log_base);
			break;
		case REPRES_DISTANCE:
			flags = FRAC_TYPE_DISTANCE;
			mpfr_init2 (renderer->rep_state.distance_est_k, total_limbs * GMP_NUMB_BITS);
			//const double kk = 12.353265; /* 256.0 / log (1e9) */
			/* XXX */
			mpfr_set_str (renderer->rep_state.distance_est_k, "-12.353265", 10, GMP_RNDN);
			//mpfr_set_str (renderer->distance_est_k, "-3.7059796", 10, GMP_RNDN);
			break;
		default:
			fprintf (stderr, "* ERROR: Invalid representation type %d in %s line %d\n", (int) renderer->md->repres.repres, __FILE__, __LINE__);
			break;
	}
	renderer->fractal_state = renderer->md->type->state_new (renderer->md->type_param, flags, frac_limbs);
}


struct color *
mandel_create_default_palette (unsigned size)
{
	struct color *p = malloc (size * sizeof (*p));
	for (unsigned i = 0; i < size; i++) {
		p[i].r = (guint16) (sin (2 * M_PI * i / size) * 32767) + 32768;
		p[i].g = (guint16) (sin (4 * M_PI * i / size) * 32767) + 32768;
		p[i].b = (guint16) (sin (6 * M_PI * i / size) * 32767) + 32768;
	}
	return p;
}


struct color *
mandel_get_default_palette (void)
{
	static struct color *p = NULL;

	if (g_atomic_pointer_get (&p) == NULL) {
		struct color *p2 = mandel_create_default_palette (COLORS);
		if (!g_atomic_pointer_compare_and_exchange (&p, NULL, p2))
			free (p2);
	}

	return p;
}


void
mandel_renderer_clear (struct mandel_renderer *renderer)
{
	if (renderer->fractal_state != NULL)
		renderer->md->type->state_free (renderer->fractal_state);
	free_not_null (renderer->data);
	mpf_clear (renderer->xmin_f);
	mpf_clear (renderer->xmax_f);
	mpf_clear (renderer->ymin_f);
	mpf_clear (renderer->ymax_f);
	if (renderer->md->repres.repres == REPRES_DISTANCE)
		mpfr_clear (renderer->rep_state.distance_est_k);
}


void
mandel_render (struct mandel_renderer *mandel)
{
	int i;
	for (i = 0; i < mandel->w * mandel->h; i++)
		mandel->data[i] = -1;

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

			if (mandel->thread_count > 1)
				calc_ms_mt (mandel);
			else
				calcpart (mandel, 0, 0, mandel->w - 1, mandel->h - 1);

			break;
		}

		case RM_SUCCESSIVE_REFINE: {
			unsigned y, chunk_size = SR_CHUNK_SIZE;

			while (!mandel->terminate && chunk_size != 0) {
				if (mandel->thread_count > 1)
					calc_sr_mt_pass (mandel, chunk_size);
				else
					for (y = 0; !mandel->terminate && y < mandel->h; y += chunk_size)
						calc_sr_row (mandel, y, chunk_size);
				chunk_size >>= 1;
			}

			break;
		}

		case RM_BOUNDARY_TRACE: {
			unsigned char flags[mandel->w * mandel->h];
			memset (flags, 0, sizeof (flags));
			int x, y;
			for (y = 0; !mandel->terminate && y < mandel->h; y++)
				for (x = 0; !mandel->terminate && x < mandel->w; x++)
					if (!flags[x * mandel->h + y]) {
						render_btrace (mandel, x, y, flags, false);
						render_btrace (mandel, x, y, flags, true);
					}
			/*GQueue *queue = g_queue_new ();
			btrace_queue_push (queue, 0, 0, 0, -1);
			while (!g_queue_is_empty (queue)) {
				int x, y, xstep, ystep;
				btrace_queue_pop (queue, &x, &y, &xstep, &ystep);
				if (!flags[x * mandel->h + y]) {
					render_btrace_test (mandel, x, y, xstep, ystep, queue, flags, false);
					render_btrace_test (mandel, x, y, xstep, ystep, queue, flags, true);
				}
			}*/
			break;
		}

		default: {
			fprintf (stderr, "* BUG: invalid case value at %s:%d\n", __FILE__, __LINE__);
			break;
		}
	}
}


static void
calcpart (struct mandel_renderer *md, int x0, int y0, int x1, int y1)
{
	if (md->terminate)
		return;
	ms_do_work (md, x0, y0, x1, y1, ms_enqueue, md);
}


static void
calc_sr_row (struct mandel_renderer *mandel, int y, int chunk_size)
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
			mandel_display_rect (mandel, x, y, MIN (chunk_size, mandel->w - x), MIN (chunk_size, mandel->h - y), mandel_get_point (mandel, x, y));
		} else {
			mandel_put_point (mandel, x, y, mandel_get_point (mandel, parent_x, parent_y));
		}
	}
}


static void
calc_sr_mt_pass (struct mandel_renderer *mandel, int chunk_size)
{
	struct sr_state state = {mandel, 0, chunk_size, g_mutex_new ()};
	GThread *threads[mandel->thread_count];
	int i;

	for (i = 0; i < mandel->thread_count; i++)
		threads[i] = g_thread_create (sr_mt_thread_func, &state, TRUE, NULL);
	for (i = 0; i < mandel->thread_count; i++)
		g_thread_join (threads[i]);

	g_mutex_free (state.mutex);
}


static gpointer
sr_mt_thread_func (gpointer data)
{
	struct sr_state *state = (struct sr_state *) data;
	while (!state->renderer->terminate) {
		int y;
		g_mutex_lock (state->mutex);
		y = state->y;
		state->y += state->chunk_size;
		g_mutex_unlock (state->mutex);
		if (y >= state->renderer->h)
			break; /* done */
		calc_sr_row (state->renderer, y, state->chunk_size);
	}
	return NULL;
}


static void
calc_ms_mt (struct mandel_renderer *mandel)
{
	struct ms_state state = {mandel, g_mutex_new (), g_queue_new (), g_cond_new (), 0};
	GThread *threads[mandel->thread_count];
	int i;

	ms_queue_push (&state, 0, 0, mandel->w - 1, mandel->h - 1);

	for (i = 0; i < mandel->thread_count; i++)
		threads[i] = g_thread_create (ms_mt_thread_func, &state, TRUE, NULL);

	for (i = 0; i < mandel->thread_count; i++)
		g_thread_join (threads[i]);

	g_mutex_free (state.mutex);
	g_queue_free (state.queue);
	g_cond_free (state.cond);
}


static gpointer
ms_mt_thread_func (gpointer data)
{
	struct ms_state *state = (struct ms_state *) data;
	struct mandel_renderer *md = state->renderer;
	while (!md->terminate) {
		g_mutex_lock (state->mutex);
		state->idle_threads++;
		/* Notify all waiting threads about the increase of idle_threads */
		g_cond_broadcast (state->cond);
		while (g_queue_is_empty (state->queue)) {
			if (state->idle_threads == md->thread_count) {
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
ms_do_work (struct mandel_renderer *md, int x0, int y0, int x1, int y1, void (*enqueue) (int, int, int, int, void *), void *data)
{
	int x, y;
	bool failed = false;
	unsigned p0 = mandel_get_point (md, x0, y0);

	for (x = x0; !failed && x <= x1; x++)
		failed = mandel_get_point (md, x, y0) != p0 || mandel_get_point (md, x, y1) != p0;

	for (y = y0; !failed && y <= y1; y++)
		failed = mandel_get_point (md, x0, y) != p0 || mandel_get_point (md, x1, y) != p0;

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
	struct mandel_renderer *renderer = (struct mandel_renderer *) data;
	calcpart (renderer, x0, y0, x1, y1);
}


static void
ms_mt_enqueue (int x0, int y0, int x1, int y1, void *data)
{
	struct ms_state *state = (struct ms_state *) data;
	ms_queue_push (state, x0, y0, x1, y1);
}


unsigned
mandel_get_precision (const struct mandel_renderer *mandel)
{
	if (mandel->frac_limbs == 0)
		return 0;
	else
		return (mandel->frac_limbs + INT_LIMBS) * mp_bits_per_limb;
}


static bool
is_inside (struct mandel_renderer *md, int x, int y, int iter)
{
	int p = mandel_get_point (md, x, y);
	return p == -1 || p == iter;
}


static void
render_btrace (struct mandel_renderer *md, int x0, int y0, unsigned char *flags, bool fill_mode)
{
	int x = x0, y = y0;
	/* XXX is it safe to choose this arbitrarily? */
	int xstep = 0, ystep = -1;
	unsigned inside = mandel_render_pixel (md, x0, y0);

	int turns = 0;
	while (!md->terminate) {
		if (x + xstep < 0 || x + xstep >= md->w || y + ystep < 0 || y + ystep >= md->h || mandel_render_pixel (md, x + xstep, y + ystep) != inside) {
			/* can't move forward, turn left */
			bt_turn_left (xstep, ystep, &xstep, &ystep);
			if (++turns == 4)
				break;
			continue;
		}
		/* move forward */
		turns = 0;
		x += xstep;
		y += ystep;
		if (fill_mode && (xstep == 1 || ystep == 1)) {
			int xfs, yfs;
			bt_turn_left (xstep, ystep, &xfs, &yfs);
			int xf = x, yf = y;
			while (xf >= 0 && yf >= 0 && xf < md->w && yf < md->h && is_inside (md, xf, yf, inside)) {
				flags[xf * md->h + yf] = 1;
				mandel_put_point (md, xf, yf, inside);
				xf += xfs;
				yf += yfs;
			}
		}
		if (x == x0 && y == y0)
			break;
		int xsn, ysn;
		bt_turn_right (xstep, ystep, &xsn, &ysn);
		/* If we don't have a wall at the right, turn right. */
		if (x + xsn >= 0 && x + xsn < md->w && y + ysn >= 0 && y + ysn < md->h && mandel_render_pixel (md, x + xsn, y + ysn) == inside) {
			xstep = xsn;
			ystep = ysn;
		}
	}
}


static inline bool
pixel_in_bounds (struct mandel_renderer *renderer, int x, int y)
{
	return x >= 0 && x < renderer->w && y >= 0 && y < renderer->h;
}


static void
render_btrace_test (struct mandel_renderer *md, int x0, int y0, int xstep0, int ystep0, GQueue *queue, unsigned char *flags, bool fill_mode)
{
	int x = x0, y = y0;
	int xstep = xstep0, ystep = ystep0;
	unsigned inside = mandel_render_pixel (md, x0, y0);

	int turns = 0;
	while (!md->terminate) {
		if (fill_mode)
			flags[x * md->h + y] = 1;
		if (!pixel_in_bounds (md, x + xstep, y + ystep) || mandel_render_pixel (md, x + xstep, y + ystep) != inside) {
			if (fill_mode && pixel_in_bounds (md, x + xstep, y + ystep))
				btrace_queue_push (queue, x + xstep, y + ystep, -ystep, xstep);
			/* can't move forward, turn left */
			bt_turn_left (xstep, ystep, &xstep, &ystep);
			if (++turns == 4)
				break;
			continue;
		}
		/* Do the filling if we're looking into the oppisite of the initial
		 * direction, or right of it. It's not exactly clear why this works
		 * (and filling in other directions doesn't), this was determined
		 * empirically... */
		if (fill_mode && ((xstep == -xstep0 && ystep == -ystep0) || (xstep == -ystep0 && ystep == xstep0))) {
			int xfs, yfs;
			bt_turn_left (xstep, ystep, &xfs, &yfs);
			int xf = x + xfs, yf = y + yfs;
			while (pixel_in_bounds (md, xf, yf) && is_inside (md, xf, yf, inside)) {
				flags[xf * md->h + yf] = 1;
				mandel_put_point (md, xf, yf, inside);
				xf += xfs;
				yf += yfs;
			}
		}
		/* move forward */
		turns = 0;
		x += xstep;
		y += ystep;
		if (x == x0 && y == y0)
			break;
		int xsn, ysn;
		bt_turn_right (xstep, ystep, &xsn, &ysn);
		/* If we don't have a wall at the right, turn right. */
		if (pixel_in_bounds (md, x + xsn, y + ysn) && mandel_render_pixel (md, x + xsn, y + ysn) == inside) {
			xstep = xsn;
			ystep = ysn;
		} else if (fill_mode && pixel_in_bounds (md, x + xsn, y + ysn))
			btrace_queue_push (queue, x + xsn, y + ysn, -xstep, -ystep);
	}
}


static void
bt_turn_right (int xs, int ys, int *xsn, int *ysn)
{
	*xsn = -ys;
	*ysn = xs;
}


static void
bt_turn_left (int xs, int ys, int *xsn, int *ysn)
{
	*xsn = ys;
	*ysn = -xs;
}


static void
mandeldata_init_mpvars (struct mandeldata *md)
{
	mandel_area_init (&md->area);
}


void
mandeldata_init (struct mandeldata *md, const struct fractal_type *type)
{
	memset (md, 0, sizeof (*md));
	md->type = type;
	mandeldata_init_mpvars (md);
	md->type_param = type->param_new ();
}


void
mandeldata_clear (struct mandeldata *md)
{
	mandel_area_clear (&md->area);
	md->type->param_free (md->type_param);
}


void
mandeldata_clone (struct mandeldata *clone, const struct mandeldata *orig)
{
	memcpy (clone, orig, sizeof (*clone));
	mandeldata_init_mpvars (clone);
	mpf_set (clone->area.center.real, orig->area.center.real);
	mpf_set (clone->area.center.imag, orig->area.center.imag);
	mpf_set (clone->area.magf, orig->area.magf);
	clone->type_param = orig->type->param_clone (orig->type_param);
}


static void
btrace_queue_push (GQueue *queue, int x, int y, int xstep, int ystep)
{
	struct btrace_q_entry *entry = malloc (sizeof (*entry));
	entry->x = x;
	entry->y = y;
	entry->xstep = xstep;
	entry->ystep = ystep;
	g_queue_push_tail (queue, entry);
}


static void
btrace_queue_pop (GQueue *queue, int *x, int *y, int *xstep, int *ystep)
{
	struct btrace_q_entry *entry = g_queue_pop_head (queue);
	*x = entry->x;
	*y = entry->y;
	*xstep = entry->xstep;
	*ystep = entry->ystep;
	free (entry);
}


double
mandel_renderer_progress (const struct mandel_renderer *renderer)
{
	return (double) g_atomic_int_get (&renderer->pixels_done) / (renderer->w * renderer->h);
}


unsigned
mandel_renderer_width (const struct mandel_renderer *renderer)
{
	return renderer->w / renderer->aa_level;
}


unsigned
mandel_renderer_height (const struct mandel_renderer *renderer)
{
	return renderer->h / renderer->aa_level;
}


int
fractal_supported_representations (const struct fractal_type *type, fractal_repres_t *res)
{
	int i = 0;
	if (type->flags & FRAC_TYPE_ESCAPE_ITER) {
		res[i++] = REPRES_ESCAPE;
		res[i++] = REPRES_ESCAPE_LOG;
	}
	if (type->flags & FRAC_TYPE_DISTANCE) {
		res[i++] = REPRES_DISTANCE;
	}
	return i;
}


static void
mandelbrot_param_set_defaults (struct mandeldata *md)
{
	struct mandelbrot_param *param = (struct mandelbrot_param *) md->type_param;
	mpf_set_d (md->area.center.real, 0.0);
	mpf_set_d (md->area.center.imag, 0.0);
	mpf_set_d (md->area.magf, 0.5);
	param->mjparam.zpower = 2;
	param->mjparam.maxiter = 1000;
	md->repres.repres = REPRES_ESCAPE;
}


static void
julia_param_set_defaults (struct mandeldata *md)
{
	struct julia_param *param = (struct julia_param *) md->type_param;
	mpf_set_d (md->area.center.real, 0.0);
	mpf_set_d (md->area.center.imag, 0.0);
	mpf_set_d (md->area.magf, 0.5);
	param->mjparam.zpower = 2;
	param->mjparam.maxiter = 1000;
	mpf_set_d (param->param.real, 0.42);
	mpf_set_d (param->param.imag, 0.42);
	md->repres.repres = REPRES_ESCAPE;
}


void
mandeldata_set_defaults (struct mandeldata *md)
{
	switch (md->type->type) {
		case FRACTAL_MANDELBROT:
			mandelbrot_param_set_defaults (md);
			break;
		case FRACTAL_JULIA:
			julia_param_set_defaults (md);
			break;
		default:
			fprintf (stderr, "* BUG: unknown fractal type %d at %s:%d\n", (int) md->type->type, __FILE__, __LINE__);
			break;
	}
}
