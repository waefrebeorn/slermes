/*
 * noc_w26_timeline_c7132f.h - C surface for upstream
 * persistence/sqlite/conversation/_store/timeline.py (gate w26_timeline_c7132f).
 *
 * The session timeline reads one snapshot of the display set.  The connection,
 * the compaction/rewind generation and the row objects are seams: the index
 * probe and a row's stored payload arrive as values.  Ported here is the
 * display-set SQL and the prompt-preview text rule.
 */
#ifndef NOC_W26_TIMELINE_C7132F_H
#define NOC_W26_TIMELINE_C7132F_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _display_rows_sql(conn, session_id, users_only=...) -> the CTE prefix every
 * timeline query is appended to.  `indexed` is the live index probe's answer:
 * true when no row of the session has a NULL display_order/display_identity. */
json_t *w26_timeline_c7132f_display_rows_sql(int indexed, int users_only);

/* _prompt_preview(db, content, display_kind, summary) -> the one-line preview,
 * "" when the row is not a human prompt, at most 120 characters. */
json_t *w26_timeline_c7132f_prompt_preview(const json_t *content,
                                           const json_t *display_kind,
                                           const json_t *summary);

#ifdef __cplusplus
}
#endif
#endif
