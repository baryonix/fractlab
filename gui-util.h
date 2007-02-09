#ifndef _GTKMANDEL_GUI_UTIL_H
#define _GTKMANDEL_GUI_UTIL_H

GtkWidget *my_gtk_label_new (const gchar *text, GtkSizeGroup *size_group);
GtkWidget *my_gtk_label_new_with_align (const gchar *text, GtkSizeGroup *size_group, double xalign, double yalign);
GtkWidget *my_gtk_stock_menu_item_with_label (const gchar *stock_id, const gchar *label);
void mpf_from_entry (GtkEntry *entry, mpf_ptr val, mpf_srcptr orig_val, const char *orig_val_str);
void my_g_object_unref_not_null (gpointer object);
void my_gtk_widget_destroy_unref (GtkWidget *widget);
GtkWidget *my_gtk_error_dialog_new (GtkWindow *parent, const char *title, const char *msg);

#endif /* _GTKMANDEL_GUI_UTIL_H */
