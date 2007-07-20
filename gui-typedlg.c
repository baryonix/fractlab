#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <gtk/gtk.h>

#include "fractal-render.h"
#include "util.h"
#include "gui-util.h"
#include "gui-typedlg.h"


typedef enum {
	GUI_FTYPE_HAS_MAXITER = 1 << 0
} gui_ftype_flags_t;

struct fractal_type_dlg;

/* Static type-specific information. */
struct gui_fractal_type {
	gui_ftype_flags_t flags;
	struct gui_type_param *(*create_gui) (GtkSizeGroup *label_size_group, GtkSizeGroup *input_size_group);
	void (*dispose) (struct gui_type_param *gui_param);
	void (*finalize) (struct gui_type_param *gui_param);
	void (*set_param) (FractalTypeDialog *dlg, struct gui_type_param *gui_param, const void *param);
	void (*get_param) (FractalTypeDialog *dlg, struct gui_type_param *gui_param, void *param);
};

/* Dynamic type-specific information. */
struct gui_fractal_type_dynamic {
	fractal_repres_t repres[REPRES_MAX];
	int repres_count;
	struct gui_type_param *gui;
};

struct gui_type_param {
	GtkWidget *main_widget;
};

struct gui_mandelbrot_param {
	struct gui_type_param ftype;
	GtkWidget *zpower_input;
};

struct gui_julia_param {
	struct gui_type_param ftype;
	GtkWidget *zpower_input, *preal_input, *pimag_input;
	struct mandel_point param;
	char preal_buf[1024], pimag_buf[1024];
};


struct _FractalTypeDialogPrivate {
	GtkListStore *type_list, *repres_list;
	GtkWidget *type_input;
	GtkWidget *area_creal_input;
	GtkWidget *area_cimag_input;
	GtkWidget *area_magf_input;
	GtkWidget *maxiter_input;
	GtkWidget *type_param_notebook;
	GtkWidget *repres_input;
	GtkWidget *repres_notebook;
	GtkWidget *repres_log_base_input;
	struct mandel_area area;
	char creal_buf[1024], cimag_buf[1024], magf_buf[1024];
	struct gui_fractal_type_dynamic frac_types[FRACTAL_MAX];
	bool disposed;
};


static void type_dlg_type_updated (FractalTypeDialog *dlg, gpointer data);
static void type_dlg_repres_updated (FractalTypeDialog *dlg, gpointer data);
static void type_dlg_defaults_clicked (FractalTypeDialog *dlg, gpointer data);

static bool repres_supported (const struct gui_fractal_type_dynamic *ftype, fractal_repres_t repres);

static struct gui_type_param *create_mandelbrot_param (GtkSizeGroup *label_size_group, GtkSizeGroup *input_size_group);
static void mandelbrot_dispose (struct gui_type_param *param);
static void mandelbrot_set_param (FractalTypeDialog *dlg, struct gui_type_param *gui_param, const void *param);
static void mandelbrot_get_param (FractalTypeDialog *dlg, struct gui_type_param *gui_param, void *param);

static struct gui_type_param *create_julia_param (GtkSizeGroup *label_size_group, GtkSizeGroup *input_size_group);
static void julia_dispose (struct gui_type_param *param);
static void julia_finalize (struct gui_type_param *param);
static void julia_set_param (FractalTypeDialog *dlg, struct gui_type_param *gui_param, const void *param);
static void julia_get_param (FractalTypeDialog *dlg, struct gui_type_param *gui_param, void *param);

static void type_dlg_set_maxiter (FractalTypeDialog *dlg, unsigned maxiter);
static unsigned type_dlg_get_maxiter (FractalTypeDialog *dlg);

static void fractal_type_dialog_class_init (gpointer g_class, gpointer data);
static void fractal_type_dialog_init (GTypeInstance *dlg, gpointer g_class);

static void fractal_type_dialog_dispose (GObject *object);
static void fractal_type_dialog_finalize (GObject *object);

static fractal_type_t type_dlg_get_type (FractalTypeDialog *dlg);
static fractal_repres_t type_dlg_get_repres (FractalTypeDialog *dlg);


static struct gui_fractal_type gui_fractal_types[] = {
	{
		.flags		= GUI_FTYPE_HAS_MAXITER,
		.create_gui	= create_mandelbrot_param,
		.dispose	= mandelbrot_dispose,
		.set_param	= mandelbrot_set_param,
		.get_param	= mandelbrot_get_param
	},
	{
		.flags		= GUI_FTYPE_HAS_MAXITER,
		.create_gui	= create_julia_param,
		.dispose	= julia_dispose,
		.finalize	= julia_finalize,
		.set_param	= julia_set_param,
		.get_param	= julia_get_param
	}
};


GType
fractal_type_dialog_get_type (void)
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			.class_size		= sizeof (FractalTypeDialogClass),
			.class_init		= fractal_type_dialog_class_init,
			.instance_size	= sizeof (FractalTypeDialog),
			.instance_init	= fractal_type_dialog_init
		};
		type = g_type_register_static (GTK_TYPE_DIALOG, "FractalTypeDialog", &info, 0);
	}

	return type;
}


static void
fractal_type_dialog_class_init (gpointer g_class, gpointer data)
{
	G_OBJECT_CLASS (g_class)->dispose = fractal_type_dialog_dispose;
	G_OBJECT_CLASS (g_class)->finalize = fractal_type_dialog_finalize;
}


static void
fractal_type_dialog_init (GTypeInstance *instance, gpointer g_class)
{
	int i;

	FractalTypeDialog *dlg = FRACTAL_TYPE_DIALOG (instance);

	dlg->priv = malloc (sizeof (*dlg->priv));
	FractalTypeDialogPrivate *const priv = dlg->priv;
	priv->disposed = false;
	GtkBox *const dlg_vbox = GTK_BOX (GTK_DIALOG (dlg)->vbox), *vbox;
	GtkWidget *container, *widget, *frame, *notebook;
	GtkCellRenderer *renderer;

	GtkSizeGroup *label_size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);
	GtkSizeGroup *input_size_group = gtk_size_group_new (GTK_SIZE_GROUP_HORIZONTAL);

	gtk_window_set_title (GTK_WINDOW (dlg), "Fractal Type and Parameters");
	gtk_dialog_add_buttons (GTK_DIALOG (dlg), GTK_STOCK_APPLY, GTK_RESPONSE_APPLY, GTK_STOCK_OK, GTK_RESPONSE_ACCEPT, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL);

	/* --- Fractal Type --- */
	container = gtk_table_new (2, 2, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (container), 2);
	gtk_table_set_col_spacings (GTK_TABLE (container), 2);
	gtk_box_pack_start (dlg_vbox, container, FALSE, FALSE, 0);

	gtk_table_attach (GTK_TABLE (container), my_gtk_label_new ("Fractal Type", label_size_group), 0, 1, 0, 1, 0, 0, 0, 0);

	priv->type_list = gtk_list_store_new (2, G_TYPE_INT, G_TYPE_STRING);
	GtkTreeIter iter[1];
	for (i = 0; i < FRACTAL_MAX; i++) {
		gtk_list_store_append (priv->type_list, iter);
		gtk_list_store_set (priv->type_list, iter, 0, i, -1);
		gtk_list_store_set (priv->type_list, iter, 1, fractal_type_by_id (i)->descr, -1);
	}
	widget = gtk_combo_box_new_with_model (GTK_TREE_MODEL (priv->type_list));
	gtk_size_group_add_widget (input_size_group, widget);
	renderer = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (widget), renderer, FALSE);
	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (widget), renderer, "text", 1);
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0); /* XXX */
	g_signal_connect_object (widget, "changed", (GCallback) type_dlg_type_updated, dlg, G_CONNECT_SWAPPED);
	gtk_table_attach (GTK_TABLE (container), widget, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
	priv->type_input = widget;
	g_object_ref (priv->type_input);

	widget = gtk_button_new_with_label ("Default Values");
	gtk_size_group_add_widget (input_size_group, widget);
	g_signal_connect_object (widget, "clicked", (GCallback) type_dlg_defaults_clicked, dlg, G_CONNECT_SWAPPED);
	gtk_table_attach (GTK_TABLE (container), widget, 1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 0, 0);

	/* --- Area --- */
	frame = gtk_frame_new ("Area");
	gtk_box_pack_start (dlg_vbox, frame, FALSE, FALSE, 0);

	container = gtk_table_new (2, 3, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (container), 2);
	gtk_table_set_col_spacings (GTK_TABLE (container), 2);
	gtk_container_add (GTK_CONTAINER (frame), container);

	gtk_table_attach (GTK_TABLE (container), my_gtk_label_new ("Center Real", label_size_group), 0, 1, 0, 1, 0, 0, 0, 0);

	widget = gtk_entry_new ();
	gtk_size_group_add_widget (input_size_group, widget);
	gtk_table_attach (GTK_TABLE (container), widget, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
	priv->area_creal_input = widget;
	g_object_ref (priv->area_creal_input);

	gtk_table_attach (GTK_TABLE (container), my_gtk_label_new ("Center Imag", label_size_group), 0, 1, 1, 2, 0, 0, 0, 0);

	widget = gtk_entry_new ();
	gtk_size_group_add_widget (input_size_group, widget);
	gtk_table_attach (GTK_TABLE (container), widget, 1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 0, 0);
	priv->area_cimag_input = widget;
	g_object_ref (priv->area_cimag_input);

	gtk_table_attach (GTK_TABLE (container), my_gtk_label_new ("Magnification", label_size_group), 0, 1, 2, 3, 0, 0, 0, 0);

	widget = gtk_entry_new ();
	gtk_size_group_add_widget (input_size_group, widget);
	gtk_table_attach (GTK_TABLE (container), widget, 1, 2, 2, 3, GTK_EXPAND | GTK_FILL, 0, 0, 0);
	priv->area_magf_input = widget;
	g_object_ref (priv->area_magf_input);

	/* --- General Parameters --- */
	frame = gtk_frame_new ("General Parameters");
	gtk_box_pack_start (dlg_vbox, frame, FALSE, FALSE, 0);

	container = gtk_table_new (2, 1, FALSE);
	gtk_table_set_row_spacings (GTK_TABLE (container), 2);
	gtk_table_set_col_spacings (GTK_TABLE (container), 2);
	gtk_container_add (GTK_CONTAINER (frame), container);

	gtk_table_attach (GTK_TABLE (container), my_gtk_label_new ("Max Iterations", label_size_group), 0, 1, 0, 1, 0, 0, 0, 0);

	widget = gtk_spin_button_new_with_range (1.0, 1000000000000.0, 100.0);
	gtk_size_group_add_widget (input_size_group, widget);
	gtk_table_attach (GTK_TABLE (container), widget, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
	priv->maxiter_input = widget;
	g_object_ref (priv->maxiter_input);

	/* --- Type-Specific Parameters --- */
	frame = gtk_frame_new ("Type-Specific Parameters");
	gtk_box_pack_start (dlg_vbox, frame, FALSE, FALSE, 0);

	container = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (container), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (container), FALSE);
	gtk_container_add (GTK_CONTAINER (frame), container);
	priv->type_param_notebook = container;
	g_object_ref (priv->type_param_notebook);

	for (i = 0; i < FRACTAL_MAX; i++) {
		priv->frac_types[i].repres_count = fractal_supported_representations (fractal_type_by_id (i), priv->frac_types[i].repres);
		priv->frac_types[i].gui = gui_fractal_types[i].create_gui (label_size_group, input_size_group);
		gtk_notebook_append_page (GTK_NOTEBOOK (container), priv->frac_types[i].gui->main_widget, NULL);
	}

	/* --- Representation --- */
	frame = gtk_frame_new ("Representation Settings");
	gtk_box_pack_start (dlg_vbox, frame, FALSE, FALSE, 0);

	vbox = GTK_BOX (gtk_vbox_new (FALSE, 2));
	gtk_container_add (GTK_CONTAINER (frame), GTK_WIDGET (vbox));

	container = gtk_hbox_new (FALSE, 2);
	gtk_box_pack_start (vbox, container, FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (container), my_gtk_label_new ("Representation Method", label_size_group), FALSE, FALSE, 0);

	priv->repres_list = gtk_list_store_new (3, G_TYPE_INT, G_TYPE_STRING, G_TYPE_BOOLEAN);
	gtk_list_store_append (priv->repres_list, iter);
	gtk_list_store_set (priv->repres_list, iter, 0, REPRES_ESCAPE, 1, "Escape-Iterations", 2, (gboolean) TRUE, -1);
	gtk_list_store_append (priv->repres_list, iter);
	gtk_list_store_set (priv->repres_list, iter, 0, REPRES_ESCAPE_LOG, 1, "Escape-Iterations (Logarithmic)", 2, (gboolean) TRUE, -1);
	gtk_list_store_append (priv->repres_list, iter);
	gtk_list_store_set (priv->repres_list, iter, 0, REPRES_DISTANCE, 1, "Distance", 2, (gboolean) TRUE, -1);

	renderer = gtk_cell_renderer_text_new ();
	widget = gtk_combo_box_new_with_model (GTK_TREE_MODEL (priv->repres_list));
	gtk_size_group_add_widget (input_size_group, widget);
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (widget), renderer, FALSE);
	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (widget), renderer, "text", 1);
	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (widget), renderer, "sensitive", 2);
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), REPRES_ESCAPE); /* XXX */
	g_signal_connect_object (widget, "changed", (GCallback) type_dlg_repres_updated, dlg, G_CONNECT_SWAPPED);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	priv->repres_input = widget;
	g_object_ref (priv->repres_input);

	notebook = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (notebook), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (notebook), FALSE);
	gtk_box_pack_start (GTK_BOX (vbox), notebook, FALSE, FALSE, 0);
	priv->repres_notebook = notebook;
	g_object_ref (priv->repres_notebook);

	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), gtk_label_new (NULL), NULL);

	container = gtk_hbox_new (FALSE, 2);
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), container, NULL);

	gtk_box_pack_start (GTK_BOX (container), my_gtk_label_new ("Base of Logarithm", label_size_group), FALSE, FALSE, 0);

	widget = gtk_spin_button_new_with_range (1.0, 100000.0, 0.001);
	gtk_size_group_add_widget (input_size_group, widget);
	gtk_box_pack_start (GTK_BOX (container), widget, TRUE, TRUE, 0);
	priv->repres_log_base_input = widget;
	g_object_ref (priv->repres_log_base_input);

	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), gtk_label_new (NULL), NULL);

	gtk_widget_show_all (GTK_WIDGET (dlg_vbox));

	mpf_init (priv->area.center.real);
	mpf_init (priv->area.center.imag);
	mpf_init (priv->area.magf);

	g_object_unref (label_size_group);
	g_object_unref (input_size_group);
}


FractalTypeDialog *
fractal_type_dialog_new (GtkWindow *parent)
{
	FractalTypeDialog *dlg = FRACTAL_TYPE_DIALOG (g_object_new (TYPE_FRACTAL_TYPE_DIALOG, NULL));
	if (parent != NULL)
		gtk_window_set_transient_for (GTK_WINDOW (dlg), parent);
	return dlg;
}


static void
type_dlg_type_updated (FractalTypeDialog *dlg, gpointer data)
{
	FractalTypeDialogPrivate *priv = dlg->priv;
	GtkTreeIter iter[1];
	fractal_type_t type = type_dlg_get_type (dlg);
	const bool has_maxiter = (gui_fractal_types[type].flags & GUI_FTYPE_HAS_MAXITER) != 0;
	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->type_param_notebook), type);
	gtk_widget_set_sensitive (priv->maxiter_input, has_maxiter);
	gtk_tree_model_get_iter_first (GTK_TREE_MODEL (priv->repres_list), iter);
	do {
		gint gi_repres;
		gtk_tree_model_get (GTK_TREE_MODEL (priv->repres_list), iter, 0, &gi_repres, -1);
		gtk_list_store_set (priv->repres_list, iter, 2, (gboolean) repres_supported (&priv->frac_types[type], (fractal_repres_t) gi_repres), -1);
	} while (gtk_tree_model_iter_next (GTK_TREE_MODEL (priv->repres_list), iter));
	struct mandeldata md[1];
	mandeldata_init (md, fractal_type_by_id (type));
	mandeldata_set_defaults (md);
	fractal_type_dialog_set_mandeldata (dlg, md);
	mandeldata_clear (md);
}


static void
type_dlg_repres_updated (FractalTypeDialog *dlg, gpointer data)
{
	FractalTypeDialogPrivate *priv = dlg->priv;
	GtkTreeIter iter[1];
	gint gi;
	gtk_combo_box_get_active_iter (GTK_COMBO_BOX (priv->repres_input), iter);
	gtk_tree_model_get (GTK_TREE_MODEL (priv->repres_list), iter, 0, &gi, -1);
	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->repres_notebook), gi);
}


static void
type_dlg_defaults_clicked (FractalTypeDialog *dlg, gpointer data)
{
	struct mandeldata md[1];
	fractal_type_t type = type_dlg_get_type (dlg);
	mandeldata_init (md, fractal_type_by_id (type));
	mandeldata_set_defaults (md);
	fractal_type_dialog_set_mandeldata (dlg, md);
	mandeldata_clear (md);
}


static void
mandelbrot_set_param (FractalTypeDialog *dlg, struct gui_type_param *gui_param_, const void *param_)
{
	struct gui_mandelbrot_param *gui_param = (struct gui_mandelbrot_param *) gui_param_;
	const struct mandelbrot_param *param = (const struct mandelbrot_param *) param_;
	type_dlg_set_maxiter (dlg, param->mjparam.maxiter);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (gui_param->zpower_input), param->mjparam.zpower);
}


static void
mandelbrot_get_param (FractalTypeDialog *dlg, struct gui_type_param *gui_param_, void *param_)
{
	struct gui_mandelbrot_param *gui_param = (struct gui_mandelbrot_param *) gui_param_;
	struct mandelbrot_param *param = (struct mandelbrot_param *) param_;
	param->mjparam.maxiter = type_dlg_get_maxiter (dlg);
	param->mjparam.zpower = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (gui_param->zpower_input));
}


static void
julia_set_param (FractalTypeDialog *dlg, struct gui_type_param *gui_param_, const void *param_)
{
	struct gui_julia_param *gui_param = (struct gui_julia_param *) gui_param_;
	const struct julia_param *param = (const struct julia_param *) param_;
	type_dlg_set_maxiter (dlg, param->mjparam.maxiter);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (gui_param->zpower_input), param->mjparam.zpower);
	mpf_set (gui_param->param.real, param->param.real);
	mpf_set (gui_param->param.imag, param->param.imag);
	gmp_snprintf (gui_param->preal_buf, sizeof (gui_param->preal_buf), "%.20Ff", gui_param->param.real);
	gmp_snprintf (gui_param->pimag_buf, sizeof (gui_param->pimag_buf), "%.20Ff", gui_param->param.imag);
	gtk_entry_set_text (GTK_ENTRY (gui_param->preal_input), gui_param->preal_buf);
	gtk_entry_set_text (GTK_ENTRY (gui_param->pimag_input), gui_param->pimag_buf);
}


static void
julia_get_param (FractalTypeDialog *dlg, struct gui_type_param *gui_param_, void *param_)
{
	struct gui_julia_param *gui_param = (struct gui_julia_param *) gui_param_;
	struct julia_param *param = (struct julia_param *) param_;
	param->mjparam.maxiter = type_dlg_get_maxiter (dlg);
	param->mjparam.zpower = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (gui_param->zpower_input));
	mpf_from_entry (GTK_ENTRY (gui_param->preal_input), param->param.real, gui_param->param.real, gui_param->preal_buf);
	mpf_from_entry (GTK_ENTRY (gui_param->pimag_input), param->param.imag, gui_param->param.imag, gui_param->pimag_buf);
}


static void
type_dlg_set_maxiter (FractalTypeDialog *dlg, unsigned maxiter)
{
	FractalTypeDialogPrivate *priv = dlg->priv;
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->maxiter_input), maxiter);
}


static unsigned
type_dlg_get_maxiter (FractalTypeDialog *dlg)
{
	FractalTypeDialogPrivate *priv = dlg->priv;
	return gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (priv->maxiter_input));
}


void
fractal_type_dialog_set_mandeldata (FractalTypeDialog *dlg, const struct mandeldata *md)
{
	FractalTypeDialogPrivate *priv = dlg->priv;
	fractal_type_t type = md->type->type;
	fractal_repres_t repres = md->repres.repres;
	gtk_combo_box_set_active (GTK_COMBO_BOX (priv->type_input), type);

	mpf_set (priv->area.center.real, md->area.center.real);
	mpf_set (priv->area.center.imag, md->area.center.imag);
	mpf_set (priv->area.magf, md->area.magf);

	if (center_coords_to_string (priv->area.center.real, priv->area.center.imag, priv->area.magf, priv->creal_buf, priv->cimag_buf, priv->magf_buf, 1024) < 0)
		fprintf (stderr, "* ERROR: center_coords_to_string() failed in %s line %d\n", __FILE__, __LINE__);

	gtk_entry_set_text (GTK_ENTRY (priv->area_creal_input), priv->creal_buf);
	gtk_entry_set_text (GTK_ENTRY (priv->area_cimag_input), priv->cimag_buf);
	gtk_entry_set_text (GTK_ENTRY (priv->area_magf_input), priv->magf_buf);

	gui_fractal_types[type].set_param (dlg, priv->frac_types[type].gui, md->type_param);
	gtk_combo_box_set_active (GTK_COMBO_BOX (priv->repres_input), repres);
	switch (repres) {
		case REPRES_ESCAPE:
			break;
		case REPRES_ESCAPE_LOG:
			gtk_spin_button_set_value (GTK_SPIN_BUTTON (priv->repres_log_base_input), md->repres.params.log_base);
			break;
		case REPRES_DISTANCE:
			break;
		default:
			fprintf (stderr, "* ERROR: Unknown representation type %d in %s line %d\n", (int) repres, __FILE__, __LINE__);
			break;
	}
}


void
fractal_type_dialog_get_mandeldata (FractalTypeDialog *dlg, struct mandeldata *md)
{
	FractalTypeDialogPrivate *priv = dlg->priv;
	const struct fractal_type *type = fractal_type_by_id (type_dlg_get_type (dlg));
	mandeldata_init (md, type);
	mpf_from_entry (GTK_ENTRY (priv->area_creal_input), md->area.center.real, priv->area.center.real, priv->creal_buf);
	mpf_from_entry (GTK_ENTRY (priv->area_cimag_input), md->area.center.imag, priv->area.center.imag, priv->cimag_buf);
	mpf_from_entry (GTK_ENTRY (priv->area_magf_input), md->area.magf, priv->area.magf, priv->magf_buf);
	gui_fractal_types[type->type].get_param (dlg, priv->frac_types[type->type].gui, md->type_param);
	md->repres.repres = type_dlg_get_repres (dlg);
	switch (md->repres.repres) {
		case REPRES_ESCAPE:
			break;
		case REPRES_ESCAPE_LOG:
			md->repres.params.log_base = gtk_spin_button_get_value (GTK_SPIN_BUTTON (priv->repres_log_base_input));
			break;
		case REPRES_DISTANCE:
			break;
		default:
			fprintf (stderr, "* ERROR: Unknown representation %d in %s line %d\n", (int) md->repres.repres, __FILE__, __LINE__);
			break;
	}
}


static bool
repres_supported (const struct gui_fractal_type_dynamic *ftype, fractal_repres_t repres)
{
	int i;
	for (i = 0; i < ftype->repres_count; i++)
		if (ftype->repres[i] == repres)
			return true;
	return false;
}


static struct gui_type_param *
create_mandelbrot_param (GtkSizeGroup *label_size_group, GtkSizeGroup *input_size_group)
{
	struct gui_mandelbrot_param *par = malloc (sizeof (*par));
	GtkTable *table;
	memset (par, 0, sizeof (*par));
	table = GTK_TABLE (gtk_table_new (2, 1, FALSE));
	gtk_table_set_row_spacings (table, 2);
	gtk_table_set_col_spacings (table, 2);
	par->ftype.main_widget = GTK_WIDGET (table);
	g_object_ref (par->ftype.main_widget);

	gtk_table_attach (table, my_gtk_label_new ("Power of Z", label_size_group), 0, 1, 0, 1, GTK_FILL, 0, 0, 0);
	par->zpower_input = gtk_spin_button_new_with_range (2.0, 100000.0, 1.0);
	gtk_size_group_add_widget (input_size_group, par->zpower_input);
	gtk_table_attach (table, par->zpower_input, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
	g_object_ref (par->zpower_input);
	return &par->ftype;
}


static struct gui_type_param *
create_julia_param (GtkSizeGroup *label_size_group, GtkSizeGroup *input_size_group)
{
	struct gui_julia_param *par = malloc (sizeof (*par));
	memset (par, 0, sizeof (*par));
	GtkWidget *widget;
	GtkTable *table = GTK_TABLE (gtk_table_new (2, 3, FALSE));
	gtk_table_set_row_spacings (table, 2);
	gtk_table_set_col_spacings (table, 2);
	par->ftype.main_widget = GTK_WIDGET (table);
	g_object_ref (par->ftype.main_widget);

	gtk_table_attach (table, my_gtk_label_new ("Power of Z", label_size_group), 0, 1, 0, 1, GTK_FILL, 0, 0, 0);

	widget = gtk_spin_button_new_with_range (2.0, 100000.0, 1.0);
	gtk_size_group_add_widget (input_size_group, widget);
	gtk_table_attach (table, widget, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 0, 0);
	par->zpower_input = widget;
	g_object_ref (par->zpower_input);

	gtk_table_attach (table, my_gtk_label_new ("Real Part of Parameter", label_size_group), 0, 1, 1, 2, GTK_FILL, 0, 0, 0);

	widget = gtk_entry_new ();
	gtk_size_group_add_widget (input_size_group, widget);
	gtk_table_attach (table, widget, 1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 0, 0);
	par->preal_input = widget;
	g_object_ref (par->preal_input);

	gtk_table_attach (table, my_gtk_label_new ("Imaginary Part of Parameter", label_size_group), 0, 1, 2, 3, GTK_FILL, 0, 0, 0);

	widget = gtk_entry_new ();
	gtk_size_group_add_widget (input_size_group, widget);
	gtk_table_attach (table, widget, 1, 2, 2, 3, GTK_EXPAND | GTK_FILL, 0, 0, 0);
	par->pimag_input = widget;
	g_object_ref (par->pimag_input);

	mpf_init (par->param.real);
	mpf_init (par->param.imag);
	return &par->ftype;
}


static fractal_type_t
type_dlg_get_type (FractalTypeDialog *dlg)
{
	FractalTypeDialogPrivate *priv = dlg->priv;
	GtkTreeIter iter[1];
	gint gi;
	gtk_combo_box_get_active_iter (GTK_COMBO_BOX (priv->type_input), iter);
	gtk_tree_model_get (GTK_TREE_MODEL (priv->type_list), iter, 0, &gi, -1);
	return (fractal_type_t) gi;
}


static fractal_repres_t
type_dlg_get_repres (FractalTypeDialog *dlg)
{
	FractalTypeDialogPrivate *priv = dlg->priv;
	GtkTreeIter iter[1];
	gint gi;
	gtk_combo_box_get_active_iter (GTK_COMBO_BOX (priv->repres_input), iter);
	gtk_tree_model_get (GTK_TREE_MODEL (priv->repres_list), iter, 0, &gi, -1);
	return (fractal_repres_t) gi;
}


static void
fractal_type_dialog_dispose (GObject *object)
{
	//fprintf (stderr, "* DEBUG: disposing type dialog\n");
	int i;
	FractalTypeDialog *const dlg = FRACTAL_TYPE_DIALOG (object);
	FractalTypeDialogPrivate *const priv = dlg->priv;
	if (!priv->disposed) {
		g_object_unref (priv->type_list);
		g_object_unref (priv->repres_list);
		g_object_unref (priv->type_input);
		g_object_unref (priv->area_creal_input);
		g_object_unref (priv->area_cimag_input);
		g_object_unref (priv->area_magf_input);
		g_object_unref (priv->maxiter_input);
		g_object_unref (priv->type_param_notebook);
		g_object_unref (priv->repres_input);
		g_object_unref (priv->repres_notebook);
		g_object_unref (priv->repres_log_base_input);
		for (i = 0; i < FRACTAL_MAX; i++)
			if (gui_fractal_types[i].dispose != NULL)
				gui_fractal_types[i].dispose (priv->frac_types[i].gui);
		priv->disposed = true;
	}
	G_OBJECT_CLASS (g_type_class_peek_parent (G_OBJECT_GET_CLASS (object)))->dispose (object);
}


static void
fractal_type_dialog_finalize (GObject *object)
{
	//fprintf (stderr, "* DEBUG: finalizing type dialog\n");
	int i;
	FractalTypeDialog *const dlg = FRACTAL_TYPE_DIALOG (object);
	FractalTypeDialogPrivate *const priv = dlg->priv;
	mandel_area_clear (&priv->area);
	for (i = 0; i < FRACTAL_MAX; i++) {
		if (gui_fractal_types[i].finalize != NULL)
			gui_fractal_types[i].finalize (priv->frac_types[i].gui);
		free (priv->frac_types[i].gui);
	}
	free (priv);
	G_OBJECT_CLASS (g_type_class_peek_parent (G_OBJECT_GET_CLASS (object)))->finalize (object);
}


static void
mandelbrot_dispose (struct gui_type_param *param)
{
	struct gui_mandelbrot_param *mparam = (struct gui_mandelbrot_param *) param;
	g_object_unref (param->main_widget);
	g_object_unref (mparam->zpower_input);
}


static void
julia_dispose (struct gui_type_param *param)
{
	struct gui_julia_param *jparam = (struct gui_julia_param *) param;
	g_object_unref (param->main_widget);
	g_object_unref (jparam->zpower_input);
	g_object_unref (jparam->preal_input);
	g_object_unref (jparam->pimag_input);
}


static void
julia_finalize (struct gui_type_param *param)
{
	struct gui_julia_param *jparam = (struct gui_julia_param *) param;
	mandel_point_clear (&jparam->param);
}
