#ifndef TERMINAL_H
#define TERMINAL_H

#include "core.h"

#define TERMINAL_MAX_COLS 64
#define TERMINAL_MAX_ROWS 34

typedef struct {
    int cols, rows;
    int cur_x, cur_y;
    int cur_fg, cur_bg;
    char ch[TERMINAL_MAX_ROWS][TERMINAL_MAX_COLS];
    uint8_t fg[TERMINAL_MAX_ROWS][TERMINAL_MAX_COLS];
} Terminal;

void terminal_window_init(Window *w, WKind kind, const char *title, void *state);
void terminal_init(Terminal *term, const Window *w);
void terminal_clear(Terminal *term);
void terminal_putc(Terminal *term, char c);
void terminal_print(Terminal *term, const char *text);
int terminal_edit_input(Terminal *term, char *buffer, int *length,
                        int capacity, char c);
void terminal_draw(uint8_t *fb, const Window *w, const Terminal *term,
                   const Color *palette, Color default_fg, Color default_bg,
                   int show_caret, const char *status);

#endif
