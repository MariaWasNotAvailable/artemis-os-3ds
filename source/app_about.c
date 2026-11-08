#include "app.h"
#include "icons.h"
#include "audio.h"

#include <string.h>
#include <stdio.h>
#include "buildinfo.h"

typedef struct {
    char text[512];
    int  text_len;
} AboutState;

static AboutState g_about_state;

static void about_reset(Window *w) {
    w->kind = WK_ABOUT;
    w->open = 1;
    w->x = 100; w->y = 40;
    w->w = 210; w->h = 140;
    strcpy(w->title, "About Artemis OS");
    w->app_state = &g_about_state;
    AboutState *s = (AboutState *)w->app_state;
    snprintf(s->text, sizeof(s->text),
        "Artemis OS 3DS\n"
        "v1.4 build %s\n"
        "\n"
        "A tiny Windows-like\n"
        "shell for the 3DS.\n"
        "\n"
        "Audio: %s\n"
        "Saves: sdmc:/3ds/artemis/",
        BUILD_NUMBER,
        sfx_status());
    s->text_len = (int)strlen(s->text);
    if (s->text_len > (int)sizeof(s->text) - 1)
        s->text_len = (int)sizeof(s->text) - 1;

    bring_to_front(WK_ABOUT);
}

static void about_draw(uint8_t *fb, Window *w, int focused) {
    (void)focused;
    AboutState *s = (AboutState *)w->app_state;
    int px = w->x + 5, py = w->y + 20;
    int pw = w->w - 10, ph = w->h - 25;
    fill_rect(fb, TOP_W, TOP_H, px, py, pw, ph, C_WIN);
    bevel(fb, TOP_W, TOP_H, px, py, pw, ph, 0);
    draw_text_block(fb, px, py, pw, ph, s->text, s->text_len, 0, 0);
}

const App g_app_about = {
    "About",
    about_reset,
    about_draw,
    NULL,          /* click */
    NULL,          /* input */
    NULL,          /* update */
    NULL,          /* load_file */
    NULL,          /* secondary_click */
    NULL,          /* bottom_draw */
    NULL,          /* bottom_hit */
    NULL,          /* bottom_action */
};
