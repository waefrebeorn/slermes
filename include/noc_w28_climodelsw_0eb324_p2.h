/*
 * noc_w28_climodelsw_0eb324_p2.h - C surface for upstream
 * hermes_cli/cli_model_switch_mixin.py (gate w28_climodelsw_0eb324_p2).
 *
 * The /model picker's pure decision core: the reasoning-effort rows, whether
 * a picked model offers an effort step, the fuzzy row filter, the viewport
 * scroll arithmetic, the initial modal state and the three-stage selection
 * state machine (provider -> model -> reasoning).
 *
 * Seams: the effort ladder + i18n catalog, the provider inventory / live
 * catalog probe, the switch result itself, and every UI side effect
 * (invalidate, close, commit) are passed in or reported back as data.
 */
#ifndef NOC_W28_CLIMODELSW_0EB324_P2_H
#define NOC_W28_CLIMODELSW_0EB324_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Viewport defaults of _compute_model_picker_viewport. */
#define W28_CLIMODELSW_0EB324_P2_VIEWPORT_RESERVED 6
#define W28_CLIMODELSW_0EB324_P2_VIEWPORT_CHROME 6
#define W28_CLIMODELSW_0EB324_P2_VIEWPORT_MIN_VISIBLE 3

/* {"efforts": [...], "label_none": str, "label_keep": str} ->
 * [[value, label], ...] ladder + off + keep-current. */
json_t *w28_climodelsw_0eb324_p2_reasoning_rows(const json_t *req);

/* {"provider_data": {...}, "model": str} -> bool (false only on an explicit
 * capability-map reasoning:false). */
json_t *w28_climodelsw_0eb324_p2_offers_reasoning(const json_t *req);

/* {"entries": [...], "query": str} -> [[original_index, entry], ...] */
json_t *w28_climodelsw_0eb324_p2_filter_entries(const json_t *req);

/* {"selected","scroll_offset","n","term_rows","reserved_below","panel_chrome",
 *  "min_visible"} -> [scroll_offset, visible] */
json_t *w28_climodelsw_0eb324_p2_viewport(const json_t *req);

/* {"providers","current_model","current_provider","user_provs","custom_provs"}
 * -> the picker's initial modal state. */
json_t *w28_climodelsw_0eb324_p2_initial_state(const json_t *req);

/* {"state": <picker state>|null, "persist_global": bool,
 *  "offered": [...], "switch": {...}, "rows": [[v,label], ...]} ->
 * {"action": "none"|"close"|"stage_model"|"back_to_provider"|
 *            "stage_reasoning"|"back_to_model"|"commit",
 *  "state": <new state>|null, "commit": {"result","persist_global",
 *  "reasoning_effort"}|null} */
json_t *w28_climodelsw_0eb324_p2_selection_step(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
