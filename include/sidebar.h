#ifndef SIDEBAR_H
#define SIDEBAR_H

#include "app_state.h"
#include "gui_core.h"
#include <stdbool.h>

/* ══════════════════════════════════════════════════════════════════════
 * Sidebar Rendering and Interaction
 * ══════════════════════════════════════════════════════════════════════ */

/* Draw the sidebar */
void sidebar_draw(app_state_t *app);

/* Handle mouse click in sidebar */
bool sidebar_handle_click(app_state_t *app, int mx, int my);

/* Handle mouse move in sidebar */
void sidebar_handle_hover(app_state_t *app, int mx, int my);

/* Handle mouse wheel in sidebar */
void sidebar_handle_wheel(app_state_t *app, int delta);

/* Get sidebar content height for scrolling */
int sidebar_content_height(app_state_t *app);

#endif /* SIDEBAR_H */