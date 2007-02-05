#ifndef _GTKMANDEL_GUI_MAINWIN_H
#define _GTKMANDEL_GUI_MAINWIN_H

struct _FractalMainWindow;
struct _FractalMainWindowClass;
struct _FractalMainWindowPrivate;

typedef struct _FractalMainWindow FractalMainWindow;
typedef struct _FractalMainWindowClass FractalMainWindowClass;
typedef struct _FractalMainWindowPrivate FractalMainWindowPrivate;

struct _FractalMainWindow {
	GtkWindow window;
	FractalMainWindowPrivate *priv;
	const struct mandeldata *md;
};

struct _FractalMainWindowClass {
	GtkWindowClass window_class;
	render_method_t render_methods[RM_MAX];
	guint mandeldata_updated_signal;
	guint load_coords_signal;
	guint save_coords_signal;
	guint info_dlg_signal;
	guint type_dlg_signal;
	guint about_dlg_signal;
};

GType fractal_main_window_get_class (void);
FractalMainWindow *fractal_main_window_new (void);
const struct mandeldata *fractal_main_window_get_mandeldata (FractalMainWindow *win);
void fractal_main_window_set_mandeldata (FractalMainWindow *win, const struct mandeldata *md);
void fractal_main_window_restart (FractalMainWindow *win);

#define TYPE_FRACTAL_MAIN_WINDOW (fractal_main_window_get_type ())
#define FRACTAL_MAIN_WINDOW(obj) (GTK_CHECK_CAST ((obj), fractal_main_window_get_type (), FractalMainWindow))
#define FRACTAL_MAIN_WINDOW_CLASS(cls) (GTK_CHECK_CLASS_CAST ((cls), fractal_main_window_get_type (), FractalMainWindowClass))
#define IS_FRACTAL_MAIN_WINDOW(obj) (GET_CHECK_TYPE ((obj), fractal_main_window_get_type ()))
#define FRACTAL_MAIN_WINDOW_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), FractalMainWindow, FractalMainWindowClass))

#endif /* _GTKMANDEL_GUI_MAINWIN_H */
