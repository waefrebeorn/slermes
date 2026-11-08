/*
 * tui_views.h - TUI modal views (help, cron, gateway, logs, skills, agent).
 *
 * Module-internal header (tui_fullscreen.c <-> tui_views.c).
 */
#ifndef TUI_VIEWS_H
#define TUI_VIEWS_H

#include <stdbool.h>
#include "tui_state.h"   /* tui_history_add */

void tui_draw_agent_info(void);
void tui_draw_cron_viewer(void);
void tui_draw_gateway_status(void);
void tui_draw_log_viewer(void);
void tui_draw_skill_browser(void);
const char * tui_tool_shelf(const char *tool_name);

#endif /* TUI_VIEWS_H */
