#include "app.h"
#include "icons.h"
#include "audio.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>

#include "stb_image.h"

#define PHOTO_DIR    "sdmc:/DCIM/"
#define PHOTO_SUFFIX ".jpg"

typedef struct {
    char files[MAX_FILES][FNAME_MAX];
    char folders[MAX_FILES][16];
    int  count;
    int  sel;
    int  scroll;
    int  view;          /* 0 = file list, 1 = full image */
    int  img_w, img_h;
    uint8_t *img;       /* decoded rgb8 (malloc'd via stb_image) or NULL */
    int  nav_timer;
} PhotoState;

static PhotoState g_photo_state;

/*======================================================================
 * JPEG decoding via stb_image
 *====================================================================*/

static uint8_t *jpeg_decode_mem(const uint8_t *data, int len, int *w, int *h) {
    int comp = 0;
    /* Force 3 components (RGB). stb_image mallocs the buffer. */
    uint8_t *rgb = stbi_load_from_memory(data, len, w, h, &comp, 3);
    return rgb;
}

/*======================================================================
 * File listing
 *====================================================================*/

static int ends_with_ci(const char *s, const char *suffix) {
    int sl = (int)strlen(s), xl = (int)strlen(suffix);
    if (xl > sl) return 0;
    const char *a = s + sl - xl;
    for (int i = 0; i < xl; i++) {
        char ca = a[i], cb = suffix[i];
        if (ca >= 'A' && ca <= 'Z') ca = ca - 'A' + 'a';
        if (cb >= 'A' && cb <= 'Z') cb = cb - 'A' + 'a';
        if (ca != cb) return 0;
    }
    return 1;
}

static void photo_refresh(Window *w) {
    PhotoState *s = (PhotoState *)w->app_state;
    s->count = 0;
    /* 3DS camera stores photos in sdmc:/DCIM/100NIN03 .. 999NIN03.
       Stop at the first missing folder once we've found at least one, to
       avoid spamming FS errors and to keep refresh fast. */
    int found_any = 0;
    for (int f = 100; f <= 999 && s->count < MAX_FILES; f++) {
        char dir[64];
        snprintf(dir, sizeof(dir), "%s%03dNIN03", PHOTO_DIR, f);
        DIR *d = opendir(dir);
        if (!d) {
            if (found_any) break;
            continue;
        }
        found_any = 1;
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            const char *n = e->d_name;
            if (n[0] == '.') continue;
            if (!ends_with_ci(n, PHOTO_SUFFIX)) continue;
            if (s->count >= MAX_FILES) break;
            size_t nlen = strlen(n);
            if (nlen > FNAME_MAX - 1) nlen = FNAME_MAX - 1;
            memcpy(s->files[s->count], n, nlen);
            s->files[s->count][nlen] = '\0';
            snprintf(s->folders[s->count], sizeof(s->folders[s->count]), "%03dNIN03", f);
            s->count++;
        }
        closedir(d);
        if (s->count >= MAX_FILES) break;
    }
    if (s->sel >= s->count) s->sel = s->count - 1;
    if (s->sel < 0) s->sel = 0;
}

static void photo_free_image(PhotoState *s) {
    if (s->img) { stbi_image_free(s->img); s->img = NULL; }
    s->img_w = s->img_h = 0;
}

static void photo_load_image(Window *w, const char *name) {
    PhotoState *s = (PhotoState *)w->app_state;
    photo_free_image(s);
    char path[160];
    snprintf(path, sizeof(path), "%s%s/%s", PHOTO_DIR, s->folders[s->sel], name);
    FILE *f = fopen(path, "rb");
    if (!f) { set_status(w, "Open failed."); return; }
    fseek(f, 0, SEEK_END);
    long flen = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (flen <= 0 || flen > (16 * 1024 * 1024)) {
        fclose(f);
        set_status(w, "Bad file size.");
        return;
    }
    uint8_t *buf = (uint8_t *)malloc((size_t)flen);
    if (!buf) { fclose(f); set_status(w, "No memory."); return; }
    size_t rd = fread(buf, 1, (size_t)flen, f);
    fclose(f);
    if (rd != (size_t)flen) { free(buf); set_status(w, "Read failed."); return; }

    int wdt = 0, hgt = 0;
    uint8_t *img = jpeg_decode_mem(buf, (int)rd, &wdt, &hgt);
    free(buf);
    if (!img) { set_status(w, "Decode failed."); return; }

    s->img = img;
    s->img_w = wdt;
    s->img_h = hgt;
    s->view = 1;
    set_status(w, name);
    sfx_play(SFX_DING);
}

/*======================================================================
 * Layout / drawing
 *====================================================================*/

static void photo_layout(Window *w,
                         int *lx, int *ly, int *lw, int *lh,
                         int *bty, int *bth) {
    *lx = w->x + 5;
    *ly = w->y + 20;
    *lw = w->w - 10;
    *lh = w->h - 25 - 20;
    *bty = *ly + *lh + 2;
    *bth = 16;
}

static const char *PH_LABELS[2] = { "View", "Refresh" };
static const int   PH_WIDTHS[2] = { 44, 56 };

static void draw_photo_list(uint8_t *fb, Window *w) {
    PhotoState *s = (PhotoState *)w->app_state;
    int lx, ly, lw, lh, bty, bth;
    photo_layout(w, &lx, &ly, &lw, &lh, &bty, &bth);

    fill_rect(fb, TOP_W, TOP_H, lx, ly, lw, lh, C_WIN);
    bevel(fb, TOP_W, TOP_H, lx, ly, lw, lh, 0);

    int line_h = 10;
    int rows = (lh - 4) / line_h;
    if (rows < 1) rows = 1;
    if (s->sel < s->scroll) s->scroll = s->sel;
    if (s->sel >= s->scroll + rows)
        s->scroll = s->sel - rows + 1;

    if (s->count == 0) {
        draw_text(fb, TOP_W, TOP_H, lx + 4, ly + 4, "(no photos)", C_DIM);
        draw_text(fb, TOP_W, TOP_H, lx + 4, ly + 16,
                  "Put JPEGs in", C_DIM);
        draw_text(fb, TOP_W, TOP_H, lx + 4, ly + 26,
                  PHOTO_DIR "NNNNIN03", C_DIM);
    } else {
        int count_shown = s->count - s->scroll;
        if (count_shown > rows) count_shown = rows;
        for (int i = 0; i < count_shown; i++) {
            int idx = s->scroll + i;
            int ry = ly + 2 + i * line_h;
            if (idx == s->sel) {
                fill_rect(fb, TOP_W, TOP_H, lx + 2, ry - 1, lw - 4, line_h, C_MENU_HI);
                draw_text(fb, TOP_W, TOP_H, lx + 4, ry, s->files[idx], C_TITLETXT);
            } else {
                draw_text(fb, TOP_W, TOP_H, lx + 4, ry, s->files[idx], C_TEXT);
            }
        }
    }

    int btx = lx;
    for (int i = 0; i < 2; i++) {
        fill_rect(fb, TOP_W, TOP_H, btx, bty, PH_WIDTHS[i], bth, C_BTN);
        bevel(fb, TOP_W, TOP_H, btx, bty, PH_WIDTHS[i], bth, 1);
        int lwlab = (int)strlen(PH_LABELS[i]) * 8;
        int tx = btx + (PH_WIDTHS[i] - lwlab) / 2;
        draw_text(fb, TOP_W, TOP_H, tx, bty + 4, PH_LABELS[i], C_TEXT);
        btx += PH_WIDTHS[i] + 3;
    }

    if (w->status_ttl > 0 && w->status[0]) {
        draw_text(fb, TOP_W, TOP_H, btx + 4, bty + 4, w->status, C_TITLE);
    }
}

static void draw_photo_image(uint8_t *fb, Window *w) {
    PhotoState *s = (PhotoState *)w->app_state;
    int lx, ly, lw, lh, bty, bth;
    photo_layout(w, &lx, &ly, &lw, &lh, &bty, &bth);
    (void)bty; (void)bth;

    fill_rect(fb, TOP_W, TOP_H, lx, ly, lw, lh, C_BLACK);
    bevel(fb, TOP_W, TOP_H, lx, ly, lw, lh, 0);

    if (!s->img || s->img_w <= 0 || s->img_h <= 0) {
        draw_text(fb, TOP_W, TOP_H, lx + 4, ly + 4, "(cannot display)", C_DIM);
        draw_text(fb, TOP_W, TOP_H, lx + 4, ly + 16,
                  s->sel >= 0 && s->sel < s->count ? s->files[s->sel] : "", C_DIM);
        goto draw_buttons;
    }

    /* fit image to inner rect (above the button strip), preserve aspect */
    int btn_h = 20;
    int img_h = lh - btn_h - 6;
    float scale = (float)lw / (float)s->img_w;
    float sh = (float)img_h / (float)s->img_h;
    if (sh < scale) scale = sh;
    int rw = (int)(s->img_w * scale);
    int rh = (int)(s->img_h * scale);
    if (rw < 1) rw = 1;
    if (rh < 1) rh = 1;
    int ox = lx + (lw - rw) / 2;
    int oy = ly + (img_h - rh) / 2 + 3;

    for (int py = 0; py < rh; py++) {
        int sy = (py * s->img_h) / rh;
        for (int px = 0; px < rw; px++) {
            int sx = (px * s->img_w) / rw;
            const uint8_t *p = &s->img[(sy * s->img_w + sx) * 3];
            put_px(fb, TOP_W, TOP_H, ox + px, oy + py,
                   (Color){ p[0], p[1], p[2] });
        }
    }

draw_buttons:;
    int by = ly + lh - btn_h - 2;
    const char *labels[3] = { "Back", "Prev", "Next" };
    int widths[3] = { 44, 44, 44 };
    int btx = lx;
    for (int i = 0; i < 3; i++) {
        fill_rect(fb, TOP_W, TOP_H, btx, by, widths[i], btn_h, C_BTN);
        bevel(fb, TOP_W, TOP_H, btx, by, widths[i], btn_h, 1);
        int lwlab = (int)strlen(labels[i]) * 8;
        int tx = btx + (widths[i] - lwlab) / 2;
        draw_text(fb, TOP_W, TOP_H, tx, by + (btn_h - 8) / 2, labels[i], C_TEXT);
        btx += widths[i] + 3;
    }
}

static void photo_draw(uint8_t *fb, Window *w, int focused) {
    (void)focused;
    PhotoState *s = (PhotoState *)w->app_state;
    if (s->view == 1) draw_photo_image(fb, w);
    else              draw_photo_list(fb, w);
}

/*======================================================================
 * Click handling
 *====================================================================*/

typedef enum {
    PH_MISS = 0, PH_LIST, PH_VIEW, PH_REFRESH
} PHHit;

static PHHit photo_inner_hit(Window *w, int mx, int my, int *out_idx) {
    PhotoState *s = (PhotoState *)w->app_state;
    int lx, ly, lw, lh, bty, bth;
    photo_layout(w, &lx, &ly, &lw, &lh, &bty, &bth);

    if (mx >= lx && mx < lx + lw && my >= ly && my < ly + lh) {
        int line_h = 10;
        int rows = (lh - 4) / line_h;
        int rel = (my - (ly + 2)) / line_h;
        if (rel >= 0 && rel < rows) {
            int idx = s->scroll + rel;
            if (idx >= 0 && idx < s->count) {
                *out_idx = idx;
                return PH_LIST;
            }
        }
    }
    int btx = lx;
    for (int i = 0; i < 2; i++) {
        if (mx >= btx && mx < btx + PH_WIDTHS[i] &&
            my >= bty && my < bty + bth) {
            return (i == 0) ? PH_VIEW : PH_REFRESH;
        }
        btx += PH_WIDTHS[i] + 3;
    }
    return PH_MISS;
}

static void photo_adjust_sel(Window *w, int step) {
    PhotoState *s = (PhotoState *)w->app_state;
    if (s->count == 0) return;
    int ns = s->sel + step;
    if (ns < 0) ns = s->count - 1;
    if (ns >= s->count) ns = 0;
    if (ns != s->sel || s->img == NULL) {
        s->sel = ns;
        photo_load_image(w, s->files[s->sel]);
    }
}

static void photo_click(Window *w, int mx, int my) {
    PhotoState *s = (PhotoState *)w->app_state;
    int lx, ly, lw, lh, bty, bth;
    photo_layout(w, &lx, &ly, &lw, &lh, &bty, &bth);

    if (s->view == 1) {
        /* bottom button strip: Back / Prev / Next */
        int btn_h = 20;
        int by = ly + lh - btn_h - 2;
        int widths[3] = { 44, 44, 44 };
        int btx = lx;
        if (my >= by && my < by + btn_h) {
            for (int i = 0; i < 3; i++) {
                if (mx >= btx && mx < btx + widths[i]) {
                    if (i == 0) { s->view = 0; photo_free_image(s); }
                    else if (i == 1) photo_adjust_sel(w, -1);
                    else         photo_adjust_sel(w, +1);
                    return;
                }
                btx += widths[i] + 3;
            }
        }
        return;
    }

    int idx = -1;
    switch (photo_inner_hit(w, mx, my, &idx)) {
        case PH_LIST:
            if (idx == s->sel) {
                photo_load_image(w, s->files[s->sel]);
            } else {
                s->sel = idx;
            }
            break;
        case PH_VIEW:
            if (s->count > 0 && s->sel >= 0 && s->sel < s->count)
                photo_load_image(w, s->files[s->sel]);
            break;
        case PH_REFRESH:
            photo_refresh(w);
            set_status(w, "Refreshed.");
            break;
        default: break;
    }
}

/*======================================================================
 * Update (image-view navigation) / reset / vtable
 *====================================================================*/

static void photo_update(Window *w) {
    PhotoState *s = (PhotoState *)w->app_state;
    if (s->view != 1) return;
    uint32_t held = hidKeysHeld();
    s->nav_timer++;
    if ((held & KEY_DRIGHT) && (s->nav_timer % 8 == 0)) photo_adjust_sel(w, +1);
    if ((held & KEY_DLEFT)  && (s->nav_timer % 8 == 0)) photo_adjust_sel(w, -1);
}

static void photo_reset(Window *w) {
    w->kind = WK_PHOTO;
    w->open = 1;
    w->x = 40; w->y = 10;
    w->w = 320; w->h = 200;
    strcpy(w->title, "Photos");
    w->app_state = &g_photo_state;
    PhotoState *s = (PhotoState *)w->app_state;
    s->sel = 0; s->scroll = 0; s->view = 0;
    s->img = NULL; s->img_w = 0; s->img_h = 0;
    s->nav_timer = 0;
    w->status[0] = '\0';
    w->status_ttl = 0;
    photo_refresh(w);
    bring_to_front(WK_PHOTO);
}

static int photo_load_file(Window *w, const char *fname) {
    (void)w; (void)fname;
    return 0;
}

const App g_app_photo = {
    "Photos",
    photo_reset,
    photo_draw,
    photo_click,
    NULL,          /* input */
    photo_update,
    photo_load_file,
    NULL,          /* secondary_click */
    NULL,          /* bottom_draw */
    NULL,          /* bottom_hit */
    NULL,          /* bottom_action */
};
