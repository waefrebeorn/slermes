/*
 * noc_w27_pluginapi_1215d9_p2.h - C11 surface for the status-transition verbs
 * of upstream plugins/kanban/dashboard/plugin_api.py
 * (gate w27_pluginapi_1215d9_p2, source lines 531-960).
 *
 * The dashboard's PATCH /tasks/{id} and POST /tasks/bulk both dispatch a
 * status verb plus a set of override knobs.  What is ported here is the
 * decision half only: which verb a status names, which verb a drag lands on,
 * which refusals are user-facing, and the exact text of the 404/409 the UI
 * renders.  The writes themselves (archive_task, complete_task, edit_task ...)
 * remain seams.  Every entry point takes a decoded request object and returns
 * the common envelope (see noc_w27_pluginapi_1215d9_p1.h).
 */
#ifndef NOC_W27_PLUGINAPI_1215D9_P2_H
#define NOC_W27_PLUGINAPI_1215D9_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* The seven statuses _STATUS_HANDLERS dispatches, in declaration order. */
#define W27_P2_STATUS_HANDLER_COUNT 7

/* req {status, unknown_detail} -> true when dispatchable, else _StatusRejected. */
json_t *w27p2_status_refusal(const json_t *req);

/* req {} -> the dispatchable status list, in table order. */
json_t *w27p2_status_table(const json_t *req);

/* req {status} -> "archive" | "verb": which half of the PATCH status phase runs. */
json_t *w27p2_patch_status_route(const json_t *req);

/* req {task_id, status, blockers:[[pid,status],...]} -> refusal text, or null. */
json_t *w27p2_open_parent_refusal(const json_t *req);

/* req {clear, model_override, provider_override} -> {model, provider}. */
json_t *w27p2_model_override_value(const json_t *req);

/* req {clear, reasoning_effort} -> the effort value to persist (null = clear). */
json_t *w27p2_reasoning_effort_value(const json_t *req);

/* req {clear, value} -> whether this override knob is part of the patch. */
json_t *w27p2_override_wanted(const json_t *req);

/* req {current_status, target} -> "unblock" | "reopen_review" | "direct". */
json_t *w27p2_drag_route(const json_t *req);

/* req {ids} -> truthy ids only; 400 "ids is required" when none survive. */
json_t *w27p2_bulk_ids(const json_t *req);

/* req {body} -> true, or 400 "body is required" for a blank comment. */
json_t *w27p2_comment_body_gate(const json_t *req);

/* req {status, assignee} -> is the review assignee deferred to after review? */
json_t *w27p2_review_assignee_deferred(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
