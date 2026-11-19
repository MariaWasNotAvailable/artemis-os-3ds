#include "app.h"
#include "terminal.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define RPS_INPUT_MAX 256

typedef enum {
    RPS_ROCK,
    RPS_PAPER,
    RPS_SCISSORS
} RpsChoice;

typedef struct {
    Terminal terminal;
    char inbuf[RPS_INPUT_MAX];
    int inlen;
} RpsState;

static RpsState g_rps_state;

static void rps_say(RpsState *s, const char *text) {
    terminal_print(&s->terminal, text);
    terminal_putc(&s->terminal, '\n');
}

static int rps_word_matches(const char *text, size_t length, const char *word) {
    if (strlen(word) != length) return 0;
    for (size_t i = 0; i < length; i++) {
        if (tolower((unsigned char)text[i]) != (unsigned char)word[i]) return 0;
    }
    return 1;
}

static int rps_parse_choice(const char *text, RpsChoice *choice) {
    while (isspace((unsigned char)*text)) text++;
    size_t length = strlen(text);
    while (length > 0 && isspace((unsigned char)text[length - 1])) length--;

    if (length == 1) {
        switch (tolower((unsigned char)text[0])) {
        case 'r': *choice = RPS_ROCK; return 1;
        case 'p': *choice = RPS_PAPER; return 1;
        case 's': *choice = RPS_SCISSORS; return 1;
        default: return 0;
        }
    }
    if (rps_word_matches(text, length, "rock")) {
        *choice = RPS_ROCK;
        return 1;
    }
    if (rps_word_matches(text, length, "paper")) {
        *choice = RPS_PAPER;
        return 1;
    }
    if (rps_word_matches(text, length, "scissors")) {
        *choice = RPS_SCISSORS;
        return 1;
    }
    return 0;
}

static void rps_prompt(RpsState *s) {
    rps_say(s, "Choose rock, paper, or scissors (R/P/S):");
}

static void rps_handle_line(RpsState *s) {
    static const char * const names[] = {"Rock", "Paper", "Scissors"};
    RpsChoice player;
    if (!rps_parse_choice(s->inbuf, &player)) {
        rps_say(s, "Invalid choice. Enter Rock, Paper, or Scissors (R/P/S).");
        rps_prompt(s);
        return;
    }

    RpsChoice computer = (RpsChoice)(rand() % 3);
    rps_say(s, "You chose:");
    rps_say(s, names[player]);
    rps_say(s, "Computer chose:");
    rps_say(s, names[computer]);

    if (player == computer)
        rps_say(s, "It's a tie!");
    else if ((player + 1) % 3 == computer)
        rps_say(s, "You lose!");
    else
        rps_say(s, "You win!");
    rps_prompt(s);
}

static void rps_reset(Window *w) {
    terminal_window_init(w, WK_RPS, "Rock Paper Scissors", &g_rps_state);

    RpsState *s = &g_rps_state;
    memset(s, 0, sizeof(*s));
    terminal_init(&s->terminal, w);
    terminal_clear(&s->terminal);
    if (g_shift_auto) {
        g_shift = 0;
        g_shift_auto = 0;
    }
    srand((unsigned)time(NULL));
    rps_say(s, "Rock Paper Scissors");
    rps_prompt(s);
    bring_to_front(WK_RPS);
}

static void rps_draw(uint8_t *fb, Window *w, int focused) {
    (void)focused;
    RpsState *s = (RpsState *)w->app_state;
    Color white = {255, 255, 255};
    Color black = {0, 0, 0};
    terminal_draw(fb, w, &s->terminal, NULL, white, black, 1, NULL);
}

static void rps_input(Window *w, char c) {
    RpsState *s = (RpsState *)w->app_state;
    if (c == '\n') {
        s->inbuf[s->inlen] = '\0';
        terminal_putc(&s->terminal, '\n');
        rps_handle_line(s);
        s->inlen = 0;
        s->inbuf[0] = '\0';
    } else if (c == '\b' || (c >= 32 && c <= 126)) {
        terminal_edit_input(&s->terminal, s->inbuf, &s->inlen,
                            sizeof(s->inbuf), c);
    }
}

const App g_app_rps = {
    "Rock Paper Scissors",
    rps_reset,
    rps_draw,
    NULL,
    rps_input,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};
