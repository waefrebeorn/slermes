#ifndef DESKTOP_CONTROLLER_H
#define DESKTOP_CONTROLLER_H

#include "app_state.h"
#include "gui_core.h"
#include <stdbool.h>

/* ══════════════════════════════════════════════════════════════════════
 * Desktop Controller — boot sequence, gateway status, pane management
 * ══════════════════════════════════════════════════════════════════════ */

typedef enum {
    BOOT_INIT = 0,
    BOOT_CONNECTING,
    BOOT_READY,
    BOOT_ERROR
} boot_state_t;

/* Initialize desktop controller */
void desktop_controller_init(void);

/* Set boot state */
void desktop_controller_set_boot_state(boot_state_t state, const char *msg);

/* Get current boot state */
boot_state_t desktop_controller_get_boot_state(void);

/* Draw boot overlay */
void desktop_controller_draw_boot_overlay(app_state_t *app);

/* Check if boot is complete */
bool desktop_controller_is_ready(void);

/* Set gateway URL */
void desktop_controller_set_gateway_url(const char *url);

/* Set gateway status */
void desktop_controller_set_gateway_connected(bool connected);

/* Set active profile */
void desktop_controller_set_active_profile(const char *profile);

#endif /* DESKTOP_CONTROLLER_H */