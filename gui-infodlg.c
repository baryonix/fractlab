#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <gtk/gtk.h>

#include "mandelbrot.h"
#include "gui-infodlg.h"
#include "util.h"


struct area_info_item {
	GtkWidget *label, *view;
	GtkTextBuffer *buffer;
};


struct _FractalInfoDialogPrivate {
	GtkWidget *notebook;
	GtkWidget *corners_label, *center_label;
	struct {
		GtkWidget *table;
		struct area_info_item items[3];
	} center;
	struct {
		GtkWidget *table;
		struct area_info_item items[4];
	} corners;
};


static void create_area_info_item (GtkWidget *table, struct area_info_item *item, int i, const char *label);
static void fractal_info_dialog_class_init (gpointer g_class, gpointer data);
static void fractal_info_dialog_init (GTypeInstance *instance, gpointer g_class);


GType
fractal_info_dialog_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof (FractalInfoDialogClass),
			NULL, NULL,
			fractal_info_dialog_class_init,
			NULL, NULL,
			sizeof (FractalInfoDialog),
			0,
			fractal_info_dialog_init
		};
		type = g_type_register_static (GTK_TYPE_DIALOG, "FractalInfoDialog", &info, 0);
	}

	return type;
}


static void
fractal_info_dialog_class_init (gpointer g_class, gpointer data)
{
}


static void
fractal_info_dialog_init (GTypeInstance *instance, gpointer g_class)
{
	FractalInfoDialog *dlg = FRACTAL_INFO_DIALOG (instance);

	dlg->priv = malloc (sizeof (*dlg->priv));
	FractalInfoDialogPrivate *const priv = dlg->priv;

	gtk_window_set_title (GTK_WINDOW (dlg), "Fractal Info");
	gtk_dialog_add_buttons (GTK_DIALOG (dlg), GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);

	priv->center.table = gtk_table_new (2, 4, false);
	gtk_table_set_homogeneous (GTK_TABLE (priv->center.table), FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (priv->center.table), 2);
	gtk_table_set_col_spacings (GTK_TABLE (priv->center.table), 2);
	gtk_container_set_border_width (GTK_CONTAINER (priv->center.table), 2);
	create_area_info_item (priv->center.table, priv->center.items + 0, 0, "cx");
	create_area_info_item (priv->center.table, priv->center.items + 1, 1, "cy");
	create_area_info_item (priv->center.table, priv->center.items + 2, 2, "magf");

	priv->corners.table = gtk_table_new (2, 4, false);
	gtk_table_set_homogeneous (GTK_TABLE (priv->corners.table), FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (priv->corners.table), 2);
	gtk_table_set_col_spacings (GTK_TABLE (priv->corners.table), 2);
	gtk_container_set_border_width (GTK_CONTAINER (priv->corners.table), 2);
	create_area_info_item (priv->corners.table, priv->corners.items + 0, 0, "xmin");
	create_area_info_item (priv->corners.table, priv->corners.items + 1, 1, "xmax");
	create_area_info_item (priv->corners.table, priv->corners.items + 2, 2, "ymin");
	create_area_info_item (priv->corners.table, priv->corners.items + 3, 3, "ymax");

	priv->center_label = gtk_label_new ("Center");
	priv->corners_label = gtk_label_new ("Corners");

	priv->notebook = gtk_notebook_new ();
	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), priv->center.table, priv->center_label);
	gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook), priv->corners.table, priv->corners_label);

	gtk_dialog_set_has_separator (GTK_DIALOG (dlg), FALSE);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dlg)->vbox), priv->notebook, FALSE, FALSE, 0);

	GdkGeometry geom;
	geom.max_width = 1000000; /* FIXME how to set max_width = unlimited? */
	geom.max_height = -1;
	gtk_window_set_geometry_hints (GTK_WINDOW (dlg), NULL, &geom, GDK_HINT_MAX_SIZE);

	gtk_widget_show_all (GTK_WIDGET (GTK_DIALOG (dlg)->vbox));
}


static void
create_area_info_item (GtkWidget *table, struct area_info_item *item, int i, const char *label)
{
	item->label = gtk_label_new (label);
	gtk_misc_set_alignment (GTK_MISC (item->label), 0.0, 0.5);
	item->buffer = gtk_text_buffer_new (NULL);
	item->view = gtk_text_view_new_with_buffer (item->buffer);
	gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (item->view), GTK_WRAP_CHAR);
	gtk_text_view_set_editable (GTK_TEXT_VIEW (item->view), FALSE);
	gtk_table_attach (GTK_TABLE (table), item->label, 0, 1, i, i + 1, GTK_FILL, 0, 0, 0);
	gtk_table_attach (GTK_TABLE (table), item->view, 1, 2, i, i + 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
}


void
fractal_info_dialog_set_mandeldata (FractalInfoDialog *dlg, const struct mandeldata *md, double aspect)
{
	FractalInfoDialogPrivate *priv = dlg->priv;

	char b0[1024], b1[1024], b2[1024];
	mpf_t xmin, xmax, ymin, ymax;

	if (center_coords_to_string (md->area.center.real, md->area.center.imag, md->area.magf, b0, b1, b2, 1024) >= 0) {
		gtk_text_buffer_set_text (priv->center.items[0].buffer, b0, strlen (b0));
		gtk_text_buffer_set_text (priv->center.items[1].buffer, b1, strlen (b1));
		gtk_text_buffer_set_text (priv->center.items[2].buffer, b2, strlen (b2));
	}

	mpf_init (xmin);
	mpf_init (xmax);
	mpf_init (ymin);
	mpf_init (ymax);

	center_to_corners (xmin, xmax, ymin, ymax, md->area.center.real, md->area.center.imag, md->area.magf, aspect);

	if (coord_pair_to_string (xmin, xmax, b0, b1, 1024) >= 0) {
		gtk_text_buffer_set_text (priv->corners.items[0].buffer, b0, strlen (b0));
		gtk_text_buffer_set_text (priv->corners.items[1].buffer, b1, strlen (b1));
	}
	if (coord_pair_to_string (ymin, ymax, b0, b1, 1024) >= 0) {
		gtk_text_buffer_set_text (priv->corners.items[2].buffer, b0, strlen (b0));
		gtk_text_buffer_set_text (priv->corners.items[3].buffer, b1, strlen (b1));
	}

	mpf_clear (xmin);
	mpf_clear (xmax);
	mpf_clear (ymin);
	mpf_clear (ymax);
}


FractalInfoDialog *
fractal_info_dialog_new (GtkWindow *parent)
{
	FractalInfoDialog *dlg = FRACTAL_INFO_DIALOG (g_object_new (TYPE_FRACTAL_INFO_DIALOG, NULL));
	if (parent != NULL)
		gtk_window_set_transient_for (GTK_WINDOW (dlg), parent);
	return dlg;
}
