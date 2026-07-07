#ifndef CHAT_VIEW_H
#define CHAT_VIEW_H

#include "app_state.h"
#include "gui_core.h"
#include <stdbool.h>

/* ══════════════════════════════════════════════════════════════════════
 * Chat View Rendering and Interaction
 * ══════════════════════════════════════════════════════════════════════ */

/* Draw the chat area */
void chat_view_draw(app_state_t *app);

/* Handle mouse click in chat area */
bool chat_view_handle_click(app_state_t *app, int mx, int my);

/* Handle mouse move in chat area */
void chat_view_handle_hover(app_state_t *app, int mx, int my);

/* Handle mouse wheel in chat area */
void chat_view_handle_wheel(app_state_t *app, int delta);

/* Handle keyboard input in chat area */
bool chat_view_handle_key(app_state_t *app, int key, int mod, const char *text);

/* Get chat content height for scrolling */
int chat_content_height(app_state_t *app);

/* Scroll to bottom */
void chat_view_scroll_to_bottom(app_state_t *app);

#endif /* CHAT_VIEW_H */