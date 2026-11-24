/*
 * noc_w27_adapter_74f618_p1.h - C surface for upstream
 * plugins/platforms/slack/adapter.py (gate w27_adapter_74f618_p1).
 *
 * Gate 1 of 11: the module-level value helpers — link-preview config
 * coercion, GFM table detection/alignment/fencing, permalink tails and the
 * small str/int/truthy readers the whole adapter is built from.
 *
 * Every exported function takes the harness request object and returns the
 * JSON "out" value. Seams (Slack Web API, sockets, files, env) stay outside.
 */
#ifndef NOC_W27_ADAPTER_74F618_P1_H
#define NOC_W27_ADAPTER_74F618_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* req: {"extra": obj|null} -> {unfurl_links?: bool, unfurl_media?: bool} */
json_t *w27_p1_unfurl_kwargs(const json_t *req);

/* req: {"line": str} -> bool (_is_table_row) */
json_t *w27_p1_is_table_row(const json_t *req);

/* req: {"s": str} -> int display width (East-Asian W/F = 2) */
json_t *w27_p1_disp_width(const json_t *req);

/* req: {"cell": str, "width": int} -> str (_pad) */
json_t *w27_p1_pad(const json_t *req);

/* req: {"line": str} -> [str] (_split_table_row) */
json_t *w27_p1_split_table_row(const json_t *req);

/* req: {"rows": [str]} -> [str] (_align_table) */
json_t *w27_p1_align_table(const json_t *req);

/* req: {"text": str} -> str (_wrap_markdown_tables) */
json_t *w27_p1_wrap_markdown_tables(const json_t *req);

/* req: {"channel_id": str|null, "message_ts": str|null} -> str */
json_t *w27_p1_permalink_path(const json_t *req);

/* req: {"value": any} -> str (_str_or_empty) */
json_t *w27_p1_str_or_empty(const json_t *req);

/* req: {"value": str} -> int (_int_or_zero) */
json_t *w27_p1_int_or_zero(const json_t *req);

/* req: {"mapping": obj, "keys": [str]} -> any|null (_first_truthy) */
json_t *w27_p1_first_truthy(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W27_ADAPTER_74F618_P1_H */
