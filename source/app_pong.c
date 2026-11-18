#include "app.h"
#include "icons.h"
#include "audio.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define PADDLE_W   6
#define PADDLE_H   36
#define BALL_SZ    4
#define WIN_SCORE  5

typedef struct {
    int paddle_y;      /* left player paddle center (circle pad) */
    int r_paddle_y;    /* right paddle center (touchscreen)      */
    int ball_x, ball_y;
    int ball_vx, ball_vy;
    int left_score, right_score;
    int game_over;
    int winner;        /* 0 = left, 1 = right */
    int serve_timer;
} PongState;

static PongState g_pong_state;

static int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void pong_reset(Window *w) {
    w->kind = WK_PONG;
    w->open = 1;
    w->x = 80; w->y = 30;
    w->w = 240; w->h = 170;
    strcpy(w->title, "Pong");
    w->app_state = &g_pong_state;
    PongState *s = (PongState *)w->app_state;
    s->paddle_y = w->y + w->h / 2;
    s->r_paddle_y = w->y + w->h / 2;
    s->ball_x = w->x + w->w / 2;
    s->ball_y = w->y + w->h / 2;
    s->ball_vx = 3;
    s->ball_vy = 2;
    s->left_score = 0;
    s->right_score = 0;
    s->game_over = 0;
    s->winner = 0;
    s->serve_timer = 0;
    bring_to_front(WK_PONG);
}

static void pong_start_ball(PongState *s, Window *w) {
    s->ball_x = w->x + w->w / 2;
    s->ball_y = w->y + w->h / 2;
    s->ball_vx = (s->winner == 0) ? -3 : 3;
    s->ball_vy = 2;
    if (s->ball_y < w->y + 40) s->ball_vy = 2;
    if (s->ball_y > w->y + w->h - 40) s->ball_vy = -2;
}

static void pong_update(Window *w) {
    PongState *s = (PongState *)w->app_state;
    if (!w->open) return;

    int court_top = w->y + 26;
    int court_bottom = w->y + w->h - 10;
    int half_pad = PADDLE_H / 2;

    if (s->game_over) {
        uint32_t down = hidKeysDown();
        if (down & (KEY_A | KEY_B | KEY_START | KEY_SELECT | KEY_TOUCH)) {
            s->left_score = 0;
            s->right_score = 0;
            s->game_over = 0;
            s->serve_timer = 60;
            s->winner = 0;
            pong_start_ball(s, w);
        }
        return;
    }

    /* --- Left paddle: circle pad --- */
    circlePosition cp;
    hidCircleRead(&cp);
    if (cp.dy > 40)  s->paddle_y += 3;
    if (cp.dy < -40) s->paddle_y -= 3;
    s->paddle_y = clampi(s->paddle_y, court_top + half_pad, court_bottom - half_pad);

    /* --- Right paddle: touchscreen --- */
    uint32_t held = hidKeysHeld();
    if (held & KEY_TOUCH) {
        touchPosition tp;
        hidTouchRead(&tp);
        /* bottom screen Y (0-240) -> court Y */
        int target = w->y + 26 + (tp.py * (court_bottom - court_top) / 240);
        int diff = target - s->r_paddle_y;
        if (diff > 3) s->r_paddle_y += 3;
        else if (diff < -3) s->r_paddle_y -= 3;
    }
    s->r_paddle_y = clampi(s->r_paddle_y, court_top + half_pad, court_bottom - half_pad);

    /* --- Serve delay --- */
    if (s->serve_timer > 0) {
        s->serve_timer--;
        if (s->serve_timer == 0) pong_start_ball(s, w);
        return;
    }

    /* --- Move ball --- */
    s->ball_x += s->ball_vx;
    s->ball_y += s->ball_vy;

    /* Top / bottom wall bounce */
    if (s->ball_y <= court_top + BALL_SZ) {
        s->ball_y = court_top + BALL_SZ;
        s->ball_vy = -s->ball_vy;
    }
    if (s->ball_y >= court_bottom - BALL_SZ) {
        s->ball_y = court_bottom - BALL_SZ;
        s->ball_vy = -s->ball_vy;
    }

    /* Left paddle collision */
    if (s->ball_vx < 0 && s->ball_x <= w->x + 12 + PADDLE_W) {
        if (s->ball_y >= s->paddle_y - half_pad - BALL_SZ &&
            s->ball_y <= s->paddle_y + half_pad + BALL_SZ) {
            s->ball_x = w->x + 12 + PADDLE_W;
            s->ball_vx = -s->ball_vx;
            if (s->ball_vx > 0) s->ball_vx += 1;  /* speed up */
            int hit = s->ball_y - s->paddle_y;
            s->ball_vy = hit / 4;
        }
    }

    /* Right paddle collision */
    if (s->ball_vx > 0 && s->ball_x >= w->x + w->w - 12 - PADDLE_W) {
        if (s->ball_y >= s->r_paddle_y - half_pad - BALL_SZ &&
            s->ball_y <= s->r_paddle_y + half_pad + BALL_SZ) {
            s->ball_x = w->x + w->w - 12 - PADDLE_W;
            s->ball_vx = -s->ball_vx;
            if (s->ball_vx < 0) s->ball_vx -= 1;
            int hit = s->ball_y - s->r_paddle_y;
            s->ball_vy = hit / 4;
        }
    }

    /* --- Score --- */
    if (s->ball_x < w->x + 4) {
        s->right_score++;
        s->winner = 1;
        s->serve_timer = 70;
        s->ball_vx = 0;
        s->ball_vy = 0;
        sfx_play(SFX_DING);
    } else if (s->ball_x > w->x + w->w - 4) {
        s->left_score++;
        s->winner = 0;
        s->serve_timer = 70;
        s->ball_vx = 0;
        s->ball_vy = 0;
        sfx_play(SFX_DING);
    }

    if (s->left_score >= WIN_SCORE || s->right_score >= WIN_SCORE) {
        s->game_over = 1;
    }
}

static void pong_draw(uint8_t *fb, Window *w, int focused) {
    (void)focused;
    PongState *s = (PongState *)w->app_state;

    int court_top = w->y + 26;
    int court_bottom = w->y + w->h - 10;
    int court_left = w->x + 10;
    int court_right = w->x + w->w - 10;

    /* Court background */
    fill_rect(fb, TOP_W, TOP_H, court_left, court_top,
              court_right - court_left, court_bottom - court_top, C_BLACK);
    bevel(fb, TOP_W, TOP_H, court_left, court_top,
          court_right - court_left, court_bottom - court_top, 1);

    /* Net (dashed center line) */
    for (int cy = court_top + 4; cy < court_bottom; cy += 12) {
        fill_rect(fb, TOP_W, TOP_H,
                  w->x + w->w / 2 - 1, cy, 2, 8, C_DIM);
    }

    /* Left paddle (circle pad) */
    int lp_x = court_left + 2;
    int lp_y = s->paddle_y - PADDLE_H / 2;
    fill_rect(fb, TOP_W, TOP_H, lp_x, lp_y, PADDLE_W, PADDLE_H, C_WIN);

    /* Right paddle (touchscreen) */
    int rp_x = court_right - 2 - PADDLE_W;
    int rp_y = s->r_paddle_y - PADDLE_H / 2;
    fill_rect(fb, TOP_W, TOP_H, rp_x, rp_y, PADDLE_W, PADDLE_H, C_WIN);

    /* Ball */
    fill_rect(fb, TOP_W, TOP_H,
              s->ball_x - BALL_SZ / 2, s->ball_y - BALL_SZ / 2,
              BALL_SZ, BALL_SZ, C_WIN);

    /* Scores */
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", s->left_score);
    draw_text(fb, TOP_W, TOP_H, w->x + w->w / 2 - 40, court_top + 2, buf, C_WIN);
    snprintf(buf, sizeof(buf), "%d", s->right_score);
    draw_text(fb, TOP_W, TOP_H, w->x + w->w / 2 + 24, court_top + 2, buf, C_WIN);

    if (s->serve_timer > 0 && !s->game_over) {
        draw_text(fb, TOP_W, TOP_H, w->x + w->w / 2 - 24,
                  court_top + court_bottom / 2 - 4, "SERVE", C_DIM);
    }

    if (s->game_over) {
        fill_rect(fb, TOP_W, TOP_H,
                  court_left + 20, court_bottom / 2 - 12,
                  court_right - court_left - 40, 24, C_TITLE);
        const char *msg = s->winner == 0 ? "PLAYER 1 WINS!" : "PLAYER 2 WINS!";
        draw_text(fb, TOP_W, TOP_H,
                  w->x + w->w / 2 - (int)strlen(msg) * 4,
                  court_bottom / 2 - 6, msg, C_TITLETXT);
        draw_text(fb, TOP_W, TOP_H,
                  w->x + w->w / 2 - 40, court_bottom / 2 + 10,
                  "Press any key", C_DIM);
    }
}

static void pong_click(Window *w, int mx, int my) {
    (void)w; (void)mx; (void)my;
}

static int pong_load_file(Window *w, const char *fname) {
    (void)w; (void)fname;
    return 0;
}

const App g_app_pong = {
    "Pong",
    pong_reset,
    pong_draw,
    pong_click,
    NULL,          /* input */
    pong_update,
    pong_load_file,
    NULL,          /* secondary_click */
    NULL,          /* bottom_draw */
    NULL,          /* bottom_hit */
    NULL,          /* bottom_action */
};
