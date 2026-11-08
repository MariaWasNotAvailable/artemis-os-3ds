#ifndef CORE_H
#define CORE_H

#include <3ds.h>
#include <stdint.h>
#include <stddef.h>

/*---------------------------------------------------------------------
 * Display constants
 *-------------------------------------------------------------------*/
#define TOP_W 400
#define TOP_H 240
#define BOT_W 320
#define BOT_H 240

#define TASKBAR_H 20
#define DESK_H    (TOP_H - TASKBAR_H)

#define KB_Y      120
#define KB_H      120
#define PAD_Y     0
#define PAD_H     120

#define ARTEMIS_DIR "sdmc:/3ds/artemis"
#define MAX_FILES   48
#define FNAME_MAX   28

/*---------------------------------------------------------------------
 * Colors
 *-------------------------------------------------------------------*/
typedef struct { uint8_t r, g, b; } Color;

extern const Color C_DESKTOP;
extern const Color C_TASKBAR;
extern const Color C_HI;
extern const Color C_LO;
extern const Color C_BLACK;
extern const Color C_WIN;
extern const Color C_TITLE;
extern const Color C_TITLE_INACTIVE;
extern const Color C_TITLETXT;
extern const Color C_TEXT;
extern const Color C_BTN;
extern const Color C_MENU;
extern const Color C_MENU_HI;
extern const Color C_KEY;
extern const Color C_KEYDN;
extern const Color C_KEYSPEC;
extern const Color C_PAD;
extern const Color C_PADDOT;
extern const Color C_CALC_LCD;
extern const Color C_FIELD;
extern const Color C_DIM;

/*---------------------------------------------------------------------
 * Drawing primitives (implemented in core.c)
 *-------------------------------------------------------------------*/
void put_px(uint8_t *fb, int fb_w, int fb_h, int x, int y, Color c);
void fill_rect(uint8_t *fb, int fb_w, int fb_h,
               int x, int y, int w, int h, Color c);
void hline(uint8_t *fb, int fb_w, int fb_h, int x, int y, int w, Color c);
void vline(uint8_t *fb, int fb_w, int fb_h, int x, int y, int h, Color c);
void bevel(uint8_t *fb, int fb_w, int fb_h, int x, int y, int w, int h,
           int raised);
void draw_char(uint8_t *fb, int fb_w, int fb_h, int x, int y, char ch, Color c);
void draw_text(uint8_t *fb, int fb_w, int fb_h, int x, int y,
               const char *s, Color c);
void draw_cursor(uint8_t *fb, int x, int y);
void draw_text_block(uint8_t *fb, int px, int py, int pw, int ph,
                     const char *text, int text_len,
                     int draw_caret, int caret_blink);
int  caret_tick(void);
void caret_tick_advance(void);

/*---------------------------------------------------------------------
 * Window manager
 *-------------------------------------------------------------------*/
struct App;                     /* forward declared in app.h */

typedef enum {
    WK_NOTEPAD = 0,
    WK_CALC    = 1,
    WK_ABOUT   = 2,
    WK_FILES   = 3,
    WK_PAINT   = 4,
    WK_MINES   = 5
} WKind;
#define NUM_WINS 6

typedef struct Window Window;

struct Window {
    WKind kind;
    int  open;
    int  x, y, w, h;
    char title[32];

    char filename[FNAME_MAX];
    int  focus_filename;
    char status[48];
    int  status_ttl;

    const struct App *app;
    void *app_state;
};

extern Window g_wins[NUM_WINS];
extern int    g_z[NUM_WINS];
extern int    g_uptime_ticks;

void bring_to_front(int k);
int  topmost_open_any(void);
int  topmost_open_input_window(void);
void set_status(Window *w, const char *s);

void ensure_artemis_dir(void);

/* Notes storage (shared by Notepad and File Explorer) */
int  note_text_save(const char *fname, const char *text, int len);
int  note_text_load(const char *fname, char *text, int cap, int *out_len);
int  note_delete(const char *fname);

/* Window geometry helpers */
void win_close_rect(Window *w, int *cx, int *cy, int *cw, int *ch);
int  win_hit(Window *w, int mx, int my);

/* Filename validation helper */
int is_valid_filename_char(char c);

#endif /* CORE_H */
