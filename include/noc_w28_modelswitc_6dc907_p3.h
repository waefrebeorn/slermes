/*
 * noc_w28_modelswitc_6dc907_p3.h - C surface for upstream
 * hermes_cli/model_switch_providers.py (gate w28_modelswitc_6dc907_p3).
 *
 * Part 3 of 3: the post-passes over built rows -- enabled filtering, current
 * model injection, the (not current, -count) sort, MoA/Local row injection,
 * the picker's own model/row filter, and the usage-window notes.  Config load,
 * inventory, openrouter fetch, plan-limit pricing and account discovery are
 * seams; the values they would return are inputs.
 */
#ifndef NOC_W28_MODELSWITC_6DC907_P3_H
#define NOC_W28_MODELSWITC_6DC907_P3_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _finalize_picker_rows(results, user_providers, current_model)
 * req {results, user_providers, current_model, requires_discovery: [[slug, model], ...]}
 * -> list of rows */
json_t *w28ms_p3_finalize_rows(const json_t *req);

/* _prepend_moa_picker_provider(providers, current_provider)
 * req {providers, moa_row} -> list */
json_t *w28ms_p3_prepend_moa(const json_t *req);

/* list_picker_providers() row filter
 * req {providers, openrouter_live_ids, openrouter_error, max_models} -> list */
json_t *w28ms_p3_picker_filter(const json_t *req);

/* _with_local_runtime_row() sort + cap
 * req {rows, local_row, max_models} -> list */
json_t *w28ms_p3_local_runtime_rows(const json_t *req);

/* _attach_window_model_notes(providers)
 * req {providers, usage: {levels: {model: level}, tags: {level: label},
 *      fresh: bool, limit_line: str} | null} -> list */
json_t *w28ms_p3_attach_window_notes(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
