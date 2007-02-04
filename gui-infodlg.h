#ifndef _GTKMANDEL_GUI_INFODLG_H
#define _GTKMANDEL_GUI_INFODLG_H

struct _FractalInfoDialog;
struct _FractalInfoDialogPrivate;
struct _FractalInfoDialogClass;

typedef struct _FractalInfoDialog FractalInfoDialog;
typedef struct _FractalInfoDialogPrivate FractalInfoDialogPrivate;
typedef struct _FractalInfoDialogClass FractalInfoDialogClass;

struct _FractalInfoDialog {
	GtkDialog dialog;
	FractalInfoDialogPrivate *priv;
};

struct _FractalInfoDialogClass {
	GtkDialogClass dialog_class;
};

GType fractal_info_dialog_get_type (void);

FractalInfoDialog *fractal_info_dialog_new (GtkWindow *parent);

void fractal_info_dialog_set_mandeldata (FractalInfoDialog *dlg, const struct mandeldata *md, double aspect);

#define TYPE_FRACTAL_INFO_DIALOG (fractal_info_dialog_get_type ())
#define FRACTAL_INFO_DIALOG(obj) (GTK_CHECK_CAST ((obj), fractal_info_dialog_get_type (), FractalInfoDialog))
#define FRACTAL_INFO_DIALOG_CLASS(cls) (GTK_CHECK_CLASS_CAST ((cls), fractal_info_dialog_get_type (), FractalInfoDialog))
#define IS_FRACTAL_INFO_DIALOG(obj) (GET_CHECK_TYPE ((obj), fractal_info_dialog_get_type ()))
#define FRACTAL_INFO_DIALOG_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), FractalInfoDialog, FractalInfoDialogClass))


#endif /* _GTKMANDEL_GUI_INFODLG_H */
