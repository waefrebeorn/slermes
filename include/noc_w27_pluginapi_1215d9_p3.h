/*
 * noc_w27_pluginapi_1215d9_p3.h - C11 surface for the estimate / config /
 * home-channel / diagnostics projections of upstream
 * plugins/kanban/dashboard/plugin_api.py
 * (gate w27_pluginapi_1215d9_p3, source lines 1064-1300).
 *
 * Each ported function is the part of a handler that runs after (or instead
 * of) its I/O: the model reply is already in hand, the config dict is already
 * loaded, the gateway platforms are already enumerated, the diagnostics are
 * already computed.  Config files, the auxiliary LLM client, the gateway
 * config loader and the SQLite cursors stay seams.  Every entry point takes a
 * decoded request object and returns the common envelope (see
 * noc_w27_pluginapi_1215d9_p1.h).
 */
#ifndef NOC_W27_PLUGINAPI_1215D9_P3_H
#define NOC_W27_PLUGINAPI_1215D9_P3_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Severity ladder used by diagnostics filtering/sorting. */
#define W27_P3_SEVERITY_COUNT 3

/* req {s, n} -> s stripped and capped at n characters plus an ellipsis. */
json_t *w27p3_cap(const json_t *req);

/* req {title, body, raw, model, llm_error, aux} -> the _run_estimate payload. */
json_t *w27p3_estimate(const json_t *req);

/* req {config} -> the dashboard.kanban preferences the UI reads at boot. */
json_t *w27p3_config_shape(const json_t *req);

/* req {platforms} -> configured home channels, sorted by platform. */
json_t *w27p3_home_channels(const json_t *req);

/* req {homes, platform, detail} -> that platform's home, or 404 <detail>. */
json_t *w27p3_home_for_platform(const json_t *req);

/* req {homes, subs, task_id} -> home channels annotated with "subscribed". */
json_t *w27p3_home_channels_subscribed(const json_t *req);

/* req {profile, raise} -> the active profile name, defaulting to "default". */
json_t *w27p3_active_profile(const json_t *req);

/* req {payload, raise} -> the provider/model override dropdown catalog. */
json_t *w27p3_model_options(const json_t *req);

/* req {diags_by_task, rows, severity} -> {diagnostics:[...], count}. */
json_t *w27p3_diagnostics_list(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
