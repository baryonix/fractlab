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
	gdk_threads_init ();
	gdk_threads_enter ();

	parse_command_line (&argc, &argv);

	mpf_set_default_prec (1024); /* ? */

	int i;
	for (i = 0; i < COLORS; i++) {
		mandelcolors[i].red = (guint16) (sin (2 * M_PI * i / COLORS) * 32767) + 32768;
		mandelcolors[i].green = (guint16) (sin (4 * M_PI * i / COLORS) * 32767) + 32768;
		mandelcolors[i].blue = (guint16) (sin (6 * M_PI * i / COLORS) * 32767) + 32768;
	}

	gtk_init (&argc, &argv);

	if (option_start_coords == NULL) {
		fprintf (stderr, "No start coordinates specified.\n");
		exit (2);
	}

	GtkMandelArea *area = gtk_mandel_area_new_from_file (option_start_coords);
	if (area == NULL) {
		fprintf (stderr, "%s: Something went wrong reading the file.\n", option_start_coords);
		exit (2);
	}

	GtkMandelApplication *app = gtk_mandel_application_new (area, 1000, RM_SUCCESSIVE_REFINE, 0.0);
	gtk_mandel_application_start (app);
	gtk_main ();
	gdk_threads_leave ();

	return 0;
}
