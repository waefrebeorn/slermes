/*
 * noc_w28_modelswitc_6dc907_p1.h - C surface for upstream
 * hermes_cli/model_switch_providers.py (gate w28_modelswitc_6dc907_p1).
 *
 * Part 1 of 3: the value-level credential and catalog predicates plus the
 * string/collection coercions every picker row is built from.  The imports
 * those functions make (auth store, credential pool, models.dev, anon_auth)
 * are seams: each one is replaced here by the exact values it would return.
 */
#ifndef NOC_W28_MODELSWITC_6DC907_P1_H
#define NOC_W28_MODELSWITC_6DC907_P1_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _discovered_catalog_stale(entry, model_ids) -> bool */
json_t *w28ms_p1_catalog_stale(const json_t *entry, const json_t *model_ids);

/* _any_env(env_vars, read_env) -> bool; env is the mapping read_env consults. */
json_t *w28ms_p1_any_env(const json_t *env_vars, const json_t *env);

/* _skip(seen, excluded, *keys) -> bool */
json_t *w28ms_p1_skip(const json_t *seen, const json_t *excluded, const json_t *keys);

/* _azure_entra_configured(read_env) -> bool; model_cfg is _get_model_config_dict(). */
json_t *w28ms_p1_azure_entra_configured(const json_t *model_cfg, const json_t *env);

/* _has_fast_aws_sdk_signal() -> bool over an environment mapping. */
json_t *w28ms_p1_fast_aws_signal(const json_t *env);

/* _has_aws_sdk_creds_for_listing(slug, current_provider) -> bool; sdk_creds is
 * the botocore `has_aws_credentials()` answer (the seam). */
json_t *w28ms_p1_aws_creds_for_listing(const json_t *slug, const json_t *current_provider,
                                       const json_t *env, const json_t *sdk_creds);

/* _is_aws_sdk(pconfig) -> bool; pconfig carries `auth_type`. */
json_t *w28ms_p1_is_aws_sdk(const json_t *pconfig);

/* _first_curated(curated, keys) -> list */
json_t *w28ms_p1_first_curated(const json_t *curated, const json_t *keys);

/* _free_tier_nous_row(row) -> dict | null; flags carries guest, guest_allowed,
 * free_inference, label and guest_model (the anon_auth seam). */
json_t *w28ms_p1_free_tier_nous_row(const json_t *row, const json_t *flags);

/* _cap_models(model_ids, max_models, slug) -> list; max_models null means None. */
json_t *w28ms_p1_cap_models(const json_t *model_ids, const json_t *max_models, const json_t *slug);

/* _split_chain_entries(models) -> the same list, mutated. */
json_t *w28ms_p1_split_chain_entries(const json_t *models);

/* _extend_unique(target, items) -> the target list, mutated. */
json_t *w28ms_p1_extend_unique(const json_t *target, const json_t *items);

/* _norm_url(url) -> str */
json_t *w28ms_p1_norm_url(const json_t *url);

/* _entry_base_url(entry, keys) -> the raw first-truthy value or "" */
json_t *w28ms_p1_entry_base_url(const json_t *entry, const json_t *keys);

/* _entry_api_mode(entry) -> str | null */
json_t *w28ms_p1_entry_api_mode(const json_t *entry);

/* _entry_credentials(entry, *key_env_keys) ->
 * {"key_env": str, "identity": any, "source": bool}; key_cmd_identity is what
 * build_command_token_provider() would return (null = it returned None). */
json_t *w28ms_p1_entry_credentials(const json_t *entry, const json_t *key_env_keys,
                                   const json_t *key_cmd_identity);

/* _discover_flag(entry) -> the declared value (default True) */
json_t *w28ms_p1_discover_flag(const json_t *entry);

/* _display_prefix(name) -> str */
json_t *w28ms_p1_display_prefix(const json_t *name);

/* _group_display_name(display_name) -> str */
json_t *w28ms_p1_group_display_name(const json_t *name);

#ifdef __cplusplus
}
#endif
#endif
