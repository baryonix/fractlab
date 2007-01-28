#ifndef _GTKMANDEL_GUI_UTIL_H
#define _GTKMANDEL_GUI_UTIL_H

GtkWidget *my_gtk_label_new (const gchar *text, GtkSizeGroup *size_group);
void mpf_from_entry (GtkEntry *entry, mpf_ptr val, mpf_srcptr orig_val, const char *orig_val_str);

#endif /* _GTKMANDEL_GUI_UTIL_H */
