#include "app.h"
#include "icons.h"
#include "terminal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define GUESS_MAX_HIGH 9999
#define GUESS_MAX_TRIES 10
#define GUESS_DEF_HIGH 10
#define GUESS_DEF_TRIES 3
#define GUESS_INPUT_MAX 256

typedef enum {
    GUESS_ASK_HIGH,
    GUESS_ASK_TRIES,
    GUESS_PLAYING,
    GUESS_FINISHED
} GuessStage;

typedef struct {
    Terminal terminal;
    char inbuf[GUESS_INPUT_MAX];
    int inlen;
    GuessStage stage;
    int high;
    int tries_left;
    int secret;
} GuessState;

static GuessState g_guess_state;

static void guess_say(GuessState *s, const char *text) {
    terminal_print(&s->terminal, text);
    terminal_putc(&s->terminal, '\n');
}

static int guess_parse_number(const char *text, int *out, int low, int high) {
    while (*text == ' ' || *text == '\t') text++;
    char *end;
    long value = strtol(text, &end, 10);
    if (end == text) return 0;
    while (*end == ' ' || *end == '\t') end++;
    if (*end || value < low || value > high) return 0;
    *out = (int)value;
    return 1;
}

static void guess_start(GuessState *s) {
    terminal_clear(&s->terminal);
    s->stage = GUESS_ASK_HIGH;
    guess_say(s, "Guess the Number");
    guess_say(s, "Highest possible number (1-9999) [default 10]:");
}

static void guess_finish(GuessState *s, const char *message) {
    guess_say(s, message);
    s->stage = GUESS_FINISHED;
    guess_say(s, "Press ENTER to play again.");
}

static void guess_handle_line(GuessState *s) {
    int value;
    char message[96];

    switch (s->stage) {
    case GUESS_ASK_HIGH:
        if (s->inlen == 0)
            s->high = GUESS_DEF_HIGH;
        else if (guess_parse_number(s->inbuf, &value, 1, GUESS_MAX_HIGH))
            s->high = value;
        else {
            guess_say(s, "Enter a whole number from 1 to 9999, or press ENTER for 10.");
            return;
        }
        s->stage = GUESS_ASK_TRIES;
        guess_say(s, "Number of tries allowed (1-10) [default 3]:");
        break;
    case GUESS_ASK_TRIES:
        if (s->inlen == 0)
            s->tries_left = GUESS_DEF_TRIES;
        else if (guess_parse_number(s->inbuf, &value, 1, GUESS_MAX_TRIES))
            s->tries_left = value;
        else {
            guess_say(s, "Enter a whole number from 1 to 10, or press ENTER for 3.");
            return;
        }
        s->secret = 1 + rand() % s->high;
        s->stage = GUESS_PLAYING;
        snprintf(message, sizeof(message), "I picked a number between 1 and %d.", s->high);
        guess_say(s, message);
        snprintf(message, sizeof(message), "You have %d tries.", s->tries_left);
        guess_say(s, message);
        guess_say(s, "Enter your guess:");
        break;
    case GUESS_PLAYING:
        if (!guess_parse_number(s->inbuf, &value, 1, s->high)) {
            snprintf(message, sizeof(message), "Enter a whole number from 1 to %d.", s->high);
            guess_say(s, message);
            guess_say(s, "Enter your guess:");
            return;
        }
        s->tries_left--;
        if (value == s->secret) {
            guess_finish(s, "Correct! You win!");
            return;
        }
        if (s->tries_left == 0) {
            snprintf(message, sizeof(message), "No tries left! The number was %d.", s->secret);
            guess_finish(s, message);
            return;
        }
        guess_say(s, value < s->secret
                    ? "Too low - the number is higher."
                    : "Too high - the number is lower.");
        snprintf(message, sizeof(message), "You have %d tries left.", s->tries_left);
        guess_say(s, message);
        guess_say(s, "Enter your guess:");
        break;
    case GUESS_FINISHED:
        guess_start(s);
        break;
    }
}

static void guess_reset(Window *w) {
    terminal_window_init(w, WK_GUESS, "Guess the Number", &g_guess_state);

    GuessState *s = &g_guess_state;
    memset(s, 0, sizeof(*s));
    terminal_init(&s->terminal, w);
    if (g_shift_auto) {
        g_shift = 0;
        g_shift_auto = 0;
    }
    srand((unsigned)time(NULL));
    guess_start(s);
    bring_to_front(WK_GUESS);
}

static void guess_draw(uint8_t *fb, Window *w, int focused) {
    (void)focused;
    GuessState *s = (GuessState *)w->app_state;
    Color white = {255, 255, 255};
    Color black = {0, 0, 0};
    terminal_draw(fb, w, &s->terminal, NULL, white, black, 1, NULL);
}

static void guess_input(Window *w, char c) {
    GuessState *s = (GuessState *)w->app_state;
    if (c == '\n') {
        s->inbuf[s->inlen] = '\0';
        terminal_putc(&s->terminal, '\n');
        guess_handle_line(s);
        s->inlen = 0;
        s->inbuf[0] = '\0';
    } else if (c == '\b' || (c >= 32 && c <= 126)) {
        terminal_edit_input(&s->terminal, s->inbuf, &s->inlen,
                            sizeof(s->inbuf), c);
    }
}

const App g_app_guess = {
    "Guess the Number",
    guess_reset,
    guess_draw,
    NULL,
    guess_input,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};
