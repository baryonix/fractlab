#ifndef _MANDEL_CMDLINE_H
#define _MANDEL_CMDLINE_H

#include <glib.h>

extern gchar *option_start_coords;
extern int thread_count;

void parse_command_line (int *argc, char ***argv);

#endif /* _MANDEL_CMDLINE_H */
