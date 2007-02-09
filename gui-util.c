#include <string.h>

#include <gtk/gtk.h>

#include <gmp.h>

#include "gui-util.h"


GtkWidget *
my_gtk_label_new (const gchar *text, GtkSizeGroup *size_group)
{
	return my_gtk_label_new_with_align (text, size_group, 0.0, 0.5);
}


GtkWidget *
my_gtk_label_new_with_align (const gchar *text, GtkSizeGroup *size_group, double xalign, double yalign)
{
	GtkWidget *label = gtk_label_new (text);
	gtk_misc_set_alignment (GTK_MISC (label), xalign, yalign);
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


GtkWidget *
my_gtk_stock_menu_item_with_label (const gchar *stock_id, const gchar *label)
{
	/* Dirty... */
	GtkWidget *widget = gtk_image_menu_item_new_from_stock (stock_id, NULL);
	gtk_label_set_text (GTK_LABEL (gtk_bin_get_child (GTK_BIN (widget))), label);
	return widget;
}


void
my_g_object_unref_not_null (gpointer object)
{
	if (object != NULL)
		g_object_unref (object);
}


void
my_gtk_widget_destroy_unref (GtkWidget *widget)
{
	if (widget == NULL)
		return;
	gtk_widget_destroy (widget);
	g_object_unref (widget);
}


GtkWidget *
my_gtk_error_dialog_new (GtkWindow *parent, const char *title, const char *msg)
{
	GtkWidget *dlg = gtk_message_dialog_new (parent, GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR, GTK_BUTTONS_NONE, "%s", msg);
	gtk_window_set_title (GTK_WINDOW (dlg), title);
	gtk_dialog_add_button (GTK_DIALOG (dlg), "Shit", GTK_RESPONSE_CLOSE);
	g_signal_connect (dlg, "response", (GCallback) gtk_widget_destroy, NULL);
	return dlg;
}
