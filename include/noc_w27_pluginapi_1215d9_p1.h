/*
 * noc_w27_pluginapi_1215d9_p1.h - C11 surface for the request/response
 * plumbing helpers of upstream plugins/kanban/dashboard/plugin_api.py
 * (gate w27_pluginapi_1215d9_p1, source lines 1-410).
 *
 * Everything here is shape: slug normalisation, card/attachment payload
 * dictionaries, diagnostic badge rollups, the 404/409/500 error envelopes and
 * the linked-task row builder.  The DB connection, the HTTP router and the
 * audit log stay in the caller; these functions own only what the values
 * decide.  Each entry point takes the decoded request object and returns the
 * common result envelope:
 *
 *   {"ok": true,  "value": <value>}
 *   {"ok": false, "kind": "http", "status": <int>, "detail": <str>}
 *   {"ok": false, "kind": "exc",  "exc": <str>,     "detail": <str>}
 */
#ifndef NOC_W27_PLUGINAPI_1215D9_P1_H
#define NOC_W27_PLUGINAPI_1215D9_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Board columns, left to right, from the one workflow definition. */
#define W27_P1_BOARD_COLUMN_COUNT 8
/* Card preview budget for the board payload (full text comes from /tasks/:id). */
#define W27_P1_CARD_SUMMARY_PREVIEW_CHARS 200

/* req {slug} -> lowercased/validated slug, null when empty, 400 when malformed. */
json_t *w27p1_normalize_slug(const json_t *req);

/* req {board, exists, archived} -> validated board slug or null; 400/404. */
json_t *w27p1_resolve_board(const json_t *req);

/* req {slug, exists} -> normalised slug, 404 when the board does not exist. */
json_t *w27p1_existing_board_slug(const json_t *req);

/* req {ids:[...]} -> "?,?,?" for an IN (...) list. */
json_t *w27p1_placeholders(const json_t *req);

/* req {attachment:{...}} -> the 8-key attachment payload. */
json_t *w27p1_attachment_dict(const json_t *req);

/* req {task, latest_summary, current_run_started_at, now} -> full card dict. */
json_t *w27p1_task_dict(const json_t *req);

/* req {diagnostics:[...]} -> compact card badge summary, or null when empty. */
json_t *w27p1_warnings_summary(const json_t *req);

/* req {task, diagnostics} -> task dict with diagnostics/warnings attached. */
json_t *w27p1_attach_diagnostics(const json_t *req);

/* req {found, obj, ident, label} -> obj, or 404 "<label> <ident> not found". */
json_t *w27p1_require_obj(const json_t *req);

/* req {ok} -> true, or 404 "task not found" when the mutator lost the task. */
json_t *w27p1_require_ok(const json_t *req);

/* req {detail} -> 409 with that detail. */
json_t *w27p1_conflict(const json_t *req);

/* req {status, types, exc, message, http_status, http_detail} -> mapped error. */
json_t *w27p1_map_errors(const json_t *req);

/* req {prefix, exc, message, http_status, http_detail} -> 500 "<prefix>: <msg>". */
json_t *w27p1_errors_to_500(const json_t *req);

/* req {links:{parents,children}, tasks:{id:{title,status}}} -> [{id,title,status}]. */
json_t *w27p1_link_tasks(const json_t *req);

/* req {} -> the module's shape constants (columns, preview budget, poll tick). */
json_t *w27p1_module_constants(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
