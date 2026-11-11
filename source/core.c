#include "core.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>

#include <3ds/services/cfgu.h>

#include "font8x8.h"
#include "buildinfo.h"

/*---------------------------------------------------------------------
 * Palette
 *-------------------------------------------------------------------*/
const Color C_DESKTOP  = {0,   128, 128};
const Color C_TASKBAR  = {192, 192, 192};
const Color C_HI       = {255, 255, 255};
const Color C_LO       = {64,  64,  64};
const Color C_BLACK    = {0,   0,   0};
const Color C_WIN      = {255, 255, 255};
const Color C_TITLE    = {0,   0,   128};
const Color C_TITLE_INACTIVE = {128, 128, 128};
const Color C_TITLETXT = {255, 255, 255};
const Color C_TEXT     = {0,   0,   0};
const Color C_BTN      = {192, 192, 192};
const Color C_MENU     = {192, 192, 192};
const Color C_MENU_HI  = {0,   0,   128};
const Color C_KEY      = {230, 230, 230};
const Color C_KEYDN    = {150, 150, 180};
const Color C_KEYSPEC  = {210, 210, 230};
const Color C_PAD      = {180, 180, 210};
const Color C_PADDOT   = {130, 130, 160};
const Color C_CALC_LCD = {190, 220, 180};
const Color C_FIELD    = {255, 255, 220};
const Color C_DIM      = {110, 110, 110};

/*---------------------------------------------------------------------
 * Drawing primitives
 *-------------------------------------------------------------------*/
void put_px(uint8_t *fb, int fb_w, int fb_h, int x, int y, Color c) {
    if ((unsigned)x >= (unsigned)fb_w || (unsigned)y >= (unsigned)fb_h) return;
    int off = (x * fb_h + (fb_h - 1 - y)) * 3;
    fb[off + 0] = c.b;
    fb[off + 1] = c.g;
    fb[off + 2] = c.r;
}

void fill_rect(uint8_t *fb, int fb_w, int fb_h,
               int x, int y, int w, int h, Color c) {
    if (x < 0)          { w += x; x = 0; }
    if (y < 0)          { h += y; y = 0; }
    if (x + w > fb_w)   w = fb_w - x;
    if (y + h > fb_h)   h = fb_h - y;
    if (w <= 0 || h <= 0) return;

    for (int i = 0; i < w; i++) {
        uint8_t *p = fb + (((x + i) * fb_h) + (fb_h - 1 - (y + h - 1))) * 3;
        for (int k = 0; k < h; k++) {
            *p++ = c.b;
            *p++ = c.g;
            *p++ = c.r;
        }
    }
}

void hline(uint8_t *fb, int fb_w, int fb_h, int x, int y, int w, Color c) {
    for (int i = 0; i < w; i++) put_px(fb, fb_w, fb_h, x + i, y, c);
}

void vline(uint8_t *fb, int fb_w, int fb_h, int x, int y, int h, Color c) {
    for (int j = 0; j < h; j++) put_px(fb, fb_w, fb_h, x, y + j, c);
}

void bevel(uint8_t *fb, int fb_w, int fb_h, int x, int y, int w, int h,
           int raised) {
    Color a = raised ? C_HI : C_LO;
    Color b = raised ? C_LO : C_HI;
    hline(fb, fb_w, fb_h, x, y, w, a);
    vline(fb, fb_w, fb_h, x, y, h, a);
    hline(fb, fb_w, fb_h, x, y + h - 1, w, b);
    vline(fb, fb_w, fb_h, x + w - 1, y, h, b);
}

void draw_char(uint8_t *fb, int fb_w, int fb_h, int x, int y, char ch, Color c) {
    if ((unsigned char)ch < 0x20 || (unsigned char)ch > 0x7F) ch = '?';
    const uint8_t *g = font8x8_basic[(int)ch - 0x20];
    for (int row = 0; row < 8; row++) {
        uint8_t bits = g[row];
        for (int col = 0; col < 8; col++) {
            if (bits & (1 << col)) put_px(fb, fb_w, fb_h, x + col, y + row, c);
        }
    }
}

void draw_text(uint8_t *fb, int fb_w, int fb_h, int x, int y,
               const char *s, Color c) {
    int ox = x;
    while (*s) {
        if (*s == '\n') { y += 10; x = ox; }
        else            { draw_char(fb, fb_w, fb_h, x, y, *s, c); x += 8; }
        s++;
    }
}

static const char *CURSOR[] = {
    "#           ",
    "##          ",
    "#.#         ",
    "#..#        ",
    "#...#       ",
    "#....#      ",
    "#.....#     ",
    "#......#    ",
    "#.......#   ",
    "#........#  ",
    "#....######",
    "#...#       ",
    "#..#        ",
    "###         ",
};
#define CURSOR_H ((int)(sizeof(CURSOR) / sizeof(CURSOR[0])))
#define CURSOR_W 12

void draw_cursor(uint8_t *fb, int x, int y) {
    for (int j = 0; j < CURSOR_H; j++) {
        for (int i = 0; i < CURSOR_W; i++) {
            char c = CURSOR[j][i];
            if (c == '#')      put_px(fb, TOP_W, TOP_H, x + i, y + j, C_BLACK);
            else if (c == '.') put_px(fb, TOP_W, TOP_H, x + i, y + j, C_HI);
        }
    }
}

/*---------------------------------------------------------------------
 * Text block with optional blinking caret (shared by Notepad/About)
 *-------------------------------------------------------------------*/
static int g_caret_tick = 0;

int caret_tick(void) { return g_caret_tick; }

void caret_tick_advance(void) { g_caret_tick = (g_caret_tick + 1) % 60; }

void draw_text_block(uint8_t *fb, int px, int py, int pw, int ph,
                     const char *text, int text_len,
                     int draw_caret, int caret_blink) {
    int chars_per_line = (pw - 6) / 8;
    if (chars_per_line < 1) chars_per_line = 1;
    int max_lines = (ph - 4) / 10;

    int col = 0, line = 0;
    int tx = px + 3, ty = py + 2;
    for (int i = 0; i < text_len && line < max_lines; i++) {
        char ch = text[i];
        if (ch == '\n') { line++; col = 0; continue; }
        if (col >= chars_per_line) { line++; col = 0; if (line >= max_lines) break; }
        draw_char(fb, TOP_W, TOP_H, tx + col * 8, ty + line * 10, ch, C_TEXT);
        col++;
    }
    if (draw_caret && caret_blink && line < max_lines) {
        int cx = tx + col * 8;
        int cy = ty + line * 10;
        vline(fb, TOP_W, TOP_H, cx, cy, 8, C_TEXT);
    }
}

/*---------------------------------------------------------------------
 * Window manager
 *-------------------------------------------------------------------*/
Window g_wins[NUM_WINS];
int    g_z[NUM_WINS] = {WK_ABOUT, WK_FILES, WK_CALC, WK_MINES, WK_PAINT, WK_NOTEPAD, WK_BASIC};

int g_uptime_ticks = 0;
int g_is_new3ds = 0;

void detect_model(void) {
    u8 model = 0;
    cfguInit();
    if (R_SUCCEEDED(CFGU_GetSystemModel(&model))) {
        /* CFG_MODEL_N3DS(2), N3DSXL(4), N2DSXL(5) → New; rest → old */
        g_is_new3ds = (model == CFG_MODEL_N3DS || model == CFG_MODEL_N3DSXL || model == CFG_MODEL_N2DSXL) ? 1 : 0;
    }
    cfguExit();
}

int is_new3ds(void) {
    return g_is_new3ds;
}

void bring_to_front(int k) {
    int pos = -1;
    for (int i = 0; i < NUM_WINS; i++) if (g_z[i] == k) { pos = i; break; }
    if (pos < 0 || pos == NUM_WINS - 1) return;
    int v = g_z[pos];
    for (int i = pos; i < NUM_WINS - 1; i++) g_z[i] = g_z[i + 1];
    g_z[NUM_WINS - 1] = v;
}

int topmost_open_any(void) {
    for (int i = NUM_WINS - 1; i >= 0; i--) {
        if (g_wins[g_z[i]].open) return g_z[i];
    }
    return -1;
}

int topmost_open_input_window(void) {
    for (int i = NUM_WINS - 1; i >= 0; i--) {
        Window *w = &g_wins[g_z[i]];
        if (!w->open) continue;
        if (w->kind == WK_NOTEPAD) return g_z[i];
        if (w->kind == WK_PAINT && w->focus_filename) return g_z[i];
        if (w->kind == WK_BASIC) return g_z[i];
    }
    return -1;
}

void set_status(Window *w, const char *s) {
    strncpy(w->status, s, sizeof(w->status) - 1);
    w->status[sizeof(w->status) - 1] = '\0';
    w->status_ttl = 120;
}

void ensure_artemis_dir(void) {
    mkdir("sdmc:/3ds", 0777);
    mkdir(ARTEMIS_DIR, 0777);
}

int note_text_save(const char *fname, const char *text, int len) {
    if (!fname || fname[0] == '\0') return 0;
    char path[128];
    snprintf(path, sizeof(path), "%s/%s", ARTEMIS_DIR, fname);
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    int ok = 0;
    if (len > 0) ok = fwrite(text, 1, len, f) == (size_t)len;
    fclose(f);
    return ok;
}

int note_text_load(const char *fname, char *text, int cap, int *out_len) {
    if (!fname || fname[0] == '\0') return 0;
    char path[128];
    snprintf(path, sizeof(path), "%s/%s", ARTEMIS_DIR, fname);
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    int n = (int)fread(text, 1, cap - 1, f);
    if (n < 0) n = 0;
    text[n] = '\0';
    if (out_len) *out_len = n;
    fclose(f);
    return 1;
}

int note_delete(const char *fname) {
    char path[128];
    snprintf(path, sizeof(path), "%s/%s", ARTEMIS_DIR, fname);
    return remove(path) == 0;
}

void win_close_rect(Window *w, int *cx, int *cy, int *cw, int *ch) {
    *cw = 14; *ch = 10;
    *cx = w->x + w->w - *cw - 3;
    *cy = w->y + 2;
}

int win_hit(Window *w, int mx, int my) {
    if (!w->open) return 0;
    int cx, cy, cw, ch;
    win_close_rect(w, &cx, &cy, &cw, &ch);
    if (mx >= cx && mx < cx + cw && my >= cy && my < cy + ch) return 1;
    if (mx >= w->x && mx < w->x + w->w &&
        my >= w->y && my < w->y + 17) return 2;
    if (mx >= w->x && mx < w->x + w->w &&
        my >= w->y && my < w->y + w->h) return 3;
    return 0;
}

int is_valid_filename_char(char c) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9')) return 1;
    if (c == '.' || c == '_' || c == '-') return 1;
    return 0;
}
