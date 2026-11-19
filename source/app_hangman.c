#include "app.h"
#include "terminal.h"
#include "hangman_words.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define HANGMAN_INPUT_MAX 256
#define HANGMAN_MAX_WRONG 6
#define HANGMAN_WORD_CAP  16

typedef struct {
    Terminal terminal;
    char inbuf[HANGMAN_INPUT_MAX];
    int inlen;
    char word[HANGMAN_WORD_CAP];
    int word_len;
    unsigned int guessed;
    int wrong;
    int game_over;
} HangmanState;

static HangmanState g_hangman_state;

static void hangman_say(HangmanState *s, const char *text) {
    terminal_print(&s->terminal, text);
    terminal_putc(&s->terminal, '\n');
}

static void hangman_prompt(HangmanState *s) {
    hangman_say(s, "Guess a letter (A-Z):");
}

static void hangman_draw_stage(HangmanState *s) {
    hangman_say(s, " +---+");
    hangman_say(s, " |   |");
    hangman_say(s, s->wrong >= 1 ? " O   |" : "     |");
    if (s->wrong >= 4)
        hangman_say(s, "/|\\  |");
    else if (s->wrong >= 3)
        hangman_say(s, "/|   |");
    else if (s->wrong >= 2)
        hangman_say(s, " |   |");
    else
        hangman_say(s, "     |");
    if (s->wrong >= 6)
        hangman_say(s, "/ \\  |");
    else if (s->wrong >= 5)
        hangman_say(s, "/    |");
    else
        hangman_say(s, "     |");
    hangman_say(s, "     |");
    hangman_say(s, "=======");
}

static void hangman_show_word(HangmanState *s) {
    terminal_print(&s->terminal, "Word:");
    for (int i = 0; i < s->word_len; i++) {
        terminal_putc(&s->terminal, ' ');
        char ch = (s->guessed & (1u << (s->word[i] - 'A')))
                    ? s->word[i] : '_';
        terminal_putc(&s->terminal, ch);
    }
    terminal_putc(&s->terminal, '\n');
}

static void hangman_show_guessed(HangmanState *s) {
    terminal_print(&s->terminal, "Guessed:");
    int any = 0;
    for (int i = 0; i < 26; i++) {
        if (!(s->guessed & (1u << i))) continue;
        terminal_putc(&s->terminal, ' ');
        terminal_putc(&s->terminal, (char)('A' + i));
        any = 1;
    }
    if (!any) terminal_print(&s->terminal, " none");
    terminal_putc(&s->terminal, '\n');
}

static void hangman_show_state(HangmanState *s) {
    char line[48];
    hangman_draw_stage(s);
    hangman_show_word(s);
    snprintf(line, sizeof(line), "Wrong guesses: %d/%d", s->wrong, HANGMAN_MAX_WRONG);
    hangman_say(s, line);
    hangman_show_guessed(s);
}

static int hangman_won(const HangmanState *s) {
    for (int i = 0; i < s->word_len; i++) {
        if (!(s->guessed & (1u << (s->word[i] - 'A')))) return 0;
    }
    return 1;
}

static void hangman_start(HangmanState *s) {
    terminal_clear(&s->terminal);
    s->inlen = 0;
    s->inbuf[0] = '\0';
    s->guessed = 0;
    s->wrong = 0;
    s->game_over = 0;

    const char *word = HANGMAN_WORDS[rand() % HANGMAN_WORD_COUNT];
    s->word_len = (int)strlen(word);
    for (int i = 0; i < s->word_len; i++)
        s->word[i] = (char)toupper((unsigned char)word[i]);
    s->word[s->word_len] = '\0';

    char line[48];
    hangman_say(s, "Hangman");
    snprintf(line, sizeof(line), "The word has %d letters.", s->word_len);
    hangman_say(s, line);
    hangman_show_state(s);
    hangman_prompt(s);
}

static int hangman_parse_letter(const char *text, char *letter) {
    while (isspace((unsigned char)*text)) text++;
    size_t len = strlen(text);
    while (len > 0 && isspace((unsigned char)text[len - 1])) len--;
    if (len != 1) return 0;

    unsigned char c = (unsigned char)text[0];
    if (c >= 'a' && c <= 'z') c = (unsigned char)(c - 'a' + 'A');
    if (c < 'A' || c > 'Z') return 0;
    *letter = (char)c;
    return 1;
}

static void hangman_handle_line(HangmanState *s) {
    char letter;
    if (s->game_over) {
        if (s->inlen == 0)
            hangman_start(s);
        else
            hangman_say(s, "Press ENTER to play again.");
        return;
    }
    if (!hangman_parse_letter(s->inbuf, &letter)) {
        hangman_say(s, "Enter one letter from A to Z.");
        hangman_prompt(s);
        return;
    }

    unsigned int bit = 1u << (letter - 'A');
    if (s->guessed & bit) {
        hangman_say(s, "You already guessed that letter.");
        hangman_prompt(s);
        return;
    }
    s->guessed |= bit;

    if (strchr(s->word, letter)) {
        hangman_say(s, "Good guess!");
        if (hangman_won(s)) {
            hangman_show_state(s);
            hangman_say(s, "You win!");
            s->game_over = 1;
            hangman_say(s, "Press ENTER to play again.");
            return;
        }
    } else {
        s->wrong++;
        if (s->wrong == HANGMAN_MAX_WRONG) {
            hangman_show_state(s);
            char line[48];
            snprintf(line, sizeof(line), "Game over. Word: %s", s->word);
            hangman_say(s, line);
            s->game_over = 1;
            hangman_say(s, "Press ENTER to play again.");
            return;
        }
        hangman_say(s, "Not in the word.");
    }

    hangman_show_state(s);
    hangman_prompt(s);
}

static void hangman_reset(Window *w) {
    terminal_window_init(w, WK_HANGMAN, "Hangman", &g_hangman_state);

    HangmanState *s = &g_hangman_state;
    memset(s, 0, sizeof(*s));
    terminal_init(&s->terminal, w);
    if (g_shift_auto) {
        g_shift = 0;
        g_shift_auto = 0;
    }
    srand((unsigned)time(NULL));
    hangman_start(s);
    bring_to_front(WK_HANGMAN);
}

static void hangman_draw(uint8_t *fb, Window *w, int focused) {
    (void)focused;
    HangmanState *s = (HangmanState *)w->app_state;
    Color white = {255, 255, 255};
    Color black = {0, 0, 0};
    terminal_draw(fb, w, &s->terminal, NULL, white, black, 1, NULL);
}

static void hangman_input(Window *w, char c) {
    HangmanState *s = (HangmanState *)w->app_state;
    if (c == '\n') {
        s->inbuf[s->inlen] = '\0';
        terminal_putc(&s->terminal, '\n');
        hangman_handle_line(s);
        s->inlen = 0;
        s->inbuf[0] = '\0';
    } else if (c == '\b' || (c >= 32 && c <= 126)) {
        terminal_edit_input(&s->terminal, s->inbuf, &s->inlen,
                            sizeof(s->inbuf), c);
    }
}

const App g_app_hangman = {
    "Hangman",
    hangman_reset,
    hangman_draw,
    NULL,
    hangman_input,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};
