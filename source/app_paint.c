#include "app.h"
#include "icons.h"
#include "audio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PAINT_CW_OLD  192
#define PAINT_CH_OLD  128
#define PAINT_CW_NEW  256
#define PAINT_CH_NEW  144

typedef struct {
    uint8_t *paint_px;
    int      paint_cw;
    int      paint_ch;
    int      paint_bytes;
    int      paint_tool;
    int      paint_color;
    int      paint_last_x;
    int      paint_last_y;
    int      paint_line_sx;
    int      paint_line_sy;
} PaintState;

static PaintState g_paint_state;

/* interaction state owned by the Paint module */
static int g_paint_touch_draw = 0;
static int g_paint_stroking   = 0;

int paint_touch_draw_enabled(void) { return g_paint_touch_draw; }
int paint_is_stroking(void)        { return g_paint_stroking; }

static const Color PAINT_PAL[16] = {
    {255, 255, 255},
    {0,   0,   0  },
    {128, 128, 128},
    {192, 192, 192},
    {128, 0,   0  },
    {255, 0,   0  },
    {255, 128, 0  },
    {255, 220, 60 },
    {0,   128, 0  },
    {120, 200, 120},
    {0,   128, 128},
    {60,  180, 220},
    {0,   0,   128},
    {80,  80,  220},
    {128, 0,   128},
    {240, 140, 200},
};

static inline void paint_set(Window *w, int x, int y, int col) {
    PaintState *s = (PaintState *)w->app_state;
    if ((unsigned)x >= (unsigned)s->paint_cw) return;
    if ((unsigned)y >= (unsigned)s->paint_ch) return;
    int idx = y * s->paint_cw + x;
    int byte = idx / 2;
    uint8_t b = s->paint_px[byte];
    if (idx & 1) b = (uint8_t)((b & 0x0F) | ((col & 0x0F) << 4));
    else         b = (uint8_t)((b & 0xF0) | (col & 0x0F));
    s->paint_px[byte] = b;
}

static inline int paint_get(Window *w, int x, int y) {
    PaintState *s = (PaintState *)w->app_state;
    if ((unsigned)x >= (unsigned)s->paint_cw) return 0;
    if ((unsigned)y >= (unsigned)s->paint_ch) return 0;
    int idx = y * s->paint_cw + x;
    uint8_t b = s->paint_px[idx / 2];
    return (idx & 1) ? (b >> 4) : (b & 0x0F);
}

static void paint_fill_all(Window *w, int col) {
    PaintState *s = (PaintState *)w->app_state;
    uint8_t v = (uint8_t)((col & 0x0F) | ((col & 0x0F) << 4));
    memset(s->paint_px, v, s->paint_bytes);
}

static void paint_brush(Window *w, int x, int y, int radius, int col) {
    for (int dy = -radius; dy <= radius; dy++) {
        for (int dx = -radius; dx <= radius; dx++) {
            if (dx * dx + dy * dy <= radius * radius) {
                paint_set(w, x + dx, y + dy, col);
            }
        }
    }
}

static void paint_line(Window *w, int x0, int y0, int x1, int y1,
                       int radius, int col) {
    int dx =  abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (1) {
        if (radius <= 0) paint_set(w, x0, y0, col);
        else             paint_brush(w, x0, y0, radius, col);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

static void paint_flood(Window *w, int sx, int sy, int new_col) {
    PaintState *s = (PaintState *)w->app_state;
    int old = paint_get(w, sx, sy);
    if (old == new_col) return;

    int total = s->paint_cw * s->paint_ch;
    int *stack = (int *)linearAlloc(total * sizeof(int));
    if (!stack) return;
    int sp = 0;
    stack[sp++] = sy * s->paint_cw + sx;
    while (sp > 0) {
        int p = stack[--sp];
        int x = p % s->paint_cw;
        int y = p / s->paint_cw;
        if (x < 0 || x >= s->paint_cw || y < 0 || y >= s->paint_ch) continue;
        if (paint_get(w, x, y) != old) continue;

        int lx = x;
        while (lx > 0 && paint_get(w, lx - 1, y) == old) lx--;
        int rx = x;
        while (rx < s->paint_cw - 1 && paint_get(w, rx + 1, y) == old) rx++;
        for (int i = lx; i <= rx; i++) paint_set(w, i, y, new_col);

        if (y > 0) {
            for (int i = lx; i <= rx; i++) {
                if (paint_get(w, i, y - 1) == old) {
                    if (sp < total)
                        stack[sp++] = (y - 1) * s->paint_cw + i;
                }
            }
        }
        if (y < s->paint_ch - 1) {
            for (int i = lx; i <= rx; i++) {
                if (paint_get(w, i, y + 1) == old) {
                    if (sp < total)
                        stack[sp++] = (y + 1) * s->paint_cw + i;
                }
            }
        }
    }
    linearFree(stack);
}

/*---------------------------------------------------------------------
 * File I/O
 *-------------------------------------------------------------------*/
static int paint_save(Window *w) {
    if (w->filename[0] == '\0') return 0;
    PaintState *s = (PaintState *)w->app_state;
    char path[160];
    snprintf(path, sizeof(path), "%s/%s", ARTEMIS_DIR, w->filename);
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    const char magic[4] = { 'A', 'P', 'T', '1' };
    uint16_t cw = s->paint_cw, ch = s->paint_ch;
    uint8_t pal[16 * 3];
    for (int i = 0; i < 16; i++) {
        pal[i * 3 + 0] = PAINT_PAL[i].r;
        pal[i * 3 + 1] = PAINT_PAL[i].g;
        pal[i * 3 + 2] = PAINT_PAL[i].b;
    }
    int ok = 1;
    ok = ok && fwrite(magic,        1, 4,           f) == 4;
    ok = ok && fwrite(&cw,          1, 2,           f) == 2;
    ok = ok && fwrite(&ch,          1, 2,           f) == 2;
    ok = ok && fwrite(pal,          1, sizeof(pal), f) == sizeof(pal);
    ok = ok && fwrite(s->paint_px,  1, s->paint_bytes, f) == s->paint_bytes;
    fclose(f);
    return ok;
}

static int paint_load_internal(Window *w, const char *fname) {
    PaintState *s = (PaintState *)w->app_state;
    char path[160];
    snprintf(path, sizeof(path), "%s/%s", ARTEMIS_DIR, fname);
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    char magic[4] = {0};
    uint16_t cw = 0, ch = 0;
    uint8_t pal[16 * 3];
    int ok = 1;
    ok = ok && fread(magic, 1, 4,           f) == 4;
    ok = ok && fread(&cw,   1, 2,           f) == 2;
    ok = ok && fread(&ch,   1, 2,           f) == 2;
    ok = ok && fread(pal,   1, sizeof(pal), f) == sizeof(pal);
    ok = ok && memcmp(magic, "APT1", 4) == 0;
    ok = ok && cw == s->paint_cw && ch == s->paint_ch;
    if (ok) {
        if (fread(s->paint_px, 1, s->paint_bytes, f) != s->paint_bytes) ok = 0;
    }
    fclose(f);
    return ok;
}

static int paint_load_file(Window *w, const char *fname) {
    if (!paint_load_internal(w, fname)) return 0;
    size_t n = strlen(fname);
    if (n > FNAME_MAX - 1) n = FNAME_MAX - 1;
    memcpy(w->filename, fname, n);
    w->filename[n] = '\0';
    return 1;
}

/*---------------------------------------------------------------------
 * Input (filename focus)
 *-------------------------------------------------------------------*/
static void paint_input(Window *w, char c) {
    if (!w->focus_filename) return;
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
}

/*---------------------------------------------------------------------
 * Layout (top screen)
 *-------------------------------------------------------------------*/
typedef enum {
    PA_MISS = 0,
    PA_FNAME,
    PA_NEW, PA_OPEN, PA_SAVE,
    PA_TOOL,
    PA_COLOR,
    PA_CANVAS,
    PA_TITLE
} PaHit;

static const char *PAINT_TOOLS[] = {
    "Pen", "Brush", "Eraser", "Fill", "Line"
};
#define PAINT_NUM_TOOLS ((int)(sizeof(PAINT_TOOLS) / sizeof(PAINT_TOOLS[0])))

static const char * const *paint_tool_icon(int i) {
    switch (i) {
        case 0: return ICON_TOOL_PEN;
        case 1: return ICON_TOOL_BRUSH;
        case 2: return ICON_TOOL_ERASER;
        case 3: return ICON_TOOL_FILL;
        case 4: return ICON_TOOL_LINE;
        case 5: return ICON_TOOL_TOUCH;
    }
    return ICON_TOOL_PEN;
}

typedef struct {
    int  tb_x, tb_y, tb_w, tb_h;
    int  fn_x, fn_w;
    int  bn_x, bn_w, bn_gap;
    int  tc_x, tc_y, tc_w, tc_h;
    int  tc_btn_h;
    int  cv_x, cv_y, cv_w, cv_h;
    int  pl_x, pl_y, pl_w, pl_h;
    int  pl_sw;
} PaintLayout;

static void paint_layout(Window *w, PaintLayout *L) {
    PaintState *s = (PaintState *)w->app_state;
    L->tb_x = w->x + 5;
    L->tb_y = w->y + 20;
    L->tb_w = w->w - 10;
    L->tb_h = 15;

    int btn_w = 28, gap = 2;
    int btns_total = 3 * btn_w + 2 * gap;
    L->fn_x = L->tb_x;
    L->fn_w = L->tb_w - btns_total - 3;
    L->bn_x = L->fn_x + L->fn_w + 3;
    L->bn_w = btn_w;
    L->bn_gap = gap;

    int col_w = 16;
    L->tc_x = w->x + 5;
    L->tc_y = L->tb_y + L->tb_h + 2;
    L->tc_w = col_w;
    L->tc_h = s->paint_ch;

    L->tc_btn_h = L->tc_h / (PAINT_NUM_TOOLS + 1);

    L->cv_x = L->tc_x + L->tc_w + 2;
    L->cv_y = L->tc_y;
    L->cv_w = s->paint_cw;
    L->cv_h = s->paint_ch;

    L->pl_x = L->cv_x;
    L->pl_y = L->cv_y + L->cv_h + 2;
    L->pl_w = s->paint_cw;
    L->pl_h = 10;
    L->pl_sw = L->pl_w / 16;
}

/*---------------------------------------------------------------------
 * Top screen drawing
 *-------------------------------------------------------------------*/
static void draw_paint_canvas(uint8_t *fb, Window *w, int px, int py) {
    PaintState *s = (PaintState *)w->app_state;
    for (int y = 0; y < s->paint_ch; y++) {
        for (int x = 0; x < s->paint_cw; x++) {
            int c = paint_get(w, x, y);
            put_px(fb, TOP_W, TOP_H, px + x, py + y, PAINT_PAL[c]);
        }
    }
    bevel(fb, TOP_W, TOP_H, px - 1, py - 1, s->paint_cw + 2, s->paint_ch + 2, 0);
}

static void draw_paint_body(uint8_t *fb, Window *w, int is_focused) {
    PaintState *s = (PaintState *)w->app_state;
    PaintLayout L;
    paint_layout(w, &L);

    Color fcol = w->focus_filename ? C_FIELD : C_WIN;
    fill_rect(fb, TOP_W, TOP_H, L.fn_x, L.tb_y, L.fn_w, L.tb_h, fcol);
    bevel(fb, TOP_W, TOP_H, L.fn_x, L.tb_y, L.fn_w, L.tb_h, 0);
    int max_chars = (L.fn_w - 6) / 8;
    int fn_len = (int)strlen(w->filename);
    const char *shown = w->filename;
    char fbuf[FNAME_MAX];
    if (fn_len > max_chars) {
        int off = fn_len - max_chars;
        memcpy(fbuf, w->filename + off, max_chars);
        fbuf[max_chars] = '\0';
        shown = fbuf;
    }
    draw_text(fb, TOP_W, TOP_H, L.fn_x + 3, L.tb_y + 3, shown, C_TEXT);
    if (w->focus_filename && is_focused && (caret_tick() < 30)) {
        int caret_x = L.fn_x + 3 + (int)strlen(shown) * 8;
        vline(fb, TOP_W, TOP_H, caret_x, L.tb_y + 3, 8, C_TEXT);
    }
    const char *tb_labels[3] = { "New", "Opn", "Sav" };
    int bx = L.bn_x;
    for (int i = 0; i < 3; i++) {
        fill_rect(fb, TOP_W, TOP_H, bx, L.tb_y, L.bn_w, L.tb_h, C_BTN);
        bevel(fb, TOP_W, TOP_H, bx, L.tb_y, L.bn_w, L.tb_h, 1);
        int lw = (int)strlen(tb_labels[i]) * 8;
        int tx = bx + (L.bn_w - lw) / 2;
        draw_text(fb, TOP_W, TOP_H, tx, L.tb_y + 4, tb_labels[i], C_TEXT);
        bx += L.bn_w + L.bn_gap;
    }

    for (int i = 0; i < PAINT_NUM_TOOLS + 1; i++) {
        int by = L.tc_y + i * L.tc_btn_h + 1;
        int bh = L.tc_btn_h - 2;
        int is_touch_btn = (i == PAINT_NUM_TOOLS);
        int sel = is_touch_btn ? g_paint_touch_draw : (s->paint_tool == i);
        fill_rect(fb, TOP_W, TOP_H, L.tc_x, by, L.tc_w, bh, C_BTN);
        bevel(fb, TOP_W, TOP_H, L.tc_x, by, L.tc_w, bh, sel ? 0 : 1);
        int icon_x = L.tc_x + (L.tc_w - ICON_SMALL_SZ) / 2;
        int icon_y = by + (bh - ICON_SMALL_SZ) / 2;
        draw_icon_small(fb, TOP_W, TOP_H, icon_x, icon_y, paint_tool_icon(i));
    }

    draw_paint_canvas(fb, w, L.cv_x, L.cv_y);

    for (int i = 0; i < 16; i++) {
        int sx = L.pl_x + i * L.pl_sw;
        int sel = (s->paint_color == i);
        fill_rect(fb, TOP_W, TOP_H, sx, L.pl_y, L.pl_sw - 1, L.pl_h,
                  PAINT_PAL[i]);
        Color edge = sel ? (Color){255, 255, 0} : C_BLACK;
        hline(fb, TOP_W, TOP_H, sx, L.pl_y, L.pl_sw - 1, edge);
        hline(fb, TOP_W, TOP_H, sx, L.pl_y + L.pl_h - 1, L.pl_sw - 1, edge);
        vline(fb, TOP_W, TOP_H, sx, L.pl_y, L.pl_h, edge);
        vline(fb, TOP_W, TOP_H, sx + L.pl_sw - 2, L.pl_y, L.pl_h, edge);
    }

    if (w->status_ttl > 0 && w->status[0]) {
        int sy = L.pl_y + L.pl_h + 1;
        draw_text(fb, TOP_W, TOP_H, L.tc_x, sy, w->status, C_TITLE);
    }
}

/*---------------------------------------------------------------------
 * Top screen hit testing / click
 *-------------------------------------------------------------------*/
static PaHit paint_inner_hit(Window *w, int mx, int my,
                             int *out_a, int *out_b) {
    PaintLayout L;
    paint_layout(w, &L);

    if (my >= L.tb_y && my < L.tb_y + L.tb_h) {
        if (mx >= L.fn_x && mx < L.fn_x + L.fn_w) return PA_FNAME;
        int bx = L.bn_x;
        for (int i = 0; i < 3; i++) {
            if (mx >= bx && mx < bx + L.bn_w) {
                if (i == 0) return PA_NEW;
                if (i == 1) return PA_OPEN;
                return PA_SAVE;
            }
            bx += L.bn_w + L.bn_gap;
        }
    }

    if (mx >= L.tc_x && mx < L.tc_x + L.tc_w &&
        my >= L.tc_y && my < L.tc_y + L.tc_h) {
        int rel = my - L.tc_y;
        int idx = rel / L.tc_btn_h;
        if (idx >= 0 && idx < PAINT_NUM_TOOLS + 1) { *out_a = idx; return PA_TOOL; }
    }

    if (mx >= L.pl_x && mx < L.pl_x + L.pl_w &&
        my >= L.pl_y && my < L.pl_y + L.pl_h) {
        int rel = mx - L.pl_x;
        int idx = rel / L.pl_sw;
        if (idx >= 0 && idx < 16) { *out_a = idx; return PA_COLOR; }
    }

    if (mx >= L.cv_x && mx < L.cv_x + L.cv_w &&
        my >= L.cv_y && my < L.cv_y + L.cv_h) {
        *out_a = mx - L.cv_x;
        *out_b = my - L.cv_y;
        return PA_CANVAS;
    }
    return PA_MISS;
}

static void paint_stroke(Window *w, int cx, int cy, int start) {
    PaintState *s = (PaintState *)w->app_state;
    int col = s->paint_color;
    int eraser_col = 0;
    switch (s->paint_tool) {
        case 0:
            if (!start && s->paint_last_x >= 0)
                paint_line(w, s->paint_last_x, s->paint_last_y, cx, cy, 0, col);
            else
                paint_set(w, cx, cy, col);
            s->paint_last_x = cx; s->paint_last_y = cy;
            break;
        case 1:
            if (!start && s->paint_last_x >= 0)
                paint_line(w, s->paint_last_x, s->paint_last_y, cx, cy, 2, col);
            else
                paint_brush(w, cx, cy, 2, col);
            s->paint_last_x = cx; s->paint_last_y = cy;
            break;
        case 2:
            if (!start && s->paint_last_x >= 0)
                paint_line(w, s->paint_last_x, s->paint_last_y, cx, cy, 2, eraser_col);
            else
                paint_brush(w, cx, cy, 2, eraser_col);
            s->paint_last_x = cx; s->paint_last_y = cy;
            break;
        case 3:
            if (start) paint_flood(w, cx, cy, col);
            break;
        case 4:
            if (start) {
                if (s->paint_line_sx < 0) {
                    s->paint_line_sx = cx; s->paint_line_sy = cy;
                } else {
                    paint_line(w, s->paint_line_sx, s->paint_line_sy, cx, cy, 0, col);
                    s->paint_line_sx = -1; s->paint_line_sy = -1;
                }
            }
            break;
    }
}

static void paint_stroke_end(Window *w) {
    PaintState *s = (PaintState *)w->app_state;
    s->paint_last_x = -1;
    s->paint_last_y = -1;
}

static void paint_click(Window *w, int mx, int my) {
    int a = 0, b = 0;
    PaHit ph = paint_inner_hit(w, mx, my, &a, &b);
    switch (ph) {
        case PA_FNAME:
            w->focus_filename = 1;
            break;
        case PA_NEW:
            paint_fill_all(w, 0);
            strcpy(w->filename, "untitled.apt");
            w->focus_filename = 0;
            ((PaintState *)w->app_state)->paint_line_sx = -1;
            ((PaintState *)w->app_state)->paint_line_sy = -1;
            set_status(w, "New canvas.");
            break;
        case PA_SAVE:
            w->focus_filename = 0;
            if (paint_save(w)) { set_status(w, "Saved."); sfx_play(SFX_DING); }
            else               { set_status(w, "Save failed."); sfx_play(SFX_ERROR); }
            break;
        case PA_OPEN:
            open_app(WK_FILES);
            break;
        case PA_TOOL:
            if (a == PAINT_NUM_TOOLS) {
                g_paint_touch_draw = !g_paint_touch_draw;
                set_status(w, g_paint_touch_draw ? "Touch-draw on." : "Touch-draw off.");
            } else {
                ((PaintState *)w->app_state)->paint_tool = a;
            }
            w->focus_filename = 0;
            ((PaintState *)w->app_state)->paint_line_sx = -1;
            ((PaintState *)w->app_state)->paint_line_sy = -1;
            break;
        case PA_COLOR:
            ((PaintState *)w->app_state)->paint_color = a;
            w->focus_filename = 0;
            break;
        case PA_CANVAS:
            w->focus_filename = 0;
            paint_stroke(w, a, b, 1);
            g_paint_stroking = 1;
            break;
        default: break;
    }
}

/*---------------------------------------------------------------------
 * Bottom screen layout / hit / drawing
 *-------------------------------------------------------------------*/
typedef struct {
    int cv_x, cv_y, cv_w, cv_h;
    int tr_x, tr_y, tr_h;
    int tb_sz, tb_gap;
    int fa_x, fa_y, fa_w, fa_h, fa_gap;
    int pl_x, pl_y, pl_w, pl_h, pl_sw;
} PaintBotLayout;

static void paint_bot_layout(PaintBotLayout *L) {
    L->cv_x = (BOT_W - g_paint_state.paint_cw) / 2;
    L->cv_y = 0;
    L->cv_w = g_paint_state.paint_cw;
    L->cv_h = g_paint_state.paint_ch;

    L->tr_y = L->cv_y + L->cv_h + 4;
    L->tr_h = 24;
    L->tb_sz = 20;
    L->tb_gap = 2;
    L->tr_x = 6;

    int tools_w = (PAINT_NUM_TOOLS + 1) * (L->tb_sz + L->tb_gap);
    L->fa_y = L->tr_y + (L->tr_h - 16) / 2;
    L->fa_h = 16;
    L->fa_w = 32;
    L->fa_gap = 2;
    L->fa_x = L->tr_x + tools_w + 4;

    L->pl_y = L->tr_y + L->tr_h + 4;
    L->pl_h = 18;
    L->pl_sw = 18;
    L->pl_w  = 16 * L->pl_sw;
    L->pl_x  = (BOT_W - L->pl_w) / 2;
}

typedef enum {
    PB_MISS = 0,
    PB_CANVAS, PB_TOOL, PB_COLOR,
    PB_NEW, PB_OPEN, PB_SAVE
} PbHit;

static PbHit paint_bot_hit(int tx, int ty, int *out_a, int *out_b) {
    PaintBotLayout L;
    paint_bot_layout(&L);

    if (tx >= L.cv_x && tx < L.cv_x + L.cv_w &&
        ty >= L.cv_y && ty < L.cv_y + L.cv_h) {
        *out_a = tx - L.cv_x;
        *out_b = ty - L.cv_y;
        return PB_CANVAS;
    }

    for (int i = 0; i < PAINT_NUM_TOOLS + 1; i++) {
        int bx = L.tr_x + i * (L.tb_sz + L.tb_gap);
        int by = L.tr_y + (L.tr_h - L.tb_sz) / 2;
        if (tx >= bx && tx < bx + L.tb_sz &&
            ty >= by && ty < by + L.tb_sz) {
            *out_a = i;
            return PB_TOOL;
        }
    }

    {
        int bx = L.fa_x;
        for (int i = 0; i < 3; i++) {
            if (tx >= bx && tx < bx + L.fa_w &&
                ty >= L.fa_y && ty < L.fa_y + L.fa_h) {
                if (i == 0) return PB_NEW;
                if (i == 1) return PB_OPEN;
                return PB_SAVE;
            }
            bx += L.fa_w + L.fa_gap;
        }
    }

    if (tx >= L.pl_x && tx < L.pl_x + L.pl_w &&
        ty >= L.pl_y && ty < L.pl_y + L.pl_h) {
        int rel = tx - L.pl_x;
        int idx = rel / L.pl_sw;
        if (idx < 0) idx = 0;
        if (idx > 15) idx = 15;
        *out_a = idx;
        return PB_COLOR;
    }

    return PB_MISS;
}

void draw_paint_bottom(uint8_t *bot, Window *w) {
    PaintState *s = (PaintState *)w->app_state;
    PaintBotLayout L;
    paint_bot_layout(&L);

    fill_rect(bot, BOT_W, BOT_H, 0, 0, BOT_W, BOT_H, C_TASKBAR);

    for (int y = 0; y < s->paint_ch; y++) {
        for (int x = 0; x < s->paint_cw; x++) {
            int c = paint_get(w, x, y);
            put_px(bot, BOT_W, BOT_H, L.cv_x + x, L.cv_y + y, PAINT_PAL[c]);
        }
    }
    bevel(bot, BOT_W, BOT_H, L.cv_x - 1, L.cv_y, s->paint_cw + 2, s->paint_ch + 1, 0);

    for (int i = 0; i < PAINT_NUM_TOOLS + 1; i++) {
        int bx = L.tr_x + i * (L.tb_sz + L.tb_gap);
        int by = L.tr_y + (L.tr_h - L.tb_sz) / 2;
        int is_touch_btn = (i == PAINT_NUM_TOOLS);
        int sel = is_touch_btn ? g_paint_touch_draw : (s->paint_tool == i);
        fill_rect(bot, BOT_W, BOT_H, bx, by, L.tb_sz, L.tb_sz, C_BTN);
        bevel(bot, BOT_W, BOT_H, bx, by, L.tb_sz, L.tb_sz, sel ? 0 : 1);
        int ix = bx + (L.tb_sz - ICON_SMALL_SZ) / 2;
        int iy = by + (L.tb_sz - ICON_SMALL_SZ) / 2;
        draw_icon_small(bot, BOT_W, BOT_H, ix, iy, paint_tool_icon(i));
    }

    const char *fa_labels[3] = { "New", "Opn", "Sav" };
    {
        int bx = L.fa_x;
        for (int i = 0; i < 3; i++) {
            fill_rect(bot, BOT_W, BOT_H, bx, L.fa_y, L.fa_w, L.fa_h, C_BTN);
            bevel(bot, BOT_W, BOT_H, bx, L.fa_y, L.fa_w, L.fa_h, 1);
            int lw = (int)strlen(fa_labels[i]) * 8;
            int tx = bx + (L.fa_w - lw) / 2;
            draw_text(bot, BOT_W, BOT_H, tx, L.fa_y + (L.fa_h - 8) / 2,
                      fa_labels[i], C_TEXT);
            bx += L.fa_w + L.fa_gap;
        }
    }

    for (int i = 0; i < 16; i++) {
        int sx = L.pl_x + i * L.pl_sw;
        int sel = (s->paint_color == i);
        fill_rect(bot, BOT_W, BOT_H, sx, L.pl_y, L.pl_sw - 1, L.pl_h, PAINT_PAL[i]);
        Color edge = sel ? (Color){255, 255, 0} : C_BLACK;
        hline(bot, BOT_W, BOT_H, sx, L.pl_y, L.pl_sw - 1, edge);
        hline(bot, BOT_W, BOT_H, sx, L.pl_y + L.pl_h - 1, L.pl_sw - 1, edge);
        vline(bot, BOT_W, BOT_H, sx, L.pl_y, L.pl_h, edge);
        vline(bot, BOT_W, BOT_H, sx + L.pl_sw - 2, L.pl_y, L.pl_h, edge);
    }

    int info_y = L.pl_y + L.pl_h + 4;
    draw_text(bot, BOT_W, BOT_H, 6, info_y, w->filename, C_TEXT);
    if (w->status_ttl > 0 && w->status[0]) {
        draw_text(bot, BOT_W, BOT_H, 6, info_y + 10, w->status, C_TITLE);
    }
}

/*---------------------------------------------------------------------
 * Bottom screen input (stylus touch)
 *-------------------------------------------------------------------*/
void paint_bottom_touch(int tx, int ty, int touching, int prev_touching) {
    Window *pw = &g_wins[WK_PAINT];
    if (!pw->open) return;

    if (touching && !prev_touching) {
        int a = 0, b = 0;
        PbHit h = paint_bot_hit(tx, ty, &a, &b);
        switch (h) {
            case PB_CANVAS:
                paint_stroke(pw, a, b, 1);
                g_paint_stroking = 1;
                break;
            case PB_TOOL:
                if (a == PAINT_NUM_TOOLS) {
                    g_paint_touch_draw = 0;
                    set_status(pw, "Touch-draw off.");
                } else {
                    ((PaintState *)pw->app_state)->paint_tool = a;
                    ((PaintState *)pw->app_state)->paint_line_sx = -1;
                    ((PaintState *)pw->app_state)->paint_line_sy = -1;
                }
                break;
            case PB_COLOR:
                ((PaintState *)pw->app_state)->paint_color = a;
                break;
            case PB_NEW:
                paint_fill_all(pw, 0);
                strcpy(pw->filename, "untitled.apt");
                ((PaintState *)pw->app_state)->paint_line_sx = -1;
                ((PaintState *)pw->app_state)->paint_line_sy = -1;
                set_status(pw, "New canvas.");
                break;
            case PB_OPEN:
                g_paint_touch_draw = 0;
                open_app(WK_FILES);
                break;
            case PB_SAVE:
                if (paint_save(pw)) { set_status(pw, "Saved."); sfx_play(SFX_DING); }
                else                { set_status(pw, "Save failed."); sfx_play(SFX_ERROR); }
                break;
            default: break;
        }
    } else if (touching && prev_touching) {
        if (g_paint_stroking) {
            PaintBotLayout L;
            paint_bot_layout(&L);
            int cx = tx - L.cv_x;
            int cy = ty - L.cv_y;
            {
                PaintState *s = (PaintState *)pw->app_state;
                if (cx >= 0 && cx < s->paint_cw && cy >= 0 && cy < s->paint_ch) {
                    paint_stroke(pw, cx, cy, 0);
                }
            }
        }
    } else if (!touching && prev_touching) {
        if (g_paint_stroking) {
            paint_stroke_end(pw);
            g_paint_stroking = 0;
        }
    }
}

/*---------------------------------------------------------------------
 * Top screen cursor-driven stroke (frame)
 *-------------------------------------------------------------------*/
void paint_cursor_start(Window *w, int gx, int gy) {
    if (!w->open || w->kind != WK_PAINT) return;
    PaintLayout L;
    paint_layout(w, &L);
    int cx = gx - L.cv_x;
    int cy = gy - L.cv_y;
    if (cx >= 0 && cx < L.cv_w && cy >= 0 && cy < L.cv_h) {
        paint_stroke(w, cx, cy, 1);
        g_paint_stroking = 1;
    }
}

void paint_cursor_stroke_frame(Window *pw, int gx, int gy, int still_held) {
    if (!g_paint_stroking) return;
    if (!still_held) {
        g_paint_stroking = 0;
        paint_stroke_end(pw);
        return;
    }
    if (!pw->open || pw->kind != WK_PAINT) return;
    PaintLayout L;
    paint_layout(pw, &L);
    int cx = gx - L.cv_x;
    int cy = gy - L.cv_y;
    if (cx >= 0 && cx < L.cv_w && cy >= 0 && cy < L.cv_h) {
        paint_stroke(pw, cx, cy, 0);
    }
}

/*---------------------------------------------------------------------
 * Reset / vtable
 *-------------------------------------------------------------------*/
static void paint_reset(Window *w) {
    w->kind = WK_PAINT;
    w->open = 1;
    w->x = 30;  w->y = 10;
    /* New 3DS gets a bigger canvas (more RAM); original keeps the small one. */
    if (is_new3ds()) {
        w->w = 300; w->h = 200;
        g_paint_state.paint_cw = PAINT_CW_NEW;
        g_paint_state.paint_ch = PAINT_CH_NEW;
    } else {
        w->w = 228; w->h = 200;
        g_paint_state.paint_cw = PAINT_CW_OLD;
        g_paint_state.paint_ch = PAINT_CH_OLD;
    }
    strcpy(w->title, "Paint");
    strcpy(w->filename, "untitled.apt");
    w->focus_filename = 0;
    w->status[0] = '\0';
    w->status_ttl = 0;
    w->app_state = &g_paint_state;
    PaintState *s = (PaintState *)w->app_state;
    if (s->paint_px) linearFree(s->paint_px);
    s->paint_bytes = s->paint_cw * s->paint_ch / 2;
    s->paint_px = (uint8_t *)linearAlloc(s->paint_bytes);
    if (!s->paint_px) {
        s->paint_bytes = 0;
        return;
    }
    s->paint_tool   = 0;
    s->paint_color  = 1;
    s->paint_last_x = -1;
    s->paint_last_y = -1;
    s->paint_line_sx = -1;
    s->paint_line_sy = -1;
    paint_fill_all(w, 0);
    bring_to_front(WK_PAINT);
}

static void paint_draw(uint8_t *fb, Window *w, int focused) {
    draw_paint_body(fb, w, focused);
}

const App g_app_paint = {
    "Paint",
    paint_reset,
    paint_draw,
    paint_click,
    paint_input,
    NULL,          /* update */
    paint_load_file,
    NULL,          /* secondary_click */
    draw_paint_bottom,
    NULL,          /* bottom_hit (handled by paint_bottom_touch) */
    NULL,          /* bottom_action  */
};
