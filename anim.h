#ifndef _GTKMANDEL_ANIM_H
#define _GTKMANDEL_ANIM_H

#define DEFAULT_MAXITER 1000

#include <glib.h>

#include "fractal-render.h"

typedef void (*frame_func_t) (void *data, struct mandeldata *md, unsigned long i);

extern gint img_width, img_height, frame_count;

GOptionGroup *anim_get_option_group (void);
void anim_render (frame_func_t frame_func, void *data);

#endif /* _GTKMANDEL_ANIM_H */
