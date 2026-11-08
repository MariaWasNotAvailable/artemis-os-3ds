#include "app.h"
#include "icons.h"
#include "audio.h"

#include <string.h>

typedef struct {
    char text[2048];
    int  text_len;
} NotepadState;

static NotepadState g_notepad_state;

/*---------------------------------------------------------------------
 * Helpers
 *-------------------------------------------------------------------*/
static void notepad_layout(Window *w,
                           int *tb_x, int *tb_y, int *tb_w, int *tb_h,
                           int *fn_x, int *fn_w,
                           int *bn_x, int *bn_w, int *bn_gap,
                           int *bd_x, int *bd_y, int *bd_w, int *bd_h) {
    *tb_x = w->x + 5;
    *tb_y = w->y + 20;
    *tb_w = w->w - 10;
    *tb_h = 15;

    int btn_w = 28;
    int gap   = 2;
    int btns_total = 3 * btn_w + 2 * gap;
    *fn_x = *tb_x;
    *fn_w = *tb_w - btns_total - 3;
    *bn_x = *fn_x + *fn_w + 3;
    *bn_w = btn_w;
    *bn_gap = gap;

    *bd_x = w->x + 5;
    *bd_y = *tb_y + *tb_h + 2;
    *bd_w = w->w - 10;
    *bd_h = w->h - (*bd_y - w->y) - 5;
}

/*---------------------------------------------------------------------
 * Input
 *-------------------------------------------------------------------*/
static void notepad_putc(Window *w, char c) {
    NotepadState *s = (NotepadState *)w->app_state;

    if (w->focus_filename) {
        if (c == '\b') {
            int n = (int)strlen(w->filename);
            if (n > 0) w->filename[n - 1] = '\0';
            return;
        }
        if (c == '\n') { w->focus_filename = 0; return; }
        if (!is_valid_filename_char(c)) return;
        int n = (int)strlen(w->filename);
        if (n < FNAME_MAX - 1) {
            w->filename[n] = c;
            w->filename[n + 1] = '\0';
        }
        return;
    }

    if (c == '\b') {
        if (s->text_len > 0) {
            s->text_len--;
            s->text[s->text_len] = '\0';
        }
        return;
    }
    if (s->text_len < (int)sizeof(s->text) - 1) {
        s->text[s->text_len++] = c;
        s->text[s->text_len]   = '\0';
    }
}

/*---------------------------------------------------------------------
 * Click handling
 *-------------------------------------------------------------------*/
typedef enum {
    NP_MISS = 0, NP_FNAME, NP_NEW, NP_OPEN, NP_SAVE, NP_BODY
} NpHit;

static NpHit notepad_inner_hit(Window *w, int mx, int my) {
    int tb_x, tb_y, tb_w, tb_h, fn_x, fn_w, bn_x, bn_w, bn_gap;
    int bd_x, bd_y, bd_w, bd_h;
    notepad_layout(w, &tb_x, &tb_y, &tb_w, &tb_h,
                   &fn_x, &fn_w, &bn_x, &bn_w, &bn_gap,
                   &bd_x, &bd_y, &bd_w, &bd_h);

    if (my >= tb_y && my < tb_y + tb_h) {
        if (mx >= fn_x && mx < fn_x + fn_w) return NP_FNAME;
        int bx = bn_x;
        for (int i = 0; i < 3; i++) {
            if (mx >= bx && mx < bx + bn_w) {
                if (i == 0) return NP_NEW;
                if (i == 1) return NP_OPEN;
                return NP_SAVE;
            }
            bx += bn_w + bn_gap;
        }
    }
    if (mx >= bd_x && mx < bd_x + bd_w && my >= bd_y && my < bd_y + bd_h)
        return NP_BODY;
    return NP_MISS;
}

static void notepad_click(Window *w, int mx, int my) {
    NotepadState *s = (NotepadState *)w->app_state;
    switch (notepad_inner_hit(w, mx, my)) {
        case NP_FNAME:
            w->focus_filename = 1;
            break;
        case NP_NEW:
            s->text[0] = '\0';
            s->text_len = 0;
            strcpy(w->filename, "untitled.txt");
            w->focus_filename = 0;
            set_status(w, "New file.");
            break;
        case NP_SAVE:
            w->focus_filename = 0;
            if (note_text_save(w->filename, s->text, s->text_len)) {
                set_status(w, "Saved."); sfx_play(SFX_DING);
            } else {
                set_status(w, "Save failed."); sfx_play(SFX_ERROR);
            }
            break;
        case NP_OPEN:
            open_app(WK_FILES);
            break;
        case NP_BODY:
            w->focus_filename = 0;
            break;
        default: break;
    }
}

/*---------------------------------------------------------------------
 * Drawing
 *-------------------------------------------------------------------*/
static void draw_notepad_body(uint8_t *fb, Window *w, int is_focused) {
    NotepadState *s = (NotepadState *)w->app_state;
    int tb_x, tb_y, tb_w, tb_h, fn_x, fn_w, bn_x, bn_w, bn_gap;
    int bd_x, bd_y, bd_w, bd_h;
    notepad_layout(w, &tb_x, &tb_y, &tb_w, &tb_h,
                   &fn_x, &fn_w, &bn_x, &bn_w, &bn_gap,
                   &bd_x, &bd_y, &bd_w, &bd_h);

    Color fcol = w->focus_filename ? C_FIELD : C_WIN;
    fill_rect(fb, TOP_W, TOP_H, fn_x, tb_y, fn_w, tb_h, fcol);
    bevel(fb, TOP_W, TOP_H, fn_x, tb_y, fn_w, tb_h, 0);
    int max_chars = (fn_w - 6) / 8;
    int fn_len = (int)strlen(w->filename);
    const char *shown = w->filename;
    char buf[FNAME_MAX];
    if (fn_len > max_chars) {
        int off = fn_len - max_chars;
        memcpy(buf, w->filename + off, max_chars);
        buf[max_chars] = '\0';
        shown = buf;
    }
    draw_text(fb, TOP_W, TOP_H, fn_x + 3, tb_y + 3, shown, C_TEXT);
    if (w->focus_filename && is_focused && (caret_tick() < 30)) {
        int caret_x = fn_x + 3 + (int)strlen(shown) * 8;
        vline(fb, TOP_W, TOP_H, caret_x, tb_y + 3, 8, C_TEXT);
    }

    const char *labels[3] = { "New", "Opn", "Sav" };
    int bx = bn_x;
    for (int i = 0; i < 3; i++) {
        fill_rect(fb, TOP_W, TOP_H, bx, tb_y, bn_w, tb_h, C_BTN);
        bevel(fb, TOP_W, TOP_H, bx, tb_y, bn_w, tb_h, 1);
        int lw = (int)strlen(labels[i]) * 8;
        int tx = bx + (bn_w - lw) / 2;
        draw_text(fb, TOP_W, TOP_H, tx, tb_y + 4, labels[i], C_TEXT);
        bx += bn_w + bn_gap;
    }

    fill_rect(fb, TOP_W, TOP_H, bd_x, bd_y, bd_w, bd_h, C_WIN);
    bevel(fb, TOP_W, TOP_H, bd_x, bd_y, bd_w, bd_h, 0);

    int body_focus = is_focused && !w->focus_filename;
    draw_text_block(fb, bd_x, bd_y, bd_w, bd_h, s->text, s->text_len,
                    body_focus, caret_tick() < 30);

    if (w->status_ttl > 0 && w->status[0]) {
        int sy = bd_y + bd_h - 10;
        fill_rect(fb, TOP_W, TOP_H, bd_x + 2, sy, bd_w - 4, 10, C_TASKBAR);
        draw_text(fb, TOP_W, TOP_H, bd_x + 4, sy + 1, w->status, C_TITLE);
    }
}

/*---------------------------------------------------------------------
 * File load (used by File Explorer)
 *-------------------------------------------------------------------*/
static int notepad_load_file(Window *w, const char *fname) {
    NotepadState *s = (NotepadState *)w->app_state;
    int n = 0;
    if (!note_text_load(fname, s->text, (int)sizeof(s->text), &n)) return 0;
    s->text_len = n;
    strncpy(w->filename, fname, FNAME_MAX - 1);
    w->filename[FNAME_MAX - 1] = '\0';
    return 1;
}

/*---------------------------------------------------------------------
 * Reset / vtable
 *-------------------------------------------------------------------*/
static void notepad_reset(Window *w) {
    w->kind = WK_NOTEPAD;
    w->open = 1;
    w->x = 30;  w->y = 20;
    w->w = 260; w->h = 170;
    strcpy(w->title, "Notes");
    w->app_state = &g_notepad_state;
    NotepadState *s = (NotepadState *)w->app_state;
    s->text[0] = '\0';
    s->text_len = 0;
    strcpy(w->filename, "untitled.txt");
    w->focus_filename = 0;
    w->status[0] = '\0';
    w->status_ttl = 0;
    w->app_state = &g_notepad_state;
    bring_to_front(WK_NOTEPAD);
}

static void notepad_draw(uint8_t *fb, Window *w, int focused) {
    draw_notepad_body(fb, w, focused);
}

const App g_app_notepad = {
    "Notes",
    notepad_reset,
    notepad_draw,
    notepad_click,
    notepad_putc,
    NULL,          /* update */
    notepad_load_file,
    NULL,          /* secondary_click */
    NULL,          /* bottom_draw */
    NULL,          /* bottom_hit */
    NULL,          /* bottom_action */
};
