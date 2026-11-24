/*
 * noc_w28_climodelsw_0eb324_p1.h - C surface for upstream
 * hermes_cli/cli_model_switch_mixin.py (gate w28_climodelsw_0eb324_p1).
 *
 * Route/value helpers of the /model switch mixin: the model-runtime field
 * set, its one-turn snapshot, the bare-``custom`` provider heal, the
 * config.yaml default route and the resume route a stored session row
 * resolves to, plus the ``/model`` usage block the picker prints.
 *
 * Every store is a seam and arrives as an answer in the request object
 * (config model section, custom-provider identity lookup, gateway runtime
 * row reader, provider-profile wire derivation, i18n catalog): what is
 * ported here is the decision each answer feeds.
 */
#ifndef NOC_W28_CLIMODELSW_0EB324_P1_H
#define NOC_W28_CLIMODELSW_0EB324_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _RUNTIME_FIELDS: CLI-level fields describing the active model route. */
#define W28_CLIMODELSW_0EB324_P1_RUNTIME_FIELDS 9

/* _MODEL_USAGE_COL: description column of the /model usage block. */
#define W28_CLIMODELSW_0EB324_P1_USAGE_COL 36

/* {"provider","base_url","model","main_slot_serves","canonical","empty_identity"}
 * -> the routable provider identity, or null when the row names none. */
json_t *w28_climodelsw_0eb324_p1_heal_provider(const json_t *req);

/* {"state": {...}} -> the 9-field runtime projection (missing key -> null). */
json_t *w28_climodelsw_0eb324_p1_runtime_fields(const json_t *req);

/* {"state": {...}, "agent": {...}|null} -> runtime fields + reasoning_config
 * (deep-copied) + agent_primary_runtime. */
json_t *w28_climodelsw_0eb324_p1_snapshot_runtime(const json_t *req);

/* {"model_cfg": <config.yaml "model" value>} -> {"model","provider"} */
json_t *w28_climodelsw_0eb324_p1_configured_route(const json_t *req);

/* {"resume":{"model","provider","base_url","api_mode"},"current_model",
 *  "current_provider","main_slot_serves","canonical","empty_identity",
 *  "foreign","derived","normalized_base_url"}
 * -> [model, provider, base_url, api_mode, provider_changed] or null. */
json_t *w28_climodelsw_0eb324_p1_stored_session_route(const json_t *req);

/* {"descriptions": {"cli.model.usage_*": text}} ->
 * [{"form","desc","line"}, ...] for the /model usage block. */
json_t *w28_climodelsw_0eb324_p1_usage_rows(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
