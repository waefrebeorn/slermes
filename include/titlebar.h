#ifndef TITLEBAR_H
#define TITLEBAR_H

#include "app_state.h"
#include "gui_core.h"
#include <stdbool.h>

/* ══════════════════════════════════════════════════════════════════════
 * Titlebar and Statusbar Rendering and Interaction
 * ══════════════════════════════════════════════════════════════════════ */

/* Titlebar hit test */
#define TITLEBAR_HIT_NONE   0
#define TITLEBAR_HIT_TOOL   1
#define TITLEBAR_HIT_TITLE  2
#define TITLEBAR_HIT_MODEL  3

/* Draw titlebar */
void titlebar_draw(app_state_t *app);

/* Draw statusbar */
void statusbar_draw(app_state_t *app);

/* Handle click in titlebar */
int titlebar_handle_click(app_state_t *app, int mx, int my);

/* Handle hover in titlebar */
void titlebar_handle_hover(app_state_t *app, int mx, int my);

/* Handle click in statusbar */
bool statusbar_handle_click(app_state_t *app, int mx, int my);

/* Tool indices */
#define TOOL_THEME       0
#define TOOL_SIDEBAR     1
#define TOOL_HAPTICS     2
#define TOOL_SETTINGS    3

#endif /* TITLEBAR_H */