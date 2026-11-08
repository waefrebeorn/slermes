/*
 * tui_slash_cmds.h - TUI slash-command handlers and their registry.
 *
 * Module-internal header (tui_fullscreen.c <-> tui_slash_cmds.c).
 */
#ifndef TUI_SLASH_CMDS_H
#define TUI_SLASH_CMDS_H

#include "tui_slash_worker.h"   /* slash_cmd_entry_t, SLASH_CAT_* */

extern const slash_cmd_entry_t slash_registry[];

#endif /* TUI_SLASH_CMDS_H */
