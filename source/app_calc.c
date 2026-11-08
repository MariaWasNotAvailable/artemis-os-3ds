#include "app.h"
#include "icons.h"
#include "audio.h"

#include <string.h>
#include <stdio.h>

typedef struct {
    long long calc_cur;
    long long calc_prev;
    int       calc_op;
    int       calc_reset;
    int       calc_error;
} CalcState;

static CalcState g_calc_state;

/*---------------------------------------------------------------------
 * Logic
 *-------------------------------------------------------------------*/
static long long calc_apply(long long a, long long b, int op, int *err) {
    *err = 0;
    switch (op) {
        case '+': return a + b;
        case '-': return a - b;
        case '*': return a * b;
        case '/':
            if (b == 0) { *err = 1; return 0; }
            return a / b;
    }
    return b;
}

static const char *CALC_LABELS[5][4] = {
    { "C",  "N",  "B",  "/" },
    { "7",  "8",  "9",  "*" },
    { "4",  "5",  "6",  "-" },
    { "1",  "2",  "3",  "+" },
    { "0",  "0",  ".",  "=" },
};

static void calc_client_rect(Window *w, int *cx, int *cy, int *cw, int *ch) {
    *cx = w->x + 4;
    *cy = w->y + 20;
    *cw = w->w - 8;
    *ch = w->h - 24;
}

static void calc_button_rect(Window *w, int row, int col,
                             int *bx, int *by, int *bw, int *bh) {
    int cx, cy, cw, ch;
    calc_client_rect(w, &cx, &cy, &cw, &ch);
    int display_h = 18;
    int grid_y = cy + display_h + 3;
    int grid_h = ch - display_h - 5;
    int cell_w = cw / 4;
    int cell_h = grid_h / 5;
    *bx = cx + col * cell_w + 1;
    *by = grid_y + row * cell_h + 1;
    *bw = cell_w - 2;
    *bh = cell_h - 2;
    if (row == 4 && col == 0) *bw = cell_w * 2 - 2;
}

static void calc_handle_input(Window *w, char code) {
    CalcState *s = (CalcState *)w->app_state;
    if (s->calc_error && code != 'C') return;
    if (code >= '0' && code <= '9') {
        if (s->calc_reset) { s->calc_cur = 0; s->calc_reset = 0; }
        if (s->calc_cur > 99999999LL || s->calc_cur < -99999999LL) return;
        long long d = code - '0';
        if (s->calc_cur < 0) s->calc_cur = s->calc_cur * 10 - d;
        else                 s->calc_cur = s->calc_cur * 10 + d;
        return;
    }
    switch (code) {
        case 'C':
            s->calc_cur = 0; s->calc_prev = 0;
            s->calc_op = 0; s->calc_reset = 1;
            s->calc_error = 0;
            break;
        case 'N':
            s->calc_cur = -s->calc_cur;
            break;
        case 'B':
            if (!s->calc_reset) s->calc_cur /= 10;
            break;
        case '+': case '-': case '*': case '/':
            if (s->calc_op && !s->calc_reset) {
                int err;
                long long r = calc_apply(s->calc_prev, s->calc_cur, s->calc_op, &err);
                if (err) { s->calc_error = 1; s->calc_cur = 0; return; }
                s->calc_cur = r;
            }
            s->calc_prev = s->calc_cur;
            s->calc_op = code;
            s->calc_reset = 1;
            break;
        case '=':
            if (s->calc_op) {
                int err;
                long long r = calc_apply(s->calc_prev, s->calc_cur, s->calc_op, &err);
                if (err) { s->calc_error = 1; s->calc_cur = 0; return; }
                s->calc_cur = r;
                s->calc_op = 0;
                s->calc_reset = 1;
            }
            break;
        case '.': break;
    }
}

static int calc_hit_button(Window *w, int mx, int my, int *row, int *col) {
    for (int r = 0; r < 5; r++) {
        for (int c = 0; c < 4; c++) {
            if (r == 4 && c == 1) continue;
            int bx, by, bw, bh;
            calc_button_rect(w, r, c, &bx, &by, &bw, &bh);
            if (mx >= bx && mx < bx + bw && my >= by && my < by + bh) {
                *row = r; *col = c;
                return 1;
            }
        }
    }
    return 0;
}

/*---------------------------------------------------------------------
 * Click
 *-------------------------------------------------------------------*/
static void calc_click(Window *w, int mx, int my) {
    int r, c;
    if (calc_hit_button(w, mx, my, &r, &c)) {
        const char *lab = CALC_LABELS[r][c];
        char code = lab[0];
        calc_handle_input(w, code);
    }
}

/*---------------------------------------------------------------------
 * Drawing
 *-------------------------------------------------------------------*/
static void draw_calc_body(uint8_t *fb, Window *w) {
    CalcState *s = (CalcState *)w->app_state;
    int cx, cy, cw, ch;
    calc_client_rect(w, &cx, &cy, &cw, &ch);

    int dh = 16;
    fill_rect(fb, TOP_W, TOP_H, cx, cy, cw, dh, C_CALC_LCD);
    bevel(fb, TOP_W, TOP_H, cx, cy, cw, dh, 0);

    char buf[32];
    if (s->calc_error) snprintf(buf, sizeof(buf), "Error");
    else               snprintf(buf, sizeof(buf), "%lld", s->calc_cur);
    int tw = (int)strlen(buf) * 8;
    draw_text(fb, TOP_W, TOP_H, cx + cw - tw - 4, cy + 4, buf, C_TEXT);
    if (s->calc_op) {
        char op[2] = { (char)s->calc_op, 0 };
        draw_text(fb, TOP_W, TOP_H, cx + 4, cy + 4, op, C_TEXT);
    }

    for (int r = 0; r < 5; r++) {
        for (int c = 0; c < 4; c++) {
            if (r == 4 && c == 1) continue;
            int bx, by, bw, bh;
            calc_button_rect(w, r, c, &bx, &by, &bw, &bh);
            fill_rect(fb, TOP_W, TOP_H, bx, by, bw, bh, C_BTN);
            bevel(fb, TOP_W, TOP_H, bx, by, bw, bh, 1);
            const char *lab = CALC_LABELS[r][c];
            const char *show = lab;
            if (!strcmp(lab, "B")) show = "<-";
            if (!strcmp(lab, "N")) show = "+/-";
            int lw = (int)strlen(show) * 8;
            int tx = bx + (bw - lw) / 2;
            int ty = by + (bh - 8) / 2;
            Color col = C_TEXT;
            char ch0 = lab[0];
            if (ch0 == '+' || ch0 == '-' || ch0 == '*' ||
                ch0 == '/' || ch0 == '=')
                col = C_TITLE;
            draw_text(fb, TOP_W, TOP_H, tx, ty, show, col);
        }
    }
}

/*---------------------------------------------------------------------
 * Reset / vtable
 *-------------------------------------------------------------------*/
static void calc_reset(Window *w) {
    w->kind = WK_CALC;
    w->open = 1;
    w->x = 200; w->y = 40;
    w->w = 150; w->h = 160;
    strcpy(w->title, "Calculator");
    w->app_state = &g_calc_state;
    CalcState *s = (CalcState *)w->app_state;
    s->calc_cur = 0; s->calc_prev = 0;
    s->calc_op = 0; s->calc_reset = 1; s->calc_error = 0;
    bring_to_front(WK_CALC);
}

static void calc_draw(uint8_t *fb, Window *w, int focused) {
    (void)focused;
    draw_calc_body(fb, w);
}

static void calc_input(Window *w, char c) {
    calc_handle_input(w, c);
}

const App g_app_calc = {
    "Calculator",
    calc_reset,
    calc_draw,
    calc_click,
    calc_input,
    NULL,          /* update */
    NULL,          /* load_file */
    NULL,          /* secondary_click */
    NULL,          /* bottom_draw */
    NULL,          /* bottom_hit */
    NULL,          /* bottom_action */
};
