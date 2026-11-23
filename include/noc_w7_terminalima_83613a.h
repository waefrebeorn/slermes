#ifndef NOC_W7_TERMINALIMA_83613A_H
#define NOC_W7_TERMINALIMA_83613A_H

#include <stdbool.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* All integer inputs cross the JSON boundary as decimal strings (libjson
 * numbers are doubles); each is parsed with strtoll. */

/* PoP: w7_terminalima_83613a_kitty_color_hex @ agent/terminal_image.py:kitty_color_hex */
char *w7_terminalima_83613a_kitty_color_hex(const char *image_id);

/* PoP: w7_terminalima_83613a_cell_box @ agent/terminal_image.py:cell_box
 * Returns [cols, rows] for an image at its current pixel size. Caller frees. */
json_t *w7_terminalima_83613a_cell_box(const char *width, const char *height);

/* PoP: w7_terminalima_83613a_kitty_apc @ agent/terminal_image.py:_kitty_apc
 * kitty APC, chunked into <=4096-byte m pieces. Caller frees. */
char *w7_terminalima_83613a_kitty_apc(const char *ctrl, const char *data);

/* PoP: w7_terminalima_83613a_grid_for @ agent/terminal_image.py:_grid_for
 * Cell grid [cols, rows] never wider than the image nor past the caps. Caller frees. */
json_t *w7_terminalima_83613a_grid_for(const char *width, const char *height,
                                       const char *max_cols, const char *max_rows);

/* PoP: w7_terminalima_83613a_kitty_placeholder_rows @ agent/terminal_image.py:kitty_placeholder_rows
 * Placeholder text grid. Caller frees. */
json_t *w7_terminalima_83613a_kitty_placeholder_rows(const char *cols, const char *rows);

/* PoP: w7_terminalima_83613a_detect_graphics @ agent/terminal_image.py:detect_terminal_graphics
 * Seam: env values are passed explicitly instead of read from the process env.
 * Returns a pointer to a static protocol string. */
const char *w7_terminalima_83613a_detect_graphics(const char *term, const char *term_program,
                                                  const char *kitty_window_id,
                                                  const char *iterm_session_id,
                                                  const char *wezterm_pane);

/* PoP: w7_terminalima_83613a_supports_kitty_placeholders @ agent/terminal_image.py:supports_kitty_placeholders */
bool w7_terminalima_83613a_supports_kitty_placeholders(const char *term, const char *term_program,
                                                       const char *kitty_window_id,
                                                       const char *iterm_session_id,
                                                       const char *wezterm_pane);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W7_TERMINALIMA_83613A_H */
