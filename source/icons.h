#ifndef ICONS_H
#define ICONS_H

#include "core.h"

#define ICON_SZ       16
#define ICON_SMALL_SZ 8

/*---------------------------------------------------------------------
 * Palette / rendering
 *-------------------------------------------------------------------*/
Color icon_pal(char c);
void draw_icon(uint8_t *fb, int fb_w, int fb_h, int x, int y,
               const char * const *map);
void draw_icon_small(uint8_t *fb, int fb_w, int fb_h, int x, int y,
                     const char * const *map);

/*---------------------------------------------------------------------
 * Icon lookup by window kind
 *-------------------------------------------------------------------*/
const char * const *icon_for_kind(int kind);
const char * const *icon_small_for_kind(int kind);
const char * const *icon_for_shutdown(void);

/*---------------------------------------------------------------------
 * Tool icons (16x16 -> 8x8 small) used by Paint
 *-------------------------------------------------------------------*/
extern const char *ICON_TOOL_PEN[];
extern const char *ICON_TOOL_BRUSH[];
extern const char *ICON_TOOL_ERASER[];
extern const char *ICON_TOOL_FILL[];
extern const char *ICON_TOOL_LINE[];
extern const char *ICON_TOOL_TOUCH[];

/*---------------------------------------------------------------------
 * Desktop icons
 *-------------------------------------------------------------------*/
typedef struct {
    int x, y;
    const char *name;
    WKind kind;
    const char * const *icon;
} DesktopIcon;

extern const DesktopIcon g_icons[];
extern int NUM_ICONS;

void draw_desktop(uint8_t *fb);
int  desktop_icon_hit(int mx, int my, WKind *out_kind);

#endif /* ICONS_H */
