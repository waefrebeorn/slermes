/*
 * noc_w28_modelswitc_6dc907_p2.h - C surface for upstream
 * hermes_cli/model_switch_providers.py (gate w28_modelswitc_6dc907_p2).
 *
 * Part 2 of 3: the _PickerBuild decision core -- which row is current, whether
 * a custom endpoint may be probed, how a fetched catalog is judged, and the row
 * shapes the builders append.  Every probe/registry/anon_auth call is a seam
 * expressed as an input flag.
 */
#ifndef NOC_W28_MODELSWITC_6DC907_P2_H
#define NOC_W28_MODELSWITC_6DC907_P2_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _discover_endpoint_models() decision:
 * opts {native, has_explicit_models, probe_live, discovery_allowed}
 * -> {"models": list|null, "native_catalog_empty": bool} */
json_t *w28ms_p2_discover_models(const json_t *live_models, const json_t *opts);

/* _PickerBuild.resolved_fast_custom_probe */
json_t *w28ms_p2_resolved_fast_probe(const json_t *for_picker, const json_t *fast_custom_probe);

/* _PickerBuild.can_probe_custom(row_is_current=...) */
json_t *w28ms_p2_can_probe_custom(const json_t *probe_custom_providers,
                                  const json_t *probe_current_custom_provider,
                                  const json_t *row_is_current);

/* _PickerBuild.endpoint_is_current(slug, aliases, url_norm, url_match_ok=...)
 * cfg {current_provider, current_base_url, url_match_ok} */
json_t *w28ms_p2_endpoint_is_current(const json_t *slug, const json_t *aliases,
                                     const json_t *url_norm, const json_t *cfg);

/* _PickerBuild.record_section3_pair(name, url_norm)
 * -> {"added": bool, "pair": [name, url] | null} */
json_t *w28ms_p2_record_section3_pair(const json_t *name, const json_t *url_norm);

/* _PickerBuild.add_builtin_row(slug, name, is_current, model_ids, source, uncapped_ok=)
 * req {slug, name, is_current, model_ids, source, uncapped_ok, max_models, flags}
 * -> {"row": dict|null, "seen": [slug.lower()]} */
json_t *w28ms_p2_add_builtin_row(const json_t *req);

/* _PickerBuild.add_endpoint_row(...)
 * req {slug, name, api_url, models, is_current, native_catalog_empty, source, shown}
 * -> {"row": dict, "seen": [slug.lower()]} */
json_t *w28ms_p2_add_endpoint_row(const json_t *req);

/* _PickerBuild.discover_endpoint() probe policy
 * req {discovery_allowed, api_key, has_explicit_models, probe_custom_providers,
 *      probe_current_custom_provider, is_current} -> {"probe_live": bool} */
json_t *w28ms_p2_discover_endpoint(const json_t *req);

/* _lap_lmstudio_row() section-0 guard + id merge
 * req {excluded, seen, configured, curated, current_provider, declared_models,
 *      label, max_models} -> {"row": dict|null} */
json_t *w28ms_p2_lmstudio_row(const json_t *req);

#ifdef __cplusplus
}
#endif
#endif
