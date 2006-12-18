#ifndef _MANDEL_CMDLINE_H
#define _MANDEL_CMDLINE_H

#include <glib.h>

extern gchar *option_center_coords, *option_corner_coords;

void parse_command_line (int *argc, char ***argv);

#endif /* _MANDEL_CMDLINE_H */
