#include "app.h"
#include "icons.h"
#include "audio.h"
#include "core.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MINE_COLS 9
#define MINE_ROWS 9
#define MINE_COUNT 10

#define MINE_BIT_MINE     0x01
#define MINE_BIT_REVEALED 0x02
#define MINE_BIT_FLAGGED  0x04

#define MINE_CELL 16
#define MINE_HEADER_H 22

typedef struct {
    uint8_t mine_cells[MINE_COLS * MINE_ROWS];
    int  mine_state;
    int  mine_flags;
    int  mine_elapsed;
    int  mine_start_tick;
    int  mine_first_click;
} MinesState;

static MinesState g_mines_state;

static int mine_idx(int cx, int cy) { return cy * MINE_COLS + cx; }

static int mine_in_bounds(int cx, int cy) {
    return cx >= 0 && cx < MINE_COLS && cy >= 0 && cy < MINE_ROWS;
}

static int mine_neighbors(Window *w, int cx, int cy) {
    MinesState *s = (MinesState *)w->app_state;
    int n = 0;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            int x = cx + dx, y = cy + dy;
            if (!mine_in_bounds(x, y)) continue;
            if (s->mine_cells[mine_idx(x, y)] & MINE_BIT_MINE) n++;
        }
    }
    return n;
}

static void mine_clear_board(Window *w) {
    MinesState *s = (MinesState *)w->app_state;
    memset(s->mine_cells, 0, sizeof(s->mine_cells));
    s->mine_state = 0;
    s->mine_flags = 0;
    s->mine_elapsed = 0;
    s->mine_first_click = 1;
    s->mine_start_tick = g_uptime_ticks;
}

static void mine_seed(Window *w, int avoid_x, int avoid_y) {
    MinesState *s = (MinesState *)w->app_state;
    int total = MINE_COLS * MINE_ROWS;
    int placed = 0;

    while (placed < MINE_COUNT) {
        int r = rand() % total;
        int cx = r % MINE_COLS;
        int cy = r / MINE_COLS;
        if (s->mine_cells[r] & MINE_BIT_MINE) continue;
        if (abs(cx - avoid_x) <= 1 && abs(cy - avoid_y) <= 1) continue;
        s->mine_cells[r] |= MINE_BIT_MINE;
        placed++;
    }
}

static int mine_check_win(Window *w) {
    MinesState *s = (MinesState *)w->app_state;
    for (int i = 0; i < MINE_COLS * MINE_ROWS; i++) {
        uint8_t c = s->mine_cells[i];
        if (!(c & MINE_BIT_MINE) && !(c & MINE_BIT_REVEALED)) return 0;
    }
    return 1;
}

static void mine_reveal(Window *w, int cx, int cy) {
    MinesState *s = (MinesState *)w->app_state;
    if (!mine_in_bounds(cx, cy)) return;
    uint8_t c = s->mine_cells[mine_idx(cx, cy)];
    if (c & MINE_BIT_REVEALED) return;
    if (c & MINE_BIT_FLAGGED) return;
    if (c & MINE_BIT_MINE) {
        s->mine_cells[mine_idx(cx, cy)] |= MINE_BIT_REVEALED;
        s->mine_state = 2;

        for (int i = 0; i < MINE_COLS * MINE_ROWS; i++) {
            if (s->mine_cells[i] & MINE_BIT_MINE)
                s->mine_cells[i] |= MINE_BIT_REVEALED;
        }
        sfx_play(SFX_ERROR);
        return;
    }

    int stack[MINE_COLS * MINE_ROWS];
    int sp = 0;
    stack[sp++] = mine_idx(cx, cy);
    while (sp > 0) {
        int p = stack[--sp];
        int x = p % MINE_COLS;
        int y = p / MINE_COLS;
        if (s->mine_cells[p] & MINE_BIT_REVEALED) continue;
        if (s->mine_cells[p] & MINE_BIT_FLAGGED) continue;
        if (s->mine_cells[p] & MINE_BIT_MINE) continue;
        s->mine_cells[p] |= MINE_BIT_REVEALED;
        if (mine_neighbors(w, x, y) != 0) continue;

        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0) continue;
                int nx = x + dx, ny = y + dy;
                if (!mine_in_bounds(nx, ny)) continue;
                if (sp < (int)(sizeof(stack) / sizeof(stack[0])))
                    stack[sp++] = mine_idx(nx, ny);
            }
        }
    }
    if (mine_check_win(w)) { s->mine_state = 1; sfx_play(SFX_DING); }
    else                   { sfx_play(SFX_REVEAL); }
}

static void mine_toggle_flag(Window *w, int cx, int cy) {
    MinesState *s = (MinesState *)w->app_state;
    if (!mine_in_bounds(cx, cy)) return;
    if (s->mine_state != 0) return;
    int i = mine_idx(cx, cy);
    uint8_t c = s->mine_cells[i];
    if (c & MINE_BIT_REVEALED) return;
    if (c & MINE_BIT_FLAGGED) {
        s->mine_cells[i] &= ~MINE_BIT_FLAGGED;
        s->mine_flags--;
    } else {
        s->mine_cells[i] |= MINE_BIT_FLAGGED;
        s->mine_flags++;
    }
    sfx_play(SFX_FLAG);
}

/*---------------------------------------------------------------------
 * Layout / hit testing
 *-------------------------------------------------------------------*/
typedef enum { MH_MISS = 0, MH_CELL, MH_FACE } MineHitKind;

typedef struct {
    int  board_x, board_y;
    int  cell;
    int  face_x, face_y, face_sz;
    int  mines_x, mines_y, mines_w, mines_h;
    int  time_x, time_y, time_w, time_h;
} MineLayout;

static void mine_layout(Window *w, MineLayout *L) {
    int bw = MINE_COLS * MINE_CELL;
    int bh = MINE_ROWS * MINE_CELL;
    int client_x = w->x + 5;
    int client_y = w->y + 20;
    int client_w = w->w - 10;
    L->board_x = client_x + (client_w - bw) / 2;
    L->board_y = client_y + MINE_HEADER_H + 2;
    L->cell = MINE_CELL;

    int counter_w = 34, counter_h = 16;
    int header_y = client_y + 2;
    L->mines_x = client_x + 4;
    L->mines_y = header_y + (MINE_HEADER_H - counter_h) / 2;
    L->mines_w = counter_w;
    L->mines_h = counter_h;

    L->time_w  = counter_w;
    L->time_h  = counter_h;
    L->time_x  = client_x + client_w - counter_w - 4;
    L->time_y  = L->mines_y;

    L->face_sz = 18;
    L->face_x  = client_x + (client_w - L->face_sz) / 2;
    L->face_y  = header_y + (MINE_HEADER_H - L->face_sz) / 2;

    (void)bw; (void)bh;
}

static MineHitKind mine_hit(Window *w, int mx, int my, int *out_cx, int *out_cy) {
    MineLayout L;
    mine_layout(w, &L);
    if (mx >= L.face_x && mx < L.face_x + L.face_sz &&
        my >= L.face_y && my < L.face_y + L.face_sz) return MH_FACE;
    int rx = mx - L.board_x;
    int ry = my - L.board_y;
    if (rx < 0 || ry < 0) return MH_MISS;
    int cx = rx / L.cell;
    int cy = ry / L.cell;
    if (!mine_in_bounds(cx, cy)) return MH_MISS;
    *out_cx = cx;
    *out_cy = cy;
    return MH_CELL;
}

/*---------------------------------------------------------------------
 * Drawing
 *-------------------------------------------------------------------*/
static Color mine_num_color(int n) {
    switch (n) {
        case 1: return (Color){0,   0,   255};
        case 2: return (Color){0,   128, 0  };
        case 3: return (Color){255, 0,   0  };
        case 4: return (Color){0,   0,   128};
        case 5: return (Color){128, 0,   0  };
        case 6: return (Color){0,   128, 128};
        case 7: return (Color){0,   0,   0  };
        case 8: return (Color){128, 128, 128};
    }
    return (Color){0, 0, 0};
}

static void draw_counter(uint8_t *fb, int x, int y, int w, int h, int value) {
    fill_rect(fb, TOP_W, TOP_H, x, y, w, h, C_BLACK);
    bevel(fb, TOP_W, TOP_H, x, y, w, h, 0);

    int v = value;
    if (v > 999) v = 999;
    if (v < -99) v = -99;
    char buf[5];
    if (v < 0) snprintf(buf, sizeof(buf), "-%02d", -v);
    else       snprintf(buf, sizeof(buf), "%03d", v);
    int tx = x + (w - 3 * 8) / 2;
    int ty = y + (h - 8) / 2;
    Color red = {255, 40, 40};
    draw_text(fb, TOP_W, TOP_H, tx, ty, buf, red);
}

static void draw_mine_face(uint8_t *fb, int x, int y, int sz, int state) {
    fill_rect(fb, TOP_W, TOP_H, x, y, sz, sz, C_BTN);
    bevel(fb, TOP_W, TOP_H, x, y, sz, sz, 1);
    Color yellow = {245, 215, 90};
    int fx = x + 2, fy = y + 2, fsz = sz - 4;
    for (int j = 0; j < fsz; j++) {
        int dy = j - fsz / 2;
        for (int i = 0; i < fsz; i++) {
            int dx = i - fsz / 2;
            if (dx * dx + dy * dy <= (fsz / 2) * (fsz / 2))
                put_px(fb, TOP_W, TOP_H, fx + i, fy + j, yellow);
        }
    }

    put_px(fb, TOP_W, TOP_H, x + 5, y + 6, C_BLACK);
    put_px(fb, TOP_W, TOP_H, x + sz - 6, y + 6, C_BLACK);

    if (state == 2) {
        put_px(fb, TOP_W, TOP_H, x + 4, y + 5, C_BLACK);
        put_px(fb, TOP_W, TOP_H, x + 6, y + 7, C_BLACK);
        put_px(fb, TOP_W, TOP_H, x + sz - 7, y + 5, C_BLACK);
        put_px(fb, TOP_W, TOP_H, x + sz - 5, y + 7, C_BLACK);
        hline(fb, TOP_W, TOP_H, x + 6, y + sz - 5, sz - 12, C_BLACK);
    } else if (state == 1) {
        hline(fb, TOP_W, TOP_H, x + 4, y + 5, sz - 8, C_BLACK);
        hline(fb, TOP_W, TOP_H, x + 4, y + 7, sz - 8, C_BLACK);
        hline(fb, TOP_W, TOP_H, x + 5, y + sz - 6, sz - 10, C_BLACK);
        put_px(fb, TOP_W, TOP_H, x + 4, y + sz - 7, C_BLACK);
        put_px(fb, TOP_W, TOP_H, x + sz - 5, y + sz - 7, C_BLACK);
    } else {
        hline(fb, TOP_W, TOP_H, x + 5, y + sz - 6, sz - 10, C_BLACK);
        put_px(fb, TOP_W, TOP_H, x + 4, y + sz - 7, C_BLACK);
        put_px(fb, TOP_W, TOP_H, x + sz - 5, y + sz - 7, C_BLACK);
    }
}

static void draw_mine_cell(uint8_t *fb, int x, int y, int cell,
                           uint8_t c, int neighbors, int exploded) {
    int revealed = (c & MINE_BIT_REVEALED) != 0;
    int flagged  = (c & MINE_BIT_FLAGGED)  != 0;
    int is_mine  = (c & MINE_BIT_MINE)     != 0;

    if (revealed) {
        Color bg = exploded ? (Color){200, 40, 40} : (Color){208, 208, 208};
        fill_rect(fb, TOP_W, TOP_H, x, y, cell, cell, bg);
        hline(fb, TOP_W, TOP_H, x, y, cell, C_LO);
        vline(fb, TOP_W, TOP_H, x, y, cell, C_LO);
        if (is_mine) {
            int cx = x + cell / 2, cy = y + cell / 2;
            for (int dy = -3; dy <= 3; dy++) {
                for (int dx = -3; dx <= 3; dx++) {
                    if (dx * dx + dy * dy <= 9)
                        put_px(fb, TOP_W, TOP_H, cx + dx, cy + dy, C_BLACK);
                }
            }
            put_px(fb, TOP_W, TOP_H, cx - 1, cy - 1, C_HI);
        } else if (neighbors > 0) {
            char s[2] = { (char)('0' + neighbors), 0 };
            draw_text(fb, TOP_W, TOP_H, x + (cell - 8) / 2,
                      y + (cell - 8) / 2, s, mine_num_color(neighbors));
        }
    } else {
        fill_rect(fb, TOP_W, TOP_H, x, y, cell, cell, C_BTN);
        bevel(fb, TOP_W, TOP_H, x, y, cell, cell, 1);
        if (flagged) {
            Color red = {200, 40, 40};
            int fx = x + 4, fy = y + 3;
            for (int j = 0; j < 6; j++) {
                hline(fb, TOP_W, TOP_H, fx, fy + j, 6 - j, red);
            }
            vline(fb, TOP_W, TOP_H, x + 4, y + 3, cell - 6, C_BLACK);
            hline(fb, TOP_W, TOP_H, x + 3, y + cell - 4, cell - 6, C_BLACK);
        }
    }
}

static void draw_mines_body(uint8_t *fb, Window *w) {
    MinesState *s = (MinesState *)w->app_state;
    MineLayout L;
    mine_layout(w, &L);

    int client_x = w->x + 5;
    int client_y = w->y + 20;
    int client_w = w->w - 10;
    fill_rect(fb, TOP_W, TOP_H, client_x, client_y, client_w, MINE_HEADER_H + 4, C_BTN);
    bevel(fb, TOP_W, TOP_H, client_x, client_y, client_w, MINE_HEADER_H + 4, 0);

    int mines_left = MINE_COUNT - s->mine_flags;
    draw_counter(fb, L.mines_x, L.mines_y, L.mines_w, L.mines_h, mines_left);
    draw_counter(fb, L.time_x,  L.time_y,  L.time_w,  L.time_h,  s->mine_elapsed);
    draw_mine_face(fb, L.face_x, L.face_y, L.face_sz, s->mine_state);

    int bw = MINE_COLS * L.cell;
    int bh = MINE_ROWS * L.cell;
    bevel(fb, TOP_W, TOP_H, L.board_x - 2, L.board_y - 2, bw + 4, bh + 4, 0);

    for (int y = 0; y < MINE_ROWS; y++) {
        for (int x = 0; x < MINE_COLS; x++) {
            int i = mine_idx(x, y);
            uint8_t c = s->mine_cells[i];
            int n = (c & MINE_BIT_REVEALED) ? mine_neighbors(w, x, y) : 0;
            int exploded = (s->mine_state == 2 && (c & MINE_BIT_MINE) &&
                            (c & MINE_BIT_REVEALED));
            draw_mine_cell(fb, L.board_x + x * L.cell, L.board_y + y * L.cell,
                           L.cell, c, n, exploded);
        }
    }
}

/*---------------------------------------------------------------------
 * Click / secondary click / update
 *-------------------------------------------------------------------*/
static void mines_click(Window *w, int mx, int my) {
    int cx = 0, cy = 0;
    MineHitKind mh = mine_hit(w, mx, my, &cx, &cy);
    if (mh == MH_FACE) {
        mine_clear_board(w);
    } else if (mh == MH_CELL) {
        MinesState *s = (MinesState *)w->app_state;
        if (s->mine_state != 0) {
            mine_clear_board(w);
        } else {
            if (s->mine_first_click) {
                mine_seed(w, cx, cy);
                s->mine_first_click = 0;
                s->mine_start_tick = g_uptime_ticks;
            }
            mine_reveal(w, cx, cy);
        }
    }
}

static int mines_secondary_click(Window *w, int mx, int my) {
    int cx = 0, cy = 0;
    if (mine_hit(w, mx, my, &cx, &cy) == MH_CELL) {
        mine_toggle_flag(w, cx, cy);
        return 1;
    }
    return 0;
}

static void mines_update(Window *w) {
    MinesState *s = (MinesState *)w->app_state;
    if (w->open && s->mine_state == 0 && !s->mine_first_click) {
        s->mine_elapsed = (g_uptime_ticks - s->mine_start_tick) / 60;
        if (s->mine_elapsed > 999) s->mine_elapsed = 999;
    }
}

/*---------------------------------------------------------------------
 * Reset / vtable
 *-------------------------------------------------------------------*/
static void mines_reset(Window *w) {
    w->kind = WK_MINES;
    w->open = 1;
    w->w = 164; w->h = 200;
    w->x = 120;
    w->y = 20;
    strcpy(w->title, "Minesweeper");
    w->app_state = &g_mines_state;
    mine_clear_board(w);
    bring_to_front(WK_MINES);
}

static void mines_draw(uint8_t *fb, Window *w, int focused) {
    (void)focused;
    draw_mines_body(fb, w);
}

const App g_app_mines = {
    "Minesweeper",
    mines_reset,
    mines_draw,
    mines_click,
    NULL,          /* input */
    mines_update,
    NULL,          /* load_file */
    mines_secondary_click,
    NULL,          /* bottom_draw */
    NULL,          /* bottom_hit */
    NULL,          /* bottom_action */
};
