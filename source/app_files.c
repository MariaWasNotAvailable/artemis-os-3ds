#include "app.h"
#include "icons.h"
#include "audio.h"

#include <string.h>
#include <dirent.h>
#include <stdio.h>

typedef struct {
    char files[MAX_FILES][FNAME_MAX];
    int  files_count;
    int  files_sel;
    int  files_scroll;
} FilesState;

static FilesState g_files_state;

static int has_suffix_ci(const char *s, const char *suffix) {
    int sl = (int)strlen(s);
    int xl = (int)strlen(suffix);
    if (xl > sl) return 0;
    for (int i = 0; i < xl; i++) {
        char a = s[sl - xl + i];
        char b = suffix[i];
        if (a >= 'A' && a <= 'Z') a = a - 'A' + 'a';
        if (b >= 'A' && b <= 'Z') b = b - 'A' + 'a';
        if (a != b) return 0;
    }
    return 1;
}

static void files_refresh(Window *w) {
    FilesState *s = (FilesState *)w->app_state;
    s->files_count = 0;
    DIR *d = opendir(ARTEMIS_DIR);
    if (!d) return;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        const char *n = e->d_name;
        if (n[0] == '.') continue;
        if (s->files_count >= MAX_FILES) break;
        size_t nlen = strlen(n);
        if (nlen > FNAME_MAX - 1) nlen = FNAME_MAX - 1;
        memcpy(s->files[s->files_count], n, nlen);
        s->files[s->files_count][nlen] = '\0';
        s->files_count++;
    }
    closedir(d);
    if (s->files_sel >= s->files_count) s->files_sel = s->files_count - 1;
    if (s->files_sel < 0) s->files_sel = 0;
}

/*---------------------------------------------------------------------
 * Layout / drawing
 *-------------------------------------------------------------------*/
typedef enum {
    FE_MISS = 0, FE_LIST, FE_OPEN, FE_DEL, FE_REFRESH
} FeHit;

static void files_layout(Window *w,
                         int *lx, int *ly, int *lw, int *lh,
                         int *by_btn, int *bh_btn) {
    *lx = w->x + 5;
    *ly = w->y + 20;
    *lw = w->w - 10;
    *lh = w->h - 25 - 20;
    *by_btn = *ly + *lh + 2;
    *bh_btn = 16;
}

static const char *FE_LABELS[3] = { "Open", "Del", "Refresh" };
static const int   FE_WIDTHS[3] = {  40,    32,    56        };

static void draw_files_body(uint8_t *fb, Window *w) {
    FilesState *s = (FilesState *)w->app_state;
    int lx, ly, lw, lh, bty, bth;
    files_layout(w, &lx, &ly, &lw, &lh, &bty, &bth);

    fill_rect(fb, TOP_W, TOP_H, lx, ly, lw, lh, C_WIN);
    bevel(fb, TOP_W, TOP_H, lx, ly, lw, lh, 0);

    int line_h = 10;
    int rows = (lh - 4) / line_h;
    if (rows < 1) rows = 1;
    if (s->files_sel < s->files_scroll) s->files_scroll = s->files_sel;
    if (s->files_sel >= s->files_scroll + rows)
        s->files_scroll = s->files_sel - rows + 1;

    if (s->files_count == 0) {
        draw_text(fb, TOP_W, TOP_H, lx + 4, ly + 4, "(no files)", C_DIM);
        draw_text(fb, TOP_W, TOP_H, lx + 4, ly + 16,
                  "Save from Notes to", C_DIM);
        draw_text(fb, TOP_W, TOP_H, lx + 4, ly + 26,
                  "populate this list.", C_DIM);
    } else {
        int count_shown = s->files_count - s->files_scroll;
        if (count_shown > rows) count_shown = rows;
        for (int i = 0; i < count_shown; i++) {
            int idx = s->files_scroll + i;
            int ry = ly + 2 + i * line_h;
            if (idx == s->files_sel) {
                fill_rect(fb, TOP_W, TOP_H, lx + 2, ry - 1, lw - 4, line_h, C_MENU_HI);
                draw_text(fb, TOP_W, TOP_H, lx + 4, ry, s->files[idx], C_TITLETXT);
            } else {
                draw_text(fb, TOP_W, TOP_H, lx + 4, ry, s->files[idx], C_TEXT);
            }
        }
    }

    int btx = lx;
    for (int i = 0; i < 3; i++) {
        fill_rect(fb, TOP_W, TOP_H, btx, bty, FE_WIDTHS[i], bth, C_BTN);
        bevel(fb, TOP_W, TOP_H, btx, bty, FE_WIDTHS[i], bth, 1);
        int lwlab = (int)strlen(FE_LABELS[i]) * 8;
        int tx = btx + (FE_WIDTHS[i] - lwlab) / 2;
        draw_text(fb, TOP_W, TOP_H, tx, bty + 4, FE_LABELS[i], C_TEXT);
        btx += FE_WIDTHS[i] + 3;
    }

    if (w->status_ttl > 0 && w->status[0]) {
        draw_text(fb, TOP_W, TOP_H, btx + 4, bty + 4, w->status, C_TITLE);
    }
}

/*---------------------------------------------------------------------
 * Click handling
 *-------------------------------------------------------------------*/
static FeHit files_inner_hit(Window *w, int mx, int my, int *out_idx) {
    int lx, ly, lw, lh, bty, bth;
    files_layout(w, &lx, &ly, &lw, &lh, &bty, &bth);

    if (mx >= lx && mx < lx + lw && my >= ly && my < ly + lh) {
        int line_h = 10;
        int rows = (lh - 4) / line_h;
        int rel = (my - (ly + 2)) / line_h;
        if (rel >= 0 && rel < rows) {
            int idx = ((FilesState *)w->app_state)->files_scroll + rel;
            if (idx >= 0 && idx < ((FilesState *)w->app_state)->files_count) {
                *out_idx = idx;
                return FE_LIST;
            }
        }
    }
    int btx = lx;
    FeHit codes[3] = { FE_OPEN, FE_DEL, FE_REFRESH };
    for (int i = 0; i < 3; i++) {
        if (mx >= btx && mx < btx + FE_WIDTHS[i] &&
            my >= bty && my < bty + bth) return codes[i];
        btx += FE_WIDTHS[i] + 3;
    }
    return FE_MISS;
}

static void files_open_selected(Window *fe) {
    FilesState *s = (FilesState *)fe->app_state;
    if (s->files_count == 0) return;
    if (s->files_sel < 0 || s->files_sel >= s->files_count) return;
    const char *name = s->files[s->files_sel];

    if (has_suffix_ci(name, ".apt")) {
        Window *pt = &g_wins[WK_PAINT];
        if (!pt->open) open_app(WK_PAINT);
        if (g_app_paint.load_file(pt, name)) {
            set_status(pt, "Loaded."); sfx_play(SFX_DING);
        } else {
            set_status(pt, "Load failed."); sfx_play(SFX_ERROR);
        }
        bring_to_front(WK_PAINT);
    } else {
        Window *np = &g_wins[WK_NOTEPAD];
        if (!np->open) open_app(WK_NOTEPAD);
        if (g_app_notepad.load_file(np, name)) {
            set_status(np, "Loaded."); sfx_play(SFX_DING);
        } else {
            set_status(np, "Load failed."); sfx_play(SFX_ERROR);
        }
        bring_to_front(WK_NOTEPAD);
    }
}

static void files_click(Window *w, int mx, int my) {
    FilesState *s = (FilesState *)w->app_state;
    int idx = -1;
    switch (files_inner_hit(w, mx, my, &idx)) {
        case FE_LIST:
            if (idx == s->files_sel) {
                files_open_selected(w);
            } else {
                s->files_sel = idx;
            }
            break;
        case FE_OPEN:
            files_open_selected(w);
            break;
        case FE_DEL:
            if (s->files_count > 0 && s->files_sel < s->files_count) {
                if (note_delete(s->files[s->files_sel]))
                    set_status(w, "Deleted.");
                else
                    set_status(w, "Delete failed.");
                files_refresh(w);
            }
            break;
        case FE_REFRESH:
            files_refresh(w);
            set_status(w, "Refreshed.");
            break;
        default: break;
    }
}

/*---------------------------------------------------------------------
 * Reset / vtable
 *-------------------------------------------------------------------*/
static void files_reset(Window *w) {
    w->kind = WK_FILES;
    w->open = 1;
    w->x = 60;  w->y = 25;
    w->w = 200; w->h = 175;
    strcpy(w->title, "File Explorer");
    w->app_state = &g_files_state;
    FilesState *s = (FilesState *)w->app_state;
    s->files_sel = 0;
    s->files_scroll = 0;
    w->status[0] = '\0';
    w->status_ttl = 0;
    files_refresh(w);
    bring_to_front(WK_FILES);
}

static void files_draw(uint8_t *fb, Window *w, int focused) {
    (void)focused;
    draw_files_body(fb, w);
}

const App g_app_files = {
    "File Explorer",
    files_reset,
    files_draw,
    files_click,
    NULL,          /* input */
    NULL,          /* update */
    NULL,          /* load_file */
    NULL,          /* secondary_click */
    NULL,          /* bottom_draw */
    NULL,          /* bottom_hit */
    NULL,          /* bottom_action */
};
