// ANSI C
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

// POSIX
#include <unistd.h>
#include <errno.h>

// GTK
#include <gtk/gtk.h>
#include <gdk/gdk.h>

// GMP
#include <gmp.h>


#include "cmdline.h"
#include "file.h"
#include "fpdefs.h"
#include "mandelbrot.h"
#include "gtkmandel.h"
#include "defs.h"
#include "gui.h"
#include "util.h"


int
main (int argc, char **argv)
{
	g_thread_init (NULL);

	//parse_command_line (&argc, &argv);

	mpf_set_default_prec (1024); /* ? */

	int i;
	for (i = 0; i < COLORS; i++) {
		mandelcolors[i].red = (guint16) (sin (2 * M_PI * i / COLORS) * 32767) + 32768;
		mandelcolors[i].green = (guint16) (sin (4 * M_PI * i / COLORS) * 32767) + 32768;
		mandelcolors[i].blue = (guint16) (sin (6 * M_PI * i / COLORS) * 32767) + 32768;
	}

	gtk_init (&argc, &argv);

#if 0
	if (option_start_coords == NULL) {
		fprintf (stderr, "No start coordinates specified.\n");
		exit (2);
	}

	GtkMandelArea *area = gtk_mandel_area_new_from_file (option_start_coords);
	if (area == NULL) {
		fprintf (stderr, "%s: Something went wrong reading the file.\n", option_start_coords);
		exit (2);
	}
#endif

	struct mandeldata md[1];
	mandeldata_init (md, fractal_type_by_id (FRACTAL_MANDELBROT));
	struct mandelbrot_param *mparam = (struct mandelbrot_param *) md->type_param;
	mparam->mjparam.zpower = 2;
	mparam->mjparam.maxiter = 1000;
	md->repres.repres = REPRES_ESCAPE;
	mpf_set_str (md->area.center.real, "-.5", 10);
	mpf_set_str (md->area.center.imag, "0", 10);
	mpf_set_str (md->area.magf, ".5", 10);
	GtkMandelApplication *app = gtk_mandel_application_new (md);
	mandeldata_clear (md);
	gtk_mandel_application_start (app);
	gtk_main ();

	return 0;
}
