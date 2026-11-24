/*
 * noc_w32_tools_f39c80_p2.h — C surface for upstream
 * hermes_cli/web_routers/tools.py (gate w32_tools_f39c80_p2).
 *
 * The toolsets-matrix / config-panel / write half of the tools dashboard
 * router: the GET /api/tools/toolsets row build (config-only switches read
 * their own section, platform toolsets read the platform set, tools are
 * de-duplicated + sorted), the section normaliser, the enable/disable plan
 * (config-only section write vs platform-set add/discard) with its
 * install-on-enable post-setup pick, the provider matrix payload, the .env
 * write plan (allowed keys, unknown-key 400, saved/skipped partition) and the
 * per-capability provider selection validation with the managed-Nous pin.
 *
 * SEAMS (never modelled here): config.yaml load/save, the profile scope /
 * config-write scope / CONFIG_MUTATION_LOCK, .env get/save, the provider
 * catalogs and their readiness/entitlement readers, apply_provider_selection,
 * spawn_profile_action/_spawn_hermes_action, and the web tool dispatcher's
 * active-backend resolution.  The oracle patches those in and compares only
 * what the values decide.
 *
 * A legacy (``capability`` omitted) provider selection is delegated whole to
 * apply_provider_selection, so only the capability path is modelled here.
 */
#ifndef NOC_W32_TOOLS_F39C80_P2_H
#define NOC_W32_TOOLS_F39C80_P2_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _dict_section(): the resulting config, with config[key] a dict (created or
 * replaced when absent / not a dict). */
json_t *w32_tools_f39c80_p2_dict_section(const json_t *config, const json_t *key);

/* GET /api/tools/toolsets row build.  Seams supplied as maps:
 *   rows            [{"name","label","description"}]
 *   platform_map    name -> target platform
 *   platform_tools  platform -> [toolset names enabled there]
 *   configured      name -> bool (_toolset_has_keys)
 *   gui_labels      raw label -> GUI label (gui_toolset_label)
 *   platform_labels platform -> raw platform label (platform_label)
 *   config          the loaded config (config-only sections)
 *   config_only     [names] (_CONFIG_ONLY_TOOLSETS)
 *   tools_raw       name -> [tool names] (resolve_toolset)
 *   tools_error     [names] whose resolve_toolset raises -> tools: []
 */
json_t *w32_tools_f39c80_p2_toolsets_rows(const json_t *rows, const json_t *platform_map,
                                          const json_t *platform_tools, const json_t *configured,
                                          const json_t *gui_labels, const json_t *platform_labels,
                                          const json_t *config, const json_t *config_only,
                                          const json_t *tools_raw, const json_t *tools_error);

/* The union of every visible provider's declared env_var keys, sorted. */
json_t *w32_tools_f39c80_p2_allowed_env_keys(const json_t *providers);

/* PUT /api/tools/toolsets/{name}/env plan -> {"ok","name","saved","skipped",
 * "is_set"} or the 400 envelope.  ``save_errors`` maps a key to the ValueError
 * text save_env_value raises for it. */
json_t *w32_tools_f39c80_p2_save_env(const json_t *name, const json_t *providers,
                                     const json_t *env_body, const json_t *env_set,
                                     const json_t *save_errors, const json_t *known);

/* PUT /api/tools/toolsets/{name}: req =
 * {name, enabled, known, config, config_only, platform_map, platform_tools,
 *  providers (the category's picker rows), has_category, installed}
 * -> {"response": <route payload>, "saved": <what was written>} or the 400
 * envelope.  saved = {"section","enabled"} for a config-only toolset, else
 * {"platform","tools":[sorted]} for the platform-set path. */
json_t *w32_tools_f39c80_p2_toggle(const json_t *req);

/* GET /api/tools/toolsets/{name}/config payload.  req =
 * {name, known, config, has_category, providers, active_flags, statuses,
 *  caps, env_set, web_state}.  ``web_state`` is the web dispatcher seam:
 * {search_backend, extract_backend, managed_search, is_managed_search,
 *  is_managed_extract}. */
json_t *w32_tools_f39c80_p2_toolset_config(const json_t *req);

/* PUT /api/tools/toolsets/{name}/provider, capability path.  req =
 * {name, capability, provider, known, config, providers, caps, features}
 * -> {"response": <route payload>,
 *     "written": {"section":"web","key":"<cap>_backend","value":<pin>}}
 * or the 400 envelope.  ``features`` = {nous_auth_present, account} where
 * account = null or {logged_in, tool_gateway_entitled, entitled_for{cat:bool}};
 * MANAGED_FEATURE_COVERAGE_CATEGORY is the fixed map. */
json_t *w32_tools_f39c80_p2_provider_capability(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
