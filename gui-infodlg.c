#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <gtk/gtk.h>

#include "mandelbrot.h"
#include "gui-util.h"
#include "gui-infodlg.h"
#include "util.h"


struct _FractalInfoDialogPrivate {
	GtkTextBuffer *center_buffers[3], *corner_buffers[4];
	bool disposed;
};


static GtkTextBuffer *create_area_info_item (GtkWidget *table, int i, const char *label);
static void fractal_info_dialog_class_init (gpointer g_class, gpointer data);
static void fractal_info_dialog_init (GTypeInstance *instance, gpointer g_class);
static void fractal_info_dialog_dispose (GObject *object);
static void fractal_info_dialog_finalize (GObject *object);


GType
fractal_info_dialog_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			.class_size		= sizeof (FractalInfoDialogClass),
			.class_init		= fractal_info_dialog_class_init,
			.instance_size	= sizeof (FractalInfoDialog),
			.instance_init	= fractal_info_dialog_init
		};
		type = g_type_register_static (GTK_TYPE_DIALOG, "FractalInfoDialog", &info, 0);
	}

	return type;
}


static void
fractal_info_dialog_class_init (gpointer g_class, gpointer data)
{
	G_OBJECT_CLASS (g_class)->dispose = fractal_info_dialog_dispose;
	G_OBJECT_CLASS (g_class)->finalize = fractal_info_dialog_finalize;
}


static void
fractal_info_dialog_init (GTypeInstance *instance, gpointer g_class)
{
	FractalInfoDialog *dlg = FRACTAL_INFO_DIALOG (instance);
	GtkWidget *notebook, *container;

	dlg->priv = malloc (sizeof (*dlg->priv));
	FractalInfoDialogPrivate *const priv = dlg->priv;
	priv->disposed = false;

	gtk_window_set_title (GTK_WINDOW (dlg), "Fractal Info");
	gtk_dialog_add_buttons (GTK_DIALOG (dlg), GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);
	gtk_dialog_set_has_separator (GTK_DIALOG (dlg), FALSE);
	GdkGeometry geom;
	geom.max_width = 1000000; /* FIXME how to set max_width = unlimited? */
	geom.max_height = -1;
	gtk_window_set_geometry_hints (GTK_WINDOW (dlg), NULL, &geom, GDK_HINT_MAX_SIZE);

	notebook = gtk_notebook_new ();
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dlg)->vbox), notebook, FALSE, FALSE, 0);

	container = gtk_table_new (2, 4, false);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), container, gtk_label_new ("Center"));
	gtk_table_set_homogeneous (GTK_TABLE (container), FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (container), 2);
	gtk_table_set_col_spacings (GTK_TABLE (container), 2);
	gtk_container_set_border_width (GTK_CONTAINER (container), 2);
	priv->center_buffers[0] = create_area_info_item (container, 0, "Center Real");
	priv->center_buffers[1] = create_area_info_item (container, 1, "Center Imag");
	priv->center_buffers[2] = create_area_info_item (container, 2, "Magnification");

	container = gtk_table_new (2, 4, false);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), container, gtk_label_new ("Corners"));
	gtk_table_set_homogeneous (GTK_TABLE (container), FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (container), 2);
	gtk_table_set_col_spacings (GTK_TABLE (container), 2);
	gtk_container_set_border_width (GTK_CONTAINER (container), 2);
	priv->corner_buffers[0] = create_area_info_item (container, 0, "xmin");
	priv->corner_buffers[1] = create_area_info_item (container, 1, "xmax");
	priv->corner_buffers[2] = create_area_info_item (container, 2, "ymin");
	priv->corner_buffers[3] = create_area_info_item (container, 3, "ymax");

	gtk_widget_show_all (GTK_WIDGET (GTK_DIALOG (dlg)->vbox));
}


static GtkTextBuffer *
create_area_info_item (GtkWidget *table, int i, const char *label)
{
	GtkTextBuffer *buffer;
	GtkWidget *widget;
	gtk_table_attach (GTK_TABLE (table), my_gtk_label_new_with_align (label, NULL, 0.0, 0.5), 0, 1, i, i + 1, GTK_FILL, 0, 0, 0);
	buffer = gtk_text_buffer_new (NULL);
	widget = gtk_text_view_new_with_buffer (buffer);
	gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (widget), GTK_WRAP_CHAR);
	gtk_text_view_set_editable (GTK_TEXT_VIEW (widget), FALSE);
	gtk_table_attach (GTK_TABLE (table), widget, 1, 2, i, i + 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
	g_object_ref (buffer);
	return buffer;
}


void
fractal_info_dialog_set_mandeldata (FractalInfoDialog *dlg, const struct mandeldata *md, double aspect)
{
	FractalInfoDialogPrivate *priv = dlg->priv;
	if (priv->disposed)
		return;

	char b0[1024], b1[1024], b2[1024];
	mpf_t xmin, xmax, ymin, ymax;

	if (center_coords_to_string (md->area.center.real, md->area.center.imag, md->area.magf, b0, b1, b2, 1024) >= 0) {
		gtk_text_buffer_set_text (priv->center_buffers[0], b0, strlen (b0));
		gtk_text_buffer_set_text (priv->center_buffers[1], b1, strlen (b1));
		gtk_text_buffer_set_text (priv->center_buffers[2], b2, strlen (b2));
	}

	mpf_init (xmin);
	mpf_init (xmax);
	mpf_init (ymin);
	mpf_init (ymax);

	center_to_corners (xmin, xmax, ymin, ymax, md->area.center.real, md->area.center.imag, md->area.magf, aspect);

	if (coord_pair_to_string (xmin, xmax, b0, b1, 1024) >= 0) {
		gtk_text_buffer_set_text (priv->corner_buffers[0], b0, strlen (b0));
		gtk_text_buffer_set_text (priv->corner_buffers[1], b1, strlen (b1));
	}
	if (coord_pair_to_string (ymin, ymax, b0, b1, 1024) >= 0) {
		gtk_text_buffer_set_text (priv->corner_buffers[2], b0, strlen (b0));
		gtk_text_buffer_set_text (priv->corner_buffers[3], b1, strlen (b1));
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


static void
fractal_info_dialog_dispose (GObject *object)
{
	fprintf (stderr, "* DEBUG: disposing info dialog\n");
	FractalInfoDialog *const dlg = FRACTAL_INFO_DIALOG (object);
	FractalInfoDialogPrivate *const priv = dlg->priv;
	GObjectClass *const parent_class = g_type_class_peek_parent (G_OBJECT_GET_CLASS (object));
	if (!priv->disposed) {
		int i;
		for (i = 0; i < 3; i++) {
			g_object_unref (priv->center_buffers[i]);
			priv->center_buffers[i] = NULL;
		}
		for (i = 0; i < 4; i++) {
			g_object_unref (priv->corner_buffers[i]);
			priv->corner_buffers[i] = NULL;
		}
		priv->disposed = true;
	}
	parent_class->dispose (object);
}


static void
fractal_info_dialog_finalize (GObject *object)
{
	fprintf (stderr, "* DEBUG: finalizing info dialog\n");
	FractalInfoDialog *const dlg = FRACTAL_INFO_DIALOG (object);
	GObjectClass *const parent_class = g_type_class_peek_parent (G_OBJECT_GET_CLASS (object));
	free (dlg->priv);
	parent_class->finalize (object);
}
