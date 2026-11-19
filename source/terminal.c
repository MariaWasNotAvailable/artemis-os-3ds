#include "terminal.h"

#include <stdio.h>
#include <string.h>

void terminal_window_init(Window *w, WKind kind, const char *title, void *state) {
    w->kind = kind;
    w->open = 1;
    w->x = 8;
    w->y = 8;
    w->w = 384;
    w->h = 200;
    snprintf(w->title, sizeof(w->title), "%s", title);
    w->app_state = state;
}

void terminal_init(Terminal *term, const Window *w) {
    memset(term, 0, sizeof(*term));
    term->cur_fg = 15;
    term->cur_bg = 0;

    int cols = (w->w - 10) / 8;
    int rows = (w->h - 54) / 10;
    term->cols = cols > TERMINAL_MAX_COLS ? TERMINAL_MAX_COLS : cols;
    term->rows = rows > TERMINAL_MAX_ROWS ? TERMINAL_MAX_ROWS : rows;
    if (term->cols < 1) term->cols = 1;
    if (term->rows < 1) term->rows = 1;
}

void terminal_clear(Terminal *term) {
    for (int y = 0; y < term->rows; y++) {
        memset(term->ch[y], ' ', term->cols);
        memset(term->fg[y], (uint8_t)term->cur_fg, term->cols);
    }
    term->cur_x = 0;
    term->cur_y = 0;
}

static void terminal_scroll(Terminal *term) {
    for (int y = 1; y < term->rows; y++) {
        memcpy(term->ch[y - 1], term->ch[y], term->cols);
        memcpy(term->fg[y - 1], term->fg[y], term->cols);
    }
    memset(term->ch[term->rows - 1], ' ', term->cols);
    memset(term->fg[term->rows - 1], (uint8_t)term->cur_fg, term->cols);
}

void terminal_putc(Terminal *term, char c) {
    if (c == '\n') {
        term->cur_x = 0;
        term->cur_y++;
    } else if (c == '\r') {
        term->cur_x = 0;
    } else if (c == '\b') {
        if (term->cur_x > 0) {
            term->cur_x--;
            term->ch[term->cur_y][term->cur_x] = ' ';
        } else if (term->cur_y > 0) {
            term->cur_y--;
            term->cur_x = term->cols - 1;
            term->ch[term->cur_y][term->cur_x] = ' ';
        }
    } else {
        if (term->cur_x >= term->cols) {
            term->cur_x = 0;
            term->cur_y++;
        }
        if (term->cur_y >= term->rows) {
            terminal_scroll(term);
            term->cur_y = term->rows - 1;
        }
        term->ch[term->cur_y][term->cur_x] = c;
        term->fg[term->cur_y][term->cur_x] = (uint8_t)term->cur_fg;
        term->cur_x++;
    }
    if (term->cur_y >= term->rows) {
        terminal_scroll(term);
        term->cur_y = term->rows - 1;
    }
}

void terminal_print(Terminal *term, const char *text) {
    while (*text) terminal_putc(term, *text++);
}

int terminal_edit_input(Terminal *term, char *buffer, int *length,
                        int capacity, char c) {
    if (c == '\b') {
        if (*length > 0) {
            (*length)--;
            buffer[*length] = '\0';
            terminal_putc(term, '\b');
        }
        return 1;
    }
    if (c >= 32 && *length < capacity - 1) {
        buffer[(*length)++] = c;
        buffer[*length] = '\0';
        terminal_putc(term, c);
        return 1;
    }
    return 0;
}

void terminal_draw(uint8_t *fb, const Window *w, const Terminal *term,
                   const Color *palette, Color default_fg, Color default_bg,
                   int show_caret, const char *status) {
    if (!term->cols || !term->rows) return;

    int x0 = w->x + 5;
    int y0 = w->y + 24;
    int client_w = w->w - 10;
    int client_h = w->h - 34;
    Color background = palette ? palette[term->cur_bg & 15] : default_bg;
    Color foreground = palette ? palette[15] : default_fg;
    fill_rect(fb, TOP_W, TOP_H, x0, y0, client_w, client_h, background);

    for (int y = 0; y < term->rows; y++) {
        for (int x = 0; x < term->cols; x++) {
            char c = term->ch[y][x];
            if (c == ' ') continue;
            Color color = palette ? palette[term->fg[y][x] & 15] : foreground;
            draw_char(fb, TOP_W, TOP_H, x0 + x * 8, y0 + y * 10, c, color);
        }
    }

    if (show_caret && caret_tick() < 30) {
        vline(fb, TOP_W, TOP_H, x0 + term->cur_x * 8,
              y0 + term->cur_y * 10, 8, foreground);
    }

    if (status && *status) {
        int iy = y0 + term->rows * 10 + 2;
        if (iy + 8 <= y0 + client_h)
            draw_text(fb, TOP_W, TOP_H, x0, iy, status, foreground);
    }
}
