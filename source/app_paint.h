#ifndef APP_PAINT_H
#define APP_PAINT_H

#include "core.h"

/* Paint-specific entry points needed by the shell (main.c). Paint keeps its
 * layout types and interaction state private; these are the only touch points. */

int paint_touch_draw_enabled(void);
int paint_is_stroking(void);

/* Start a stylus/cursor stroke at top-screen coords (gx, gy). Computes the
 * canvas-relative position internally. Safe to call when paint is not open. */
void paint_cursor_start(Window *w, int gx, int gy);

/* Per-frame continuation of a cursor-driven stroke while a mouse button is
 * held. still_held should reflect whether A/L/touch-pad is currently down. */
void paint_cursor_stroke_frame(Window *w, int gx, int gy, int still_held);

/* Bottom-screen stylus input handler. Call once per frame with the current
 * and previous touch state. */
void paint_bottom_touch(int tx, int ty, int touching, int prev_touching);

/* Draw the paint bottom-screen UI. */
void draw_paint_bottom(uint8_t *bot, Window *w);

#endif /* APP_PAINT_H */
