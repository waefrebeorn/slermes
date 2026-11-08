/*
 * tui_input.h - TUI input-buffer editing and autocomplete helpers.
 *
 * Module-internal header (tui_fullscreen.c <-> tui_input.c).
 */
#ifndef TUI_INPUT_H
#define TUI_INPUT_H

#include <stdbool.h>
#include "tui_state.h"   /* tui_redraw_input */

void tui_autocomplete(void);
void tui_input_delete_to_end(void);
void tui_input_delete_word(void);
void tui_input_history_add(const char *line);
void tui_input_init(void);
void tui_input_insert_char(char ch);

#endif /* TUI_INPUT_H */
