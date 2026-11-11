#include "icons.h"

#include <string.h>

/*---------------------------------------------------------------------
 * 8-bit palette used by the char-based icon maps
 *-------------------------------------------------------------------*/
Color icon_pal(char c) {
    switch (c) {
        case 'K': return (Color){0,   0,   0  };
        case 'W': return (Color){255, 255, 255};
        case 'N': return (Color){0,   0,   128};
        case 'B': return (Color){60,  120, 220};
        case 'L': return (Color){200, 200, 200};
        case 'D': return (Color){100, 100, 100};
        case 'G': return (Color){140, 190, 130};
        case 'Y': return (Color){245, 215, 90};
        case 'O': return (Color){200, 150, 40};
        case 'R': return (Color){200, 40,  40};
        case 'S': return (Color){150, 150, 150};
        default:  return (Color){0,   0,   0  };
    }
}

void draw_icon(uint8_t *fb, int fb_w, int fb_h, int x, int y,
               const char * const *map) {
    for (int j = 0; j < ICON_SZ; j++) {
        const char *row = map[j];
        for (int i = 0; i < ICON_SZ; i++) {
            char ch = row[i];
            if (ch == '.' || ch == '\0') continue;
            put_px(fb, fb_w, fb_h, x + i, y + j, icon_pal(ch));
        }
    }
}

void draw_icon_small(uint8_t *fb, int fb_w, int fb_h, int x, int y,
                     const char * const *map) {
    for (int j = 0; j < ICON_SMALL_SZ; j++) {
        const char *row = map[j];
        for (int i = 0; i < ICON_SMALL_SZ; i++) {
            char ch = row[i];
            if (ch == '.' || ch == '\0') continue;
            put_px(fb, fb_w, fb_h, x + i, y + j, icon_pal(ch));
        }
    }
}

/*---------------------------------------------------------------------
 * 16x16 application icons
 *-------------------------------------------------------------------*/
static const char * const ICON_NOTES[ICON_SZ] = {
    "................",
    ".KKKKKKKKKKK....",
    ".KWWWWWWWWWSK...",
    ".KWWWWWWWWWWSK..",
    ".KNNNNNNNNWWWSK.",
    ".KWWWWWWWWWWWWK.",
    ".KNNNNNNNNNNNWK.",
    ".KWWWWWWWWWWWWK.",
    ".KNNNNNNNNNNNWK.",
    ".KWWWWWWWWWWWWK.",
    ".KNNNNNNNNNNNWK.",
    ".KWWWWWWWWWWWWK.",
    ".KNNNNNNNNNNNWK.",
    ".KWWWWWWWWWWWWK.",
    ".KKKKKKKKKKKKKK.",
    "................",
};

static const char * const ICON_CALC[ICON_SZ] = {
    "................",
    ".KKKKKKKKKKKKKK.",
    ".KLLLLLLLLLLLLK.",
    ".KLGGGGGGGGGGLK.",
    ".KLGGGGGGGGGGLK.",
    ".KLLLLLLLLLLLLK.",
    ".KLWWLWWLWWLRRK.",
    ".KLLLLLLLLLLLLK.",
    ".KLWWLWWLWWLRRK.",
    ".KLLLLLLLLLLLLK.",
    ".KLWWLWWLWWLRRK.",
    ".KLLLLLLLLLLLLK.",
    ".KLWWLWWLWWLRRK.",
    ".KLLLLLLLLLLLLK.",
    ".KKKKKKKKKKKKKK.",
    "................",
};

static const char * const ICON_FILES[ICON_SZ] = {
    "................",
    "................",
    "...KKKKKK.......",
    "...KOOOOK.......",
    ".KKKOOOOKKKKKKK.",
    ".KYYYYYYYYYYYYK.",
    ".KYYYYYYYYYYYYK.",
    ".KYYYYYYYYYYYYK.",
    ".KYYYYYYYYYYYYK.",
    ".KYYYYYYYYYYYYK.",
    ".KYYYYYYYYYYYYK.",
    ".KYYYYYYYYYYYYK.",
    ".KOOOOOOOOOOOOK.",
    ".KKKKKKKKKKKKKK.",
    "................",
};

static const char * const ICON_ABOUT[ICON_SZ] = {
    "................",
    ".....KKKKKK.....",
    "...KKBBBBBBKK...",
    "..KBBBBBBBBBBK..",
    ".KBBBBBWWBBBBBK.",
    ".KBBBBBWWBBBBBK.",
    ".KBBBBBBBBBBBBK.",
    ".KBBBBBWWBBBBBK.",
    ".KBBBBBWWBBBBBK.",
    ".KBBBBBWWBBBBBK.",
    ".KBBBBBWWBBBBBK.",
    ".KBBBBBWWBBBBBK.",
    ".KBBBBBBBBBBBBK.",
    "..KBBBBBBBBBBK..",
    "...KKBBBBBBKK...",
    ".....KKKKKK.....",
};

static const char * const ICON_PAINT[ICON_SZ] = {
    "................",
    "....KKKKKKK.....",
    "...KWWWWWWWKK...",
    "..KWWRRRWGGGWK..",
    ".KWWRRRWWGGGWWK.",
    ".KWWRRRWWGGGWWK.",
    ".KWWWWWWWWWWWWK.",
    ".KWWBBBWWYYYWWK.",
    ".KWWBBBWWYYYWWK.",
    ".KWWWWWWWWWWWWK.",
    "..KWWWWWWWWWWK..",
    "...KWWWWWWWWWK..",
    "....KWWWWWWWWK..",
    "....KKWWWWWWK...",
    "......KKKKKK....",
    "................",
};

static const char * const ICON_MINES[ICON_SZ] = {
    "................",
    "................",
    ".........KK.....",
    ".........KRK....",
    "..........KYK...",
    "....KKKK..KR....",
    "...KKKKKKKK.....",
    "..KKKKWWKKKK....",
    "..KKKKKKKKKK....",
    "..KKKKKKKKKK....",
    "..KKKKKKKKKK....",
    "..KKKKKKKKKK....",
    "...KKKKKKKK.....",
    "....KKKKKK......",
    "................",
    "................",
};

static const char * const ICON_BASIC[ICON_SZ] = {
    "................",
    ".......K........",
    "......KKK.......",
    ".....KWKWK......",
    "....KWKWWWK....",
    "...KWKWWWWWK...",
    "..KWKWKWKWKWK..",
    ".KWKWKWKWKWKWK.",
    ".KWKWKWKWKWKWK.",
    "..KWKWKWKWKWK..",
    "...KWKWWWWWK...",
    "....KWKWWWK....",
    ".....KWKWK.....",
    "......KKK......",
    ".......K.......",
    "................",
};

const char * const *icon_for_kind(int kind) {
    switch (kind) {
        case WK_NOTEPAD: return ICON_NOTES;
        case WK_CALC:    return ICON_CALC;
        case WK_FILES:   return ICON_FILES;
        case WK_ABOUT:   return ICON_ABOUT;
        case WK_PAINT:   return ICON_PAINT;
        case WK_MINES:   return ICON_MINES;
        case WK_BASIC:   return ICON_BASIC;
    }
    return ICON_ABOUT;
}

static const char * const ICON_BASIC_S[ICON_SMALL_SZ] = {
    "........",
    "...K....",
    "..KKK...",
    ".KWKWK..",
    "KWKWWWKK",
    ".KWKWK..",
    "..KKK...",
    "........",
};
static const char * const ICON_POWER[] = {
    "................",
    ".......KK.......",
    ".......KK.......",
    "..KK...KK...KK..",
    ".KRRK..KK..KRRK.",
    ".KRRK..KK..KRRK.",
    "KRRK........KRRK",
    "KRRK........KRRK",
    "KRRK........KRRK",
    "KRRK........KRRK",
    "KRRK........KRRK",
    ".KRRK......KRRK.",
    ".KRRK......KRRK.",
    "..KRRKKKKKKRRK..",
    "...KRRRRRRRRK...",
    "....KKKKKKKK....",
    "................",
};

const char * const *icon_for_shutdown(void) {
    return ICON_POWER;
}

/*---------------------------------------------------------------------
 * 8x8 small icons (title bar / taskbar)
 *-------------------------------------------------------------------*/
static const char * const ICON_NOTES_S[ICON_SMALL_SZ] = {
    "KKKKKKK.",
    "KWWWWWSK",
    "KNNNNWSK",
    "KWWWWWWK",
    "KNNNNNWK",
    "KWWWWWWK",
    "KNNNNNWK",
    "KKKKKKKK",
};

static const char * const ICON_CALC_S[ICON_SMALL_SZ] = {
    "KKKKKKKK",
    "KGGGGGGK",
    "KGGGGGGK",
    "KKKKKKKK",
    "KWLWLWRK",
    "KLLLLLLK",
    "KWLWLWRK",
    "KKKKKKKK",
};

static const char * const ICON_FILES_S[ICON_SMALL_SZ] = {
    "........",
    "KKK.....",
    "KYYKKKKK",
    "KYYYYYYK",
    "KYYYYYYK",
    "KYYYYYYK",
    "KOOOOOOK",
    "KKKKKKKK",
};

static const char * const ICON_ABOUT_S[ICON_SMALL_SZ] = {
    ".KKKKKK.",
    "KBBWWBBK",
    "KBBBBBBK",
    "KBBWWBBK",
    "KBBWWBBK",
    "KBBWWBBK",
    "KBBBBBBK",
    ".KKKKKK.",
};

static const char * const ICON_PAINT_S[ICON_SMALL_SZ] = {
    ".KKKKKK.",
    "KRRKGGKK",
    "KRRKGGWK",
    "KWWWWWWK",
    "KBBKYYWK",
    "KBBKYYWK",
    "KWWWWWK.",
    ".KKKKKK.",
};

static const char * const ICON_MINES_S[ICON_SMALL_SZ] = {
    "......KR",
    ".....KYK",
    ".....K..",
    ".KKKK...",
    "KKKKKKK.",
    "KKKWKKK.",
    "KKKKKKK.",
    ".KKKKK..",
};

const char * const *icon_small_for_kind(int kind) {
    switch (kind) {
        case WK_NOTEPAD: return ICON_NOTES_S;
        case WK_CALC:    return ICON_CALC_S;
        case WK_FILES:   return ICON_FILES_S;
        case WK_ABOUT:   return ICON_ABOUT_S;
        case WK_PAINT:   return ICON_PAINT_S;
        case WK_MINES:   return ICON_MINES_S;
        case WK_BASIC:   return ICON_BASIC_S;
    }
    return ICON_ABOUT_S;
}

/*---------------------------------------------------------------------
 * Paint tool icons (8x8)
 *-------------------------------------------------------------------*/
const char *ICON_TOOL_PEN[ICON_SMALL_SZ] = {
    "......KK",
    ".....KYK",
    "....KYYK",
    "...KYWLK",
    "..KLWLK.",
    ".KLLLK..",
    "KKKKK...",
    ".KK.....",
};

const char *ICON_TOOL_BRUSH[ICON_SMALL_SZ] = {
    "......KK",
    ".....KKK",
    "....KNNK",
    "...KNNK.",
    "..KBBNK.",
    ".KKBBBK.",
    "KLWWBK..",
    "KKKKK...",
};

const char *ICON_TOOL_ERASER[ICON_SMALL_SZ] = {
    "........",
    "..KKKKK.",
    ".KRRRRRK",
    "KRRRRRRK",
    "KWWWWWWK",
    "KWWWWWK.",
    ".KKKKK..",
    "........",
};

const char *ICON_TOOL_FILL[ICON_SMALL_SZ] = {
    ".KK.....",
    ".KB.....",
    "KKBK....",
    "KBBBK...",
    "KBBBBKK.",
    ".KBBBBBK",
    "..KBBBBK",
    "...KKKK.",
};

const char *ICON_TOOL_LINE[ICON_SMALL_SZ] = {
    "........",
    ".KK.....",
    "KKKK....",
    ".KKKK...",
    "..KKKK..",
    "...KKKK.",
    "....KKKK",
    ".....KK.",
};

const char *ICON_TOOL_TOUCH[ICON_SMALL_SZ] = {
    ".....KK.",
    "....KLK.",
    "...KLLK.",
    "..KLLK..",
    ".KWLK...",
    ".KWK....",
    "KKKKKKKK",
    "DDDDDDDD",
};

/*---------------------------------------------------------------------
 * Desktop icons
 *-------------------------------------------------------------------*/
const DesktopIcon g_icons[] = {
    { 12,  10, "Notes", WK_NOTEPAD, ICON_NOTES  },
    { 12,  58, "Calc",  WK_CALC,    ICON_CALC   },
    { 12, 106, "Files", WK_FILES,   ICON_FILES  },
    { 12, 154, "About", WK_ABOUT,   ICON_ABOUT  },
    { 56,  10, "Paint", WK_PAINT,   ICON_PAINT  },
    { 56,  58, "Mines", WK_MINES,   ICON_MINES  },
    { 56, 106, "Basic", WK_BASIC,   ICON_BASIC  },
};
int NUM_ICONS = (int)(sizeof(g_icons) / sizeof(g_icons[0]));

void draw_desktop(uint8_t *fb) {
    fill_rect(fb, TOP_W, TOP_H, 0, 0, TOP_W, DESK_H, C_DESKTOP);
    for (int i = 0; i < NUM_ICONS; i++) {
        const DesktopIcon *ic = &g_icons[i];
        draw_icon(fb, TOP_W, TOP_H, ic->x, ic->y, ic->icon);

        int lw = (int)strlen(ic->name) * 8;
        int tx = ic->x + (ICON_SZ - lw) / 2;
        int ty = ic->y + ICON_SZ + 2;
        draw_text(fb, TOP_W, TOP_H, tx + 1, ty + 1, ic->name, C_BLACK);
        draw_text(fb, TOP_W, TOP_H, tx,     ty,     ic->name, C_TITLETXT);
    }
}

int desktop_icon_hit(int mx, int my, WKind *out_kind) {
    for (int i = 0; i < NUM_ICONS; i++) {
        const DesktopIcon *ic = &g_icons[i];
        if (mx >= ic->x && mx < ic->x + ICON_SZ &&
            my >= ic->y && my < ic->y + ICON_SZ + 12) {
            *out_kind = ic->kind;
            return 1;
        }
    }
    return 0;
}
