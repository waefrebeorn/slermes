/*
 * noc_w32_tools_f39c80_p1.h — C surface for upstream
 * hermes_cli/web_routers/tools.py (gate w32_tools_f39c80_p1).
 *
 * The terminal-backend decision half of the tools dashboard router, plus the
 * toolset model-catalog surface: the config-or-env read that every probe is
 * built on, the four backend probes as pure (status, detail) decisions over
 * their seam results, the GET /api/tools/terminal/backends row build with its
 * active-backend normalisation, the PUT validation, and the model-catalog
 * side (plugin resolution, current-selection resolution, the models payload,
 * the model-write validation, provider-row resolution and the known-toolset
 * gate).
 *
 * SEAMS (never modelled here): env reads (get_env_value), config.yaml
 * load/save, the profile scope + config-write scope, find_docker()/run_capture
 * subprocess IO, the agent terminal registry probe, the plugin backend rows,
 * hermes_cli.tools_config's provider catalogs and readiness/entitlement
 * readers, and IMAGEGEN_BACKENDS / _plugin_*_catalog.  The oracle patches
 * those and compares what the values decide once they have answered.
 */
#ifndef NOC_W32_TOOLS_F39C80_P1_H
#define NOC_W32_TOOLS_F39C80_P1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _terminal_cfg_value(): config[key] when present, non-None and non-blank
 * (str().strip()), else env_map[env_var].strip() ("" when absent/None). */
json_t *w32_tools_f39c80_p1_cfg_value(const json_t *terminal_cfg, const json_t *cfg_key,
                                      const json_t *env_var, const json_t *env_map);

/* _probe_ssh_backend() -> {"status", "detail"}; both keys missing ->
 * needs_setup naming "terminal.ssh_host" / "terminal.ssh_user" in that order. */
json_t *w32_tools_f39c80_p1_probe_ssh(const json_t *terminal_cfg, const json_t *env_map);

/* _probe_modal_backend(): direct credentials (a raised probe is skipped) or
 * both MODAL_TOKEN_* env vars -> ready, else the setup hint. */
json_t *w32_tools_f39c80_p1_probe_modal(const json_t *has_direct, const json_t *direct_raises,
                                        const json_t *env_map);

/* _probe_daytona_backend(): DAYTONA_API_KEY -> ready, else the setup hint. */
json_t *w32_tools_f39c80_p1_probe_daytona(const json_t *env_map);

/* _probe_docker_backend() from its seam results:
 * {"found", "runtime", "hint", "outcome": "ok"|"timeout"|"error",
 *  "returncode", "error"} -> {"status", "detail"}. */
json_t *w32_tools_f39c80_p1_probe_docker(const json_t *state);

/* GET /api/tools/terminal/backends: {"active": <str>, "backends": [{name,
 * label, description, active, status, detail}]}.  ``probes`` maps a backend
 * name to {"status", "detail"} (the seam's _probe_terminal_backend result). */
json_t *w32_tools_f39c80_p1_backends_payload(const json_t *rows, const json_t *config,
                                             const json_t *probes);

/* PUT /api/tools/terminal/backend validation: ok -> {"ok": true, "backend":
 * <normalised>}; unknown -> {"ok": false, "status": 400, "detail": <msg>}
 * (the detail reprs the *raw* submitted value). */
json_t *w32_tools_f39c80_p1_select_backend(const json_t *rows, const json_t *backend);

/* _no_models(): {name, has_models: false, models: [], current, default}. */
json_t *w32_tools_f39c80_p1_no_models(const json_t *name);

/* _resolve_toolset_model_plugin(): image_gen -> image_gen_plugin_name or
 * imagegen_backend; video_gen -> video_gen_plugin_name; else null. */
json_t *w32_tools_f39c80_p1_model_plugin(const json_t *ts_key, const json_t *row);

/* _MODEL_CATALOG_TOOLSETS lookup: the config section, or null. */
json_t *w32_tools_f39c80_p1_catalog_section(const json_t *name);

/* GET /api/tools/toolsets/{name}/models payload (the row/plugin/catalog
 * resolution is the seam; ``row`` may be null -> _no_models). */
json_t *w32_tools_f39c80_p1_models_payload(const json_t *name, const json_t *config,
                                           const json_t *row, const json_t *catalog,
                                           const json_t *default_model);

/* PUT /api/tools/toolsets/{name}/model validation -> ok payload or the 400
 * envelope {"ok": false, "status": 400, "detail": <msg>}. */
json_t *w32_tools_f39c80_p1_model_select(const json_t *name, const json_t *model,
                                         const json_t *config, const json_t *row,
                                         const json_t *catalog);

/* _find_toolset_provider_row(): by name, else the first row whose
 * active_flags[name] is truthy; null when nothing matches. */
json_t *w32_tools_f39c80_p1_provider_row(const json_t *rows, const json_t *provider,
                                         const json_t *active_flags);

/* _require_known_toolset(): {"ok": true} or the 400 "Unknown toolset: <name>"
 * envelope. */
json_t *w32_tools_f39c80_p1_require_known(const json_t *name, const json_t *known_names);

/* The fixed maps this half of the router is tuned with. */
json_t *w32_tools_f39c80_p1_constants(void);

#ifdef __cplusplus
}
#endif
#endif
