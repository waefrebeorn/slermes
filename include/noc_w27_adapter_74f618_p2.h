/*
 * noc_w27_adapter_74f618_p2.h - C surface for upstream
 * plugins/platforms/slack/adapter.py (gate w27_adapter_74f618_p2).
 *
 * Gate 2: Block Kit reading — the non-quoted mention walker, the inline
 * element renderer (text/color/entities/date/link/message_mention), the
 * rich_text section/quote/list/preformatted walker, and pasted-table
 * projection. Rendering only: no API, no state.
 */
#ifndef NOC_W27_ADAPTER_74F618_P2_H
#define NOC_W27_ADAPTER_74F618_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* req: {"blocks": [..]|null} -> ["<@U1>", ...] (non-quoted user elements only) */
json_t *w27_p2_collect_block_mentions(const json_t *req);

/* req: {"event": obj} -> str (flat text + block-only mentions) */
json_t *w27_p2_mention_detection_text(const json_t *req);

/* req: {"el": obj, "name": str} -> str (_slack_str_field) */
json_t *w27_p2_slack_str_field(const json_t *req);

/* req: {"el": obj} -> str (one inline element) */
json_t *w27_p2_render_inline_element(const json_t *req);

/* req: {"elements": [obj]} -> str (concatenated) */
json_t *w27_p2_render_inline_elements(const json_t *req);

/* req: {"blocks": [..]|null} -> str (rich_text + table projection) */
json_t *w27_p2_extract_text_blocks(const json_t *req);

/* req: {"value": any} -> str (text leaves of a table cell subtree) */
json_t *w27_p2_table_cell_text(const json_t *req);

/* req: {"block": obj, "max_chars": int?} -> str (pipe rows, truncated) */
json_t *w27_p2_render_table_block(const json_t *req);

/* req: {"response": obj|null, "code": str} -> bool (_slack_error_is) */
json_t *w27_p2_slack_error_is(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W27_ADAPTER_74F618_P2_H */
