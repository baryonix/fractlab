#ifndef _GTKMANDEL_GUI_TYPEDLG_H
#define _GTKMANDEL_GUI_TYPEDLG_H

struct _FractalTypeDialog;
struct _FractalTypeDialogPrivate;
struct _FractalTypeDialogClass;

typedef struct _FractalTypeDialog FractalTypeDialog;
typedef struct _FractalTypeDialogPrivate FractalTypeDialogPrivate;
typedef struct _FractalTypeDialogClass FractalTypeDialogClass;

struct _FractalTypeDialog {
	GtkDialog dialog;
	FractalTypeDialogPrivate *priv;
};

struct _FractalTypeDialogClass {
	GtkDialogClass dialog_class;
};

GType fractal_type_dialog_get_type (void);

FractalTypeDialog *fractal_type_dialog_new (GtkWindow *parent);

void fractal_type_dialog_get_mandeldata  (FractalTypeDialog *dlg, struct mandeldata *md);
void fractal_type_dialog_set_mandeldata (FractalTypeDialog *dlg, const struct mandeldata *md);

#define TYPE_FRACTAL_TYPE_DIALOG (fractal_type_dialog_get_type ())
#define FRACTAL_TYPE_DIALOG(obj) (GTK_CHECK_CAST ((obj), fractal_type_dialog_get_type (), FractalTypeDialog))
#define FRACTAL_TYPE_DIALOG_CLASS(cls) (GTK_CHECK_CLASS_CAST ((cls), fractal_type_dialog_get_type (), FractalTypeDialog))
#define IS_FRACTAL_TYPE_DIALOG(obj) (GET_CHECK_TYPE ((obj), fractal_type_dialog_get_type ()))
#define FRACTAL_TYPE_DIALOG_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), FractalTypeDialog, FractalTypeDialogClass))

#endif /* _GTKMANDEL_GUI_TYPEDLG_H */
