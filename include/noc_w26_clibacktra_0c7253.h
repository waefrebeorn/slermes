/*
 * noc_w26_clibacktra_0c7253.h - C surface for upstream
 * hermes_cli/cli_backtrack_browse.py (gate w26_clibacktra_0c7253).
 *
 * The transcript pager's pure geometry: the pager is sized to the terminal,
 * a scroll offset is nudged the smallest amount that shows the selected
 * prompt, and the browse footer sheds hints as cells run out.  The terminal
 * itself (prompt_toolkit, shutil) is a seam: the size arrives as numbers and
 * the cell width of a string is its code-point count.
 */
#ifndef NOC_W26_CLIBACKTRA_0C7253_H
#define NOC_W26_CLIBACKTRA_0C7253_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* scroll_into_view(offset, span, visible, total) -> int: the smallest scroll
 * that shows the selected prompt (its first row when taller than the view). */
json_t *w26_clibacktra_0c7253_scroll(int offset, const json_t *span, int visible, int total);

/* pager_geometry(reserved_below, hint_rows) -> [inner_width, visible_rows]
 * for a terminal of `columns` x `lines` cells. */
json_t *w26_clibacktra_0c7253_geometry(int columns, int lines, int reserved_below, int hint_rows);

/* fit_browse_hint(hint, width) -> the footer line for `width` cells, shedding
 * hints the way Codex's prompt-navigation footer does. */
json_t *w26_clibacktra_0c7253_fit_hint(const char *hint, int width);

/* The module's literal style constants, as an object. */
json_t *w26_clibacktra_0c7253_constants(void);

#ifdef __cplusplus
}
#endif
#endif
