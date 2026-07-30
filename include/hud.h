#ifndef HUD_H
#define HUD_H

#include "app_state.h"
#include "gui_core.h"
#include <stdbool.h>

/* ══════════════════════════════════════════════════════════════════════
 * Floating HUD (Heads-Up Display) - transient status indicators
 * ══════════════════════════════════════════════════════════════════════ */

#define HUD_MAX_ITEMS 4

/* Initialize HUD system */
void hud_init(void);

/* Push a HUD item (label, value, color, duration) */
void hud_push(const char *label, const char *value, gc_color_t color, int duration_sec);

/* Draw all active HUD items */
void hud_draw(app_state_t *app);

/* Update HUD (remove expired items) */
void hud_update(void);

#endif /* HUD_H */