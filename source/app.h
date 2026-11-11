#ifndef APP_H
#define APP_H

#include "core.h"

struct App;

/*---------------------------------------------------------------------
 * Application interface. Each built-in app exposes a const App vtable
 * and owns its own state (stored in Window::app_state). The shell never
 * reaches into app-specific fields.
 *-------------------------------------------------------------------*/
typedef void (*AppResetFn)(Window *w);
typedef void (*AppDrawFn)(uint8_t *fb, Window *w, int focused);
typedef void (*AppClickFn)(Window *w, int mx, int my);
typedef void (*AppInputFn)(Window *w, char c);
typedef void (*AppUpdateFn)(Window *w);
typedef int  (*AppLoadFileFn)(Window *w, const char *fname);
typedef int  (*AppSecondaryClickFn)(Window *w, int mx, int my);
typedef void (*AppBottomDrawFn)(uint8_t *bot, Window *w);
typedef int  (*AppBottomHitFn)(int tx, int ty, int *out_a, int *out_b);
typedef void (*AppBottomActionFn)(Window *w, int kind, int a, int b);

typedef struct App App;

struct App {
    const char *name;
    AppResetFn           reset;
    AppDrawFn            draw;
    AppClickFn           click;
    AppInputFn           input;
    AppUpdateFn          update;
    AppLoadFileFn        load_file;
    AppSecondaryClickFn  secondary_click;
    AppBottomDrawFn      bottom_draw;
    AppBottomHitFn       bottom_hit;
    AppBottomActionFn    bottom_action;
};

/*---------------------------------------------------------------------
 * App registry (defined in app.c, instances in each app_*.c)
 *-------------------------------------------------------------------*/
extern const App g_app_notepad;
extern const App g_app_calc;
extern const App g_app_about;
extern const App g_app_files;
extern const App g_app_paint;
extern const App g_app_mines;
extern const App g_app_basic;

const App *app_for_kind(WKind k);

/* Launch (or re-launch) an application window. */
void open_app(WKind k);

#endif /* APP_H */
