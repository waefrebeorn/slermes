#ifndef PET_UI_H
#define PET_UI_H

#include "app_state.h"
#include "gui_core.h"
#include <stdbool.h>

/* ══════════════════════════════════════════════════════════════════════
 * Pet UI Rendering
 * ══════════════════════════════════════════════════════════════════════ */

/* Draw the pet if active */
void pet_ui_draw(app_state_t *app);

/* Update pet animation frame */
void pet_ui_update_animation(app_state_t *app);

/* Derive pet state from app signals */
int pet_ui_derive_state(app_state_t *app);

/* Handle pet click */
bool pet_ui_handle_click(app_state_t *app, int mx, int my);

/* Initialize pet system */
void pet_ui_init(app_state_t *app);

#endif /* PET_UI_H */