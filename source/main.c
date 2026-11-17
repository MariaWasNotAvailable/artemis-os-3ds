#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "core.h"
#include "icons.h"
#include "audio.h"
#include "app.h"
#include "app_paint.h"

/*---------------------------------------------------------------------
 * Cursor (shell state)
 *-------------------------------------------------------------------*/
static int g_cursor_x = TOP_W / 2;
static int g_cursor_y = TOP_H / 2;

/*---------------------------------------------------------------------
 * Keyboard (bottom screen) - shell owned
 *-------------------------------------------------------------------*/
#define KB_Y      120
#define KB_H      120
#define PAD_Y     0
#define PAD_H     120
#define KEY_COLS  10
#define KEY_ROWS  5
#define KEY_W     (BOT_W / KEY_COLS)
#define KEY_H     (KB_H / KEY_ROWS)

#define K_SHIFT -1
#define K_BKSP  -2
#define K_RET   -3
#define K_SPACE -4

typedef struct {
    int  x, y, w, h;
    int  code;
    const char *label;
    int  special;
} Key;

static Key g_keys[48];
static int g_nkeys = 0;
int g_shift = 0;
int g_shift_auto = 0;
static int g_pressed_key = -1;

static char g_lbl_pool[64][4];

static void add_key(int gx, int gy, int span, int code,
                    const char *label, int special) {
    Key *k = &g_keys[g_nkeys++];
    k->x = gx * KEY_W;
    k->y = KB_Y + gy * KEY_H;
    k->w = KEY_W * span;
    k->h = KEY_H;
    k->code  = code;
    k->label = label;
    k->special = special;
}

static void build_keyboard(void) {
    g_nkeys = 0;
    int lb = 0;

    const char *row0 = "1234567890";
    const char *row1 = "qwertyuiop";
    const char *row2 = "asdfghjkl";
    const char *row3 = "zxcvbnm";

    for (int i = 0; i < 10; i++) {
        g_lbl_pool[lb][0] = row0[i]; g_lbl_pool[lb][1] = '\0';
        add_key(i, 0, 1, row0[i], g_lbl_pool[lb], 0); lb++;
    }
    for (int i = 0; i < 10; i++) {
        g_lbl_pool[lb][0] = row1[i]; g_lbl_pool[lb][1] = '\0';
        add_key(i, 1, 1, row1[i], g_lbl_pool[lb], 0); lb++;
    }
    for (int i = 0; i < 9; i++) {
        g_lbl_pool[lb][0] = row2[i]; g_lbl_pool[lb][1] = '\0';
        add_key(i, 2, 1, row2[i], g_lbl_pool[lb], 0); lb++;
    }
    add_key(9, 2, 1, K_BKSP, "<-", 1);

    add_key(0, 3, 1, K_SHIFT, "Sh", 1);
    for (int i = 0; i < 7; i++) {
        g_lbl_pool[lb][0] = row3[i]; g_lbl_pool[lb][1] = '\0';
        add_key(1 + i, 3, 1, row3[i], g_lbl_pool[lb], 0); lb++;
    }
    add_key(8, 3, 2, K_RET, "Ret", 1);

    add_key(0, 4, 8, K_SPACE, "Space", 1);
    g_lbl_pool[lb][0] = '.'; g_lbl_pool[lb][1] = '\0';
    add_key(8, 4, 1, '.', g_lbl_pool[lb], 1); lb++;
    g_lbl_pool[lb][0] = ','; g_lbl_pool[lb][1] = '\0';
    add_key(9, 4, 1, ',', g_lbl_pool[lb], 1); lb++;
}

static int key_at(int tx, int ty) {
    for (int i = 0; i < g_nkeys; i++) {
        Key *k = &g_keys[i];
        if (tx >= k->x && tx < k->x + k->w && ty >= k->y && ty < k->y + k->h)
            return i;
    }
    return -1;
}

static void draw_keyboard(uint8_t *fb) {
    fill_rect(fb, BOT_W, BOT_H, 0, KB_Y, BOT_W, KB_H, C_TASKBAR);
    for (int i = 0; i < g_nkeys; i++) {
        Key *k = &g_keys[i];
        int dn = (i == g_pressed_key) || (k->code == K_SHIFT && g_shift);
        Color base = k->special ? C_KEYSPEC : C_KEY;
        Color c    = dn ? C_KEYDN : base;
        fill_rect(fb, BOT_W, BOT_H, k->x + 2, k->y + 2, k->w - 4, k->h - 4, c);
        bevel(fb, BOT_W, BOT_H, k->x + 2, k->y + 2, k->w - 4, k->h - 4, dn ? 0 : 1);

        const char *lab = k->label;
        char upper[4] = {0};
        if (g_shift && k->code >= 'a' && k->code <= 'z') {
            upper[0] = (char)(k->code - 'a' + 'A');
            lab = upper;
        } else if (g_shift && k->code >= '0' && k->code <= '9') {
            static const char sym[] = "!@#$%?&*()";
            upper[0] = sym[k->code - '0'];
            lab = upper;
        } else if (g_shift && k->code == '.') {
            upper[0] = '\'';
            lab = upper;
        } else if (g_shift && k->code == ',') {
            upper[0] = '"';
            lab = upper;
        }
        int lw = (int)strlen(lab) * 8;
        int tx = k->x + (k->w - lw) / 2;
        int ty = k->y + (k->h - 8) / 2;
        draw_text(fb, BOT_W, BOT_H, tx, ty, lab, C_TEXT);
    }
}

/*---------------------------------------------------------------------
 * Trackpad (bottom screen)
 *-------------------------------------------------------------------*/
static void draw_trackpad(uint8_t *fb) {
    fill_rect(fb, BOT_W, BOT_H, 0, PAD_Y, BOT_W, PAD_H, C_PAD);
    bevel(fb, BOT_W, BOT_H, 4, PAD_Y + 4, BOT_W - 8, PAD_H - 8, 0);
    for (int y = PAD_Y + 20; y < PAD_Y + PAD_H - 12; y += 14) {
        for (int x = 16; x < BOT_W - 16; x += 14) {
            put_px(fb, BOT_W, BOT_H, x,     y,     C_PADDOT);
            put_px(fb, BOT_W, BOT_H, x + 1, y,     C_PADDOT);
            put_px(fb, BOT_W, BOT_H, x,     y + 1, C_PADDOT);
            put_px(fb, BOT_W, BOT_H, x + 1, y + 1, C_PADDOT);
        }
    }
}

/*---------------------------------------------------------------------
 * Keyboard press -> routed to the focused app's input handler
 *-------------------------------------------------------------------*/
static void kbd_press(int ki) {
    Key *k = &g_keys[ki];
    g_pressed_key = ki;
    sfx_play(SFX_CLICK);
    if (k->code == K_SHIFT) { g_shift = !g_shift; return; }

    char c;
    switch (k->code) {
        case K_BKSP:  c = '\b'; break;
        case K_RET:   c = '\n'; break;
        case K_SPACE: c = ' ';  break;
        default:
            c = (char)k->code;
            if (g_shift && c >= 'a' && c <= 'z') {
                c = c - 'a' + 'A';
            } else if (g_shift && c >= '0' && c <= '9') {
                static const char sym[] = "!@#$%?&*()";
                c = sym[c - '0'];
            } else if (g_shift && c == '.') {
                c = '\'';
            } else if (g_shift && c == ',') {
                c = '"';
            }
            break;
    }

    int tw = topmost_open_input_window();
    if (tw >= 0) {
        Window *w = &g_wins[tw];
        if (w->app && w->app->input) w->app->input(w, c);
    }
}

/*---------------------------------------------------------------------
 * Window chrome (title bar, borders, close button) + app body
 *-------------------------------------------------------------------*/
static void draw_window(uint8_t *fb, Window *w, int is_focused) {
    if (!w->open) return;
    int x = w->x, y = w->y, ww = w->w, hh = w->h;

    fill_rect(fb, TOP_W, TOP_H, x, y, ww, hh, C_BTN);
    bevel(fb, TOP_W, TOP_H, x, y, ww, hh, 1);
    bevel(fb, TOP_W, TOP_H, x + 1, y + 1, ww - 2, hh - 2, 1);

    Color title_col = is_focused ? C_TITLE : C_TITLE_INACTIVE;
    fill_rect(fb, TOP_W, TOP_H, x + 3, y + 3, ww - 6, 14, title_col);

    draw_icon_small(fb, TOP_W, TOP_H, x + 5, y + 5, icon_small_for_kind(w->kind));
    draw_text(fb, TOP_W, TOP_H, x + 17, y + 6, w->title, C_TITLETXT);

    int cx, cy, cw, ch;
    win_close_rect(w, &cx, &cy, &cw, &ch);
    fill_rect(fb, TOP_W, TOP_H, cx, cy, cw, ch, C_BTN);
    bevel(fb, TOP_W, TOP_H, cx, cy, cw, ch, 1);
    draw_text(fb, TOP_W, TOP_H, cx + 3, cy + 1, "X", C_TEXT);

    if (w->app && w->app->draw) w->app->draw(fb, w, is_focused);
}

/*---------------------------------------------------------------------
 * Taskbar / Start menu
 *-------------------------------------------------------------------*/
static int g_start_open = 0;

typedef struct { int x, y, w, h; } Rect;
static Rect g_task_rects[NUM_WINS];

static void draw_taskbar(uint8_t *fb) {
    int ty = TOP_H - TASKBAR_H;
    fill_rect(fb, TOP_W, TOP_H, 0, ty, TOP_W, TASKBAR_H, C_TASKBAR);
    bevel(fb, TOP_W, TOP_H, 0, ty, TOP_W, TASKBAR_H, 1);

    int sx = 4, sy = ty + 3, sw = 60, sh = TASKBAR_H - 6;
    fill_rect(fb, TOP_W, TOP_H, sx, sy, sw, sh, C_BTN);
    bevel(fb, TOP_W, TOP_H, sx, sy, sw, sh, g_start_open ? 0 : 1);
    {
        const char *label = "Start";
        int lw = (int)strlen(label) * 8;
        int tx = sx + (sw - lw) / 2;
        if (g_start_open) { tx += 1; }
        draw_text(fb, TOP_W, TOP_H, tx, sy + 4, label, C_TEXT);
    }

    int clock_w = 50;
    int batt_w  = 18;
    int tray_gap = 3;
    int tray_w = clock_w + tray_gap + batt_w;
    int tray_x = TOP_W - tray_w - 4;
    int clock_x = tray_x;
    int batt_x  = clock_x + clock_w + tray_gap;
    int task_area_end = tray_x - 4;

    int open_count = 0;
    for (int i = 0; i < NUM_WINS; i++) if (g_wins[i].open) open_count++;

    int bx = sx + sw + 6;
    if (open_count > 0) {
        int total_avail = task_area_end - bx;
        int gap = 4;
        int desired = 82;
        int max_each = (total_avail - gap * (open_count - 1)) / open_count;
        int bw = desired;
        if (max_each < desired) bw = max_each;
        if (bw < 24) bw = 24;

        int top = topmost_open_any();
        for (int i = 0; i < NUM_WINS; i++) {
            Window *w = &g_wins[i];
            g_task_rects[i].w = 0;
            if (!w->open) continue;
            if (bx + bw > task_area_end) break;

            int bh = sh;
            fill_rect(fb, TOP_W, TOP_H, bx, sy, bw, bh, C_BTN);
            bevel(fb, TOP_W, TOP_H, bx, sy, bw, bh, (i == top) ? 0 : 1);
            draw_icon_small(fb, TOP_W, TOP_H, bx + 3, sy + 3, icon_small_for_kind(w->kind));

            int text_space = bw - 14;
            int max_chars = text_space / 8;
            if (max_chars < 0) max_chars = 0;
            char short_title[16];
            int n = (int)strlen(w->title);
            if (n > max_chars) n = max_chars;
            if (n > (int)sizeof(short_title) - 1) n = (int)sizeof(short_title) - 1;
            memcpy(short_title, w->title, n);
            short_title[n] = '\0';
            draw_text(fb, TOP_W, TOP_H, bx + 14, sy + 4, short_title, C_TEXT);

            g_task_rects[i].x = bx;
            g_task_rects[i].y = sy;
            g_task_rects[i].w = bw;
            g_task_rects[i].h = bh;
            bx += bw + gap;
        }
    } else {
        for (int i = 0; i < NUM_WINS; i++) g_task_rects[i].w = 0;
    }

    time_t now = time(NULL);
    struct tm lt;
    localtime_r(&now, &lt);
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", lt.tm_hour, lt.tm_min);

    int clock_y = ty + 3;
    int clock_h = TASKBAR_H - 6;
    fill_rect(fb, TOP_W, TOP_H, clock_x, clock_y, clock_w, clock_h, C_BTN);
    bevel(fb, TOP_W, TOP_H, clock_x, clock_y, clock_w, clock_h, 0);
    int clock_text_w = (int)strlen(buf) * 8;
    draw_text(fb, TOP_W, TOP_H,
              clock_x + (clock_w - clock_text_w) / 2,
              clock_y + (clock_h - 8) / 2,
              buf, C_TEXT);

    {
        u8 level = 0;
        u8 charging = 0;
        PTMU_GetBatteryLevel(&level);
        PTMU_GetBatteryChargeState(&charging);
        int lvl_max = 5;
        if (level > lvl_max) level = lvl_max;
        int by = clock_y;
        int bh = clock_h;
        int bx = batt_x;
        int bw = batt_w - 2;

        fill_rect(fb, TOP_W, TOP_H, bx, by, bw, bh, C_BTN);
        bevel(fb, TOP_W, TOP_H, bx, by, bw, bh, 0);

        fill_rect(fb, TOP_W, TOP_H, bx + bw, by + 3, 2, bh - 6, C_LO);

        int inner_x = bx + 2;
        int inner_y = by + 2;
        int inner_w = bw - 4;
        int inner_h = bh - 4;
        if (inner_w > 0 && inner_h > 0) {
            int fill_w = (inner_w * level + lvl_max / 2) / lvl_max;
            Color fc;
            if (charging)      fc = (Color){ 255, 220, 60 };
            else if (level >= 4) fc = (Color){ 40,  160, 40 };
            else if (level >= 2) fc = (Color){ 60,  170, 220 };
            else                 fc = (Color){ 210, 40,  40 };
            if (fill_w > 0)
                fill_rect(fb, TOP_W, TOP_H, inner_x, inner_y, fill_w, inner_h, fc);
        }
    }
}

static int start_button_hit(int mx, int my) {
    int ty = TOP_H - TASKBAR_H;
    return (mx >= 4 && mx < 64 && my >= ty + 3 && my < ty + TASKBAR_H - 3);
}

static int task_button_hit(int mx, int my) {
    for (int i = 0; i < NUM_WINS; i++) {
        if (g_task_rects[i].w == 0) continue;
        if (mx >= g_task_rects[i].x && mx < g_task_rects[i].x + g_task_rects[i].w &&
            my >= g_task_rects[i].y && my < g_task_rects[i].y + g_task_rects[i].h)
            return i;
    }
    return -1;
}

#define MENU_W      150
#define MENU_ITEM_H 20

static const char *MENU_ITEMS[] = {
    "Notes",
    "Paint",
    "Calculator",
    "Minesweeper",
    "File Explorer",
    "About...",
    "BASIC",
    "Shut Down",
};
#define MENU_COUNT ((int)(sizeof(MENU_ITEMS) / sizeof(MENU_ITEMS[0])))

static const int MENU_KINDS[] = {
    WK_NOTEPAD, WK_PAINT, WK_CALC, WK_MINES, WK_FILES, WK_ABOUT, WK_BASIC, -1
};

static void menu_rect(int *x, int *y, int *w, int *h) {
    *w = MENU_W;
    *h = MENU_COUNT * MENU_ITEM_H + 6;
    *x = 2;
    *y = TOP_H - TASKBAR_H - *h;
}

static int menu_item_at(int mx, int my) {
    int mx0, my0, mw, mh;
    menu_rect(&mx0, &my0, &mw, &mh);
    if (mx < mx0 || mx >= mx0 + mw || my < my0 || my >= my0 + mh) return -1;
    int rel = my - (my0 + 3);
    if (rel < 0) return -1;
    int idx = rel / MENU_ITEM_H;
    if (idx < 0 || idx >= MENU_COUNT) return -1;
    return idx;
}

static void draw_start_menu(uint8_t *fb, int cursor_x, int cursor_y) {
    if (!g_start_open) return;
    int x, y, w, h;
    menu_rect(&x, &y, &w, &h);
    fill_rect(fb, TOP_W, TOP_H, x, y, w, h, C_MENU);
    bevel(fb, TOP_W, TOP_H, x, y, w, h, 1);

    fill_rect(fb, TOP_W, TOP_H, x + 2, y + 2, 14, h - 4, C_TITLE);
    {
        const char *ribbon = "Artemis OS";
        int n = (int)strlen(ribbon);
        int avail = h - 6;
        int step = n > 0 ? avail / n : 10;
        if (step > 10) step = 10;
        int y0 = y + 4 + (avail - step * n) / 2;
        for (int i = 0; i < n; i++) {
            char s[2] = { ribbon[i], 0 };
            draw_text(fb, TOP_W, TOP_H, x + 5, y0 + i * step, s, C_TITLETXT);
        }
    }

    int hover = menu_item_at(cursor_x, cursor_y);
    for (int i = 0; i < MENU_COUNT; i++) {
        int iy = y + 3 + i * MENU_ITEM_H;
        Color txt = C_TEXT;
        if (i == hover) {
            fill_rect(fb, TOP_W, TOP_H, x + 18, iy, w - 22, MENU_ITEM_H, C_MENU_HI);
            txt = C_TITLETXT;
        }
        const char * const *ic = (MENU_KINDS[i] == -1)
                                   ? icon_for_shutdown()
                                   : icon_for_kind(MENU_KINDS[i]);
        int icon_y = iy + (MENU_ITEM_H - ICON_SZ) / 2;
        draw_icon(fb, TOP_W, TOP_H, x + 20, icon_y, ic);
        draw_text(fb, TOP_W, TOP_H, x + 42, iy + (MENU_ITEM_H - 8) / 2,
                  MENU_ITEMS[i], txt);
    }
}

/*---------------------------------------------------------------------
 * Shell-level click dispatch
 *-------------------------------------------------------------------*/
static int g_drag_idx   = -1;
static int g_drag_off_x = 0;
static int g_drag_off_y = 0;

int g_quit_flag = 0;

static void do_click(int mx, int my) {
    sfx_play(SFX_CLICK);
    if (g_drag_idx >= 0) { g_drag_idx = -1; return; }

    if (g_start_open) {
        int idx = menu_item_at(mx, my);
        if (idx >= 0) {
            g_start_open = 0;
            int kind = MENU_KINDS[idx];
            if (kind == -1) {
                sfx_play(SFX_SHUTDOWN);
                svcSleepThread(700 * 1000 * 1000LL);
                g_quit_flag = 1;
            } else {
                open_app((WKind)kind);
            }
            return;
        }
        g_start_open = 0;
    }

    if (start_button_hit(mx, my)) { g_start_open = !g_start_open; return; }

    int tb = task_button_hit(mx, my);
    if (tb >= 0) {
        if (!g_wins[tb].open) open_app((WKind)tb);
        else                  bring_to_front(tb);
        return;
    }

    for (int i = NUM_WINS - 1; i >= 0; i--) {
        int k = g_z[i];
        Window *w = &g_wins[k];
        if (!w->open) continue;
        int h = win_hit(w, mx, my);
        if (!h) continue;
        if (h == 1) {
            if (k == WK_BASIC && g_shift_auto) { g_shift = 0; g_shift_auto = 0; }
            w->open = 0; return;
        }
        bring_to_front(k);
        if (h == 2) {
            g_drag_idx   = k;
            g_drag_off_x = mx - w->x;
            g_drag_off_y = my - w->y;
            return;
        }
        if (w->app && w->app->click) w->app->click(w, mx, my);
        return;
    }

    WKind ik;
    if (desktop_icon_hit(mx, my, &ik)) { open_app(ik); return; }
}

/*---------------------------------------------------------------------
 * Main
 *-------------------------------------------------------------------*/
int main(void) {
    gfxInitDefault();
    ptmuInit();
    detect_model();
    sfx_init();
    ensure_artemis_dir();

    memset(g_wins, 0, sizeof(g_wins));
    g_wins[WK_NOTEPAD].kind = WK_NOTEPAD;
    g_wins[WK_CALC].kind    = WK_CALC;
    g_wins[WK_ABOUT].kind   = WK_ABOUT;
    g_wins[WK_FILES].kind   = WK_FILES;
    g_wins[WK_PAINT].kind   = WK_PAINT;
    g_wins[WK_MINES].kind   = WK_MINES;
    g_wins[WK_BASIC].kind   = WK_BASIC;

    build_keyboard();

    int prev_touching     = 0;
    int touch_start_x     = 0;
    int touch_start_y     = 0;
    int touch_prev_x      = 0;
    int touch_prev_y      = 0;
    int touch_started_pad = 0;
    int touch_moved       = 0;

    sfx_play(SFX_STARTUP);

    while (aptMainLoop() && !g_quit_flag) {
        g_uptime_ticks++;
        caret_tick_advance();

        for (int i = 0; i < NUM_WINS; i++) {
            if (g_wins[i].status_ttl > 0) g_wins[i].status_ttl--;
        }

        /* per-frame app updates (e.g. Minesweeper timer) */
        for (int i = 0; i < NUM_WINS; i++) {
            if (g_wins[i].open && g_wins[i].app && g_wins[i].app->update)
                g_wins[i].app->update(&g_wins[i]);
        }

        hidScanInput();
        uint32_t kDown = hidKeysDown();
        uint32_t kHeld = hidKeysHeld();

        if (kDown & KEY_SELECT) break;
        if (kDown & KEY_START)  g_start_open = !g_start_open;
        if (kDown & KEY_B) {
            int top = topmost_open_any();
            if (top >= 0) {
                if (g_wins[top].kind == WK_BASIC && g_shift_auto) { g_shift = 0; g_shift_auto = 0; }
                g_wins[top].open = 0;
            }
        }

        int dx = 0, dy = 0;
        if (kHeld & KEY_DLEFT)  dx -= 2;
        if (kHeld & KEY_DRIGHT) dx += 2;
        if (kHeld & KEY_DUP)    dy -= 2;
        if (kHeld & KEY_DDOWN)  dy += 2;
        circlePosition cp;
        hidCircleRead(&cp);
        if (cp.dx >  40) dx += 3;
        if (cp.dx < -40) dx -= 3;
        if (cp.dy >  40) dy -= 3;
        if (cp.dy < -40) dy += 3;
        g_cursor_x += dx;
        g_cursor_y += dy;

        if (kDown & KEY_A) do_click(g_cursor_x, g_cursor_y);
        if (kDown & KEY_L) do_click(g_cursor_x, g_cursor_y);

        if (kDown & KEY_R) {
            int top = topmost_open_any();
            if (top >= 0) {
                Window *w = &g_wins[top];
                if (w->app && w->app->secondary_click)
                    w->app->secondary_click(w, g_cursor_x, g_cursor_y);
            }
        }

        /* Paint cursor-driven stroke continuation (mouse buttons) */
        {
            Window *pw = &g_wins[WK_PAINT];
            int still_held = (kHeld & KEY_A) || (kHeld & KEY_L) ||
                             ((kHeld & KEY_TOUCH) && touch_started_pad);
            paint_cursor_stroke_frame(pw, g_cursor_x, g_cursor_y, still_held);
        }

        int touching = (kHeld & KEY_TOUCH) ? 1 : 0;
        touchPosition tp = {0, 0};
        if (touching) hidTouchRead(&tp);

        int tdraw_active = paint_touch_draw_enabled() &&
                           g_wins[WK_PAINT].open;

        if (tdraw_active) {
            paint_bottom_touch(tp.px, tp.py, touching, prev_touching);
        } else if (touching && !prev_touching) {
            touch_start_x = tp.px;
            touch_start_y = tp.py;
            touch_prev_x  = tp.px;
            touch_prev_y  = tp.py;
            touch_moved   = 0;
            touch_started_pad = (tp.py < PAD_Y + PAD_H) ? 1 : 0;
            if (!touch_started_pad) {
                int ki = key_at(tp.px, tp.py);
                if (ki >= 0) kbd_press(ki);
            } else {
                int top = topmost_open_any();
                if (top == WK_PAINT) {
                    paint_cursor_start(&g_wins[WK_PAINT], g_cursor_x, g_cursor_y);
                }
            }
        } else if (touching && prev_touching) {
            if (touch_started_pad) {
                g_cursor_x += tp.px - touch_prev_x;
                g_cursor_y += tp.py - touch_prev_y;
                if (abs(tp.px - touch_start_x) > 4 ||
                    abs(tp.py - touch_start_y) > 4) touch_moved = 1;
            }
            touch_prev_x = tp.px;
            touch_prev_y = tp.py;
        } else if (!touching && prev_touching) {
            if (touch_started_pad && !touch_moved && !paint_is_stroking()) {
                do_click(g_cursor_x, g_cursor_y);
            }
            g_pressed_key = -1;
        }
        prev_touching = touching;

        if (g_drag_idx >= 0) {
            Window *w = &g_wins[g_drag_idx];
            w->x = g_cursor_x - g_drag_off_x;
            w->y = g_cursor_y - g_drag_off_y;
            if (w->x < 0) w->x = 0;
            if (w->y < 0) w->y = 0;
            if (w->x + w->w > TOP_W)          w->x = TOP_W - w->w;
            if (w->y + w->h > DESK_H - 4)     w->y = DESK_H - 4 - w->h;
            if (!w->open) g_drag_idx = -1;
        }

        if (g_cursor_x < 0)         g_cursor_x = 0;
        if (g_cursor_x > TOP_W - 2) g_cursor_x = TOP_W - 2;
        if (g_cursor_y < 0)         g_cursor_y = 0;
        if (g_cursor_y > TOP_H - 2) g_cursor_y = TOP_H - 2;

        uint8_t *top = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
        uint8_t *bot = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);

        draw_desktop(top);
        int focused = topmost_open_any();
        for (int i = 0; i < NUM_WINS; i++) {
            int k = g_z[i];
            draw_window(top, &g_wins[k], k == focused);
        }
        draw_taskbar(top);
        draw_start_menu(top, g_cursor_x, g_cursor_y);
        draw_cursor(top, g_cursor_x, g_cursor_y);

        if (tdraw_active) {
            draw_paint_bottom(bot, &g_wins[WK_PAINT]);
        } else {
            draw_trackpad(bot);
            draw_keyboard(bot);
        }

        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }

    sfx_exit();
    ptmuExit();
    gfxExit();
    return 0;
}
