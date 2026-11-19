#include "app.h"

const App *app_for_kind(WKind k) {
    switch (k) {
        case WK_NOTEPAD: return &g_app_notepad;
        case WK_CALC:    return &g_app_calc;
        case WK_ABOUT:   return &g_app_about;
        case WK_FILES:   return &g_app_files;
        case WK_PAINT:   return &g_app_paint;
        case WK_MINES:   return &g_app_mines;
        case WK_BASIC:   return &g_app_basic;
        case WK_PHOTO:   return &g_app_photo;
        case WK_PONG:    return &g_app_pong;
        case WK_GUESS:   return &g_app_guess;
    }
    return NULL;
}

void open_app(WKind k) {
    Window *w = &g_wins[k];
    const App *app = app_for_kind(k);
    if (!app || !app->reset) return;
    w->app = app;
    app->reset(w);
}
