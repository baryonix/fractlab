#include <string.h>

#include <gtk/gtk.h>

#include <gmp.h>

#include "gui-util.h"


GtkWidget *
my_gtk_label_new (const gchar *text, GtkSizeGroup *size_group)
{
	GtkWidget *label = gtk_label_new (text);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	if (size_group != NULL)
		gtk_size_group_add_widget (size_group, label);
	return label;
}


void
mpf_from_entry (GtkEntry *entry, mpf_ptr val, mpf_srcptr orig_val, const char *orig_val_str)
{
	const char *text = gtk_entry_get_text (entry);
	if (strcmp (text, orig_val_str) == 0)
		mpf_set (val, orig_val);
	else
		mpf_set_str (val, text, 10);
}
