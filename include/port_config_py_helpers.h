/*
 * port_config_py_helpers.h — Faithful C11 ports of the remaining module-level
 * pure helpers from Python hermes_cli/config.py (the 40 REAL_GAP set scanned in
 * the parity battleground). Each C function carries the exact
 *   /* PoP: <cname> @ hermes_cli/config.py:<pyname> *\/
 * comment so the scanner credits it to the next matching definition.
 *
 * Split into two translation units:
 *   port_config_py_pure.c   — pure transforms, no filesystem/config-load I/O
 *   port_config_py_io.c     — helpers that read/write config.yaml / .env
 *
 * Reuses existing ports where possible:
 *   config_deep_merge            (port_config_pure.c  = _deep_merge)
 *   config_items_by_unique_name  (port_config_pure.c  = _items_by_unique_name)
 *   config_normalize_max_turns   (port_config_pure.c  = _normalize_max_turns_config)
 *   normalize_custom_provider_entry_json (port_config_helpers.c = _normalize_custom_provider_entry)
 *   coerce_ssl_verify / coerce_config_version / is_env_config_key /
 *   format_config_get_value / validate_config_key (port_config_helpers.c)
 */

#ifndef PORT_CONFIG_PY_HELPERS_H
#define PORT_CONFIG_PY_HELPERS_H

#include <stdbool.h>
#include <stddef.h>

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- reuse declarations from sibling ports ---- */
json_t *config_deep_merge(const json_t *base, const json_t *override);
json_t *config_items_by_unique_name(const json_t *items);
json_t *config_normalize_max_turns(const json_t *config_in);
char *normalize_custom_provider_entry_json(const char *entry_json, const char *provider_key);
int  coerce_ssl_verify(const char *value, int is_bool, int bool_val);
int  coerce_config_version(const char *value, int is_bool);
bool is_env_config_key(const char *key);
bool validate_config_key(const char *key, char *suggestion, size_t sug_size);
void format_config_get_value(const char *value, int as_json, char *out, size_t out_size);

/* ===================================================================
 * port_config_py_pure.c  (no filesystem I/O)
 * =================================================================== */

/* normalize_route_base_url — route-identity normalizer (hermes_cli/route_identity.py).
 * Lowercases scheme + host only; preserves path, trailing slash, query,
 * whitespace and bracketed host syntax verbatim. Returns malloc'd string. */
/* PoP: normalize_route_base_url @ hermes_cli/config.py:normalize_route_base_url */
char *config_normalize_route_base_url(const char *url);

/* _coerce_ssl_verify — wrapper exposing the config.py name. */
/* PoP: _coerce_ssl_verify @ hermes_cli/config.py:_coerce_ssl_verify */
int config_coerce_ssl_verify(const char *value);

/* _coerce_config_version — wrapper exposing the config.py name. */
/* PoP: _coerce_config_version @ hermes_cli/config.py:_coerce_config_version */
int config_coerce_config_version(const char *value);

/* _is_env_config_key — wrapper exposing the config.py name. */
/* PoP: _is_env_config_key @ hermes_cli/config.py:_is_env_config_key */
bool config_is_env_config_key(const char *key);

/* _format_config_get_value — wrapper exposing the config.py name. */
/* PoP: _format_config_get_value @ hermes_cli/config.py:_format_config_get_value */
void config_format_config_get_value(const char *value, int as_json, char *out, size_t out_size);

/* _suggest_closest_key — thin wrapper over validate_config_key. */
/* PoP: _suggest_closest_key @ hermes_cli/config.py:_suggest_closest_key */
bool config_suggest_closest_key(const char *key, char *suggestion, size_t sug_size);

/* _deep_merge — alias for the ported config_deep_merge. */
/* PoP: _deep_merge @ hermes_cli/config.py:_deep_merge */
json_t *config_py_deep_merge(const json_t *base, const json_t *override);

/* _items_by_unique_name — alias for the ported config_items_by_unique_name. */
/* PoP: _items_by_unique_name @ hermes_cli/config.py:_items_by_unique_name */
json_t *config_py_items_by_unique_name(const json_t *items);

/* _normalize_max_turns_config — alias for the ported config_normalize_max_turns. */
/* PoP: _normalize_max_turns_config @ hermes_cli/config.py:_normalize_max_turns_config */
json_t *config_py_normalize_max_turns(const json_t *config_in);

/* _get_nested — return dotted-path value from nested dict/list; NULL if missing. */
/* PoP: _get_nested @ hermes_cli/config.py:_get_nested */
json_t *config_py_get_nested(const json_t *config, const char *dotted_key);

/* _set_nested — set a dotted key path, creating dicts on demand. Returns 0 ok, <0 error. */
/* PoP: _set_nested @ hermes_cli/config.py:_set_nested */
int config_py_set_nested(json_t *config, const char *dotted_key, json_t *value);
/* save_config_value — faithful port of hermes_cli/config.py:save_config_value.
 * Loads merged config as JSON, sets a dotted key, persists atomically. */
int config_py_save_value(const char *dotted_key, json_t *value);
/* Public config.yaml path accessor (mirrors get_config_path()). */
void config_py_get_config_path(char *buf, size_t sz);

/* _unset_nested — remove a dotted key. Returns 1 if removed, 0 if absent. */
/* PoP: _unset_nested @ hermes_cli/config.py:_unset_nested */
int config_py_unset_nested(json_t *config, const char *dotted_key);

/* _explicit_config_paths — collect leaf dotted paths of a raw config. */
/* PoP: _explicit_config_paths @ hermes_cli/config.py:_explicit_config_paths */
char **config_py_explicit_paths(const json_t *config, int *out_count); /* caller frees array+strings */

/* _strip_default_values — drop keys equal to defaults; preserve_keys kept.
 * preserve_keys is an array of dotted paths. Returns a new json_t (caller frees). */
/* PoP: _strip_default_values @ hermes_cli/config.py:_strip_default_values */
json_t *config_py_strip_default_values(const json_t *config, const json_t *defaults,
                                        char **preserve_keys, int preserve_count);

/* _normalize_root_model_keys — migrate stale root-level model keys into model:. */
/* PoP: _normalize_root_model_keys @ hermes_cli/config.py:_normalize_root_model_keys */
json_t *config_py_normalize_root_model_keys(const json_t *config_in);

/* _merge_partial_save — merge override over raw, deep-merging shared dicts. */
/* PoP: _merge_partial_save @ hermes_cli/config.py:_merge_partial_save */
json_t *config_py_merge_partial_save(const json_t *raw, const json_t *override);

/* _preserve_env_ref_templates — restore ${VAR} templates when value unchanged. */
/* PoP: _preserve_env_ref_templates @ hermes_cli/config.py:_preserve_env_ref_templates */
json_t *config_py_preserve_env_ref_templates(const json_t *current, const json_t *raw,
                                              const json_t *loaded_expanded);

/* _env_ref_var_name — normalize a ${...} body to its env var name, or NULL. */
/* PoP: _env_ref_var_name @ hermes_cli/config.py:_env_ref_var_name */
char *config_py_env_ref_var_name(const char *ref);

/* _env_ref_snapshot — map every ${VAR} name in obj to its current env value. */
/* PoP: _env_ref_snapshot @ hermes_cli/config.py:_env_ref_snapshot */
void config_py_env_ref_snapshot(const json_t *obj, json_t *snapshot); /* snapshot is an object */

/* _env_expand_match — expand one ${VAR}/${env:VAR} match body. Returns malloc'd. */
/* PoP: _env_expand_match @ hermes_cli/config.py:_env_expand_match */
char *config_py_env_expand_match(const char *inner); /* inner = text between ${ and } */

/* normalize_extra_headers — coerce a dict to {str:str}, dropping None values. */
/* PoP: normalize_extra_headers @ hermes_cli/config.py:normalize_extra_headers */
json_t *config_py_normalize_extra_headers(const json_t *extra_headers);

/* is_provider_enabled — whether a providers.<name> block is enabled. */
/* PoP: is_provider_enabled @ hermes_cli/config.py:is_provider_enabled */
bool config_py_is_provider_enabled(const json_t *provider_cfg);

/* _custom_provider_entry_to_provider_config — translate legacy entry to v12 shape. */
/* PoP: _custom_provider_entry_to_provider_config @ hermes_cli/config.py:_custom_provider_entry_to_provider_config */
json_t *config_py_custom_provider_entry_to_provider_config(const json_t *entry, const char *provider_key);

/* get_config_value — nested get with default (delegates to _get_nested). */
/* PoP: get_config_value @ hermes_cli/config.py:get_config_value */
json_t *config_py_get_config_value(const json_t *cfg, const char *dotted_key, const json_t *default_val);

/* unset_config_value — unset a dotted key (delegates to _unset_nested). */
/* PoP: unset_config_value @ hermes_cli/config.py:unset_config_value */
int config_py_unset_config_value(json_t *cfg, const char *dotted_key);

/* _resolve_hermes_uid_gid — parse HERMES_UID/HERMES_GID env (best-effort). */
/* PoP: _resolve_hermes_uid_gid @ hermes_cli/config.py:_resolve_hermes_uid_gid */
void config_py_resolve_hermes_uid_gid(long *out_uid, long *out_gid);
void config_py_chown_to_hermes_uid(const char *path);

/* _is_container — detect Docker/Podman/LXC container. */
/* PoP: _is_container @ hermes_cli/config.py:_is_container */
bool config_py_is_container(void);

/* _secure_dir — best-effort chmod 0700 (or HERMES_HOME_MODE) + chown to hermes uid. */
/* PoP: _secure_dir @ hermes_cli/config.py:_secure_dir */
void config_py_secure_dir(const char *path);

/* _secure_file — best-effort chmod 0600. */
/* PoP: _secure_file @ hermes_cli/config.py:_secure_file */
void config_py_secure_file(const char *path);

/* _sanitize_env_lines — sanitize concatenated .env entries into clean lines.
 * Returns malloc'd normalized text. */
/* PoP: _sanitize_env_lines @ hermes_cli/config.py:_sanitize_env_lines */
char *config_py_sanitize_env_lines(const char *text);

/* ===================================================================
 * port_config_py_io.c  (filesystem / config-load I/O)
 * =================================================================== */

/* _ensure_default_soul_md — seed/upgrade SOUL.md in HERMES_HOME. */
/* PoP: _ensure_default_soul_md @ hermes_cli/config.py:_ensure_default_soul_md */
void config_py_ensure_default_soul_md(const char *home);

/* _ensure_hermes_home_managed — managed-mode home verification + SOUL.md seed. */
/* PoP: _ensure_hermes_home_managed @ hermes_cli/config.py:_ensure_hermes_home_managed */
int config_py_ensure_hermes_home_managed(const char *home); /* 0 ok, <0 error */

/* _backup_corrupt_config — snapshot a broken config.yaml to .corrupt.<ts>.bak. */
/* PoP: _backup_corrupt_config @ hermes_cli/config.py:_backup_corrupt_config */
char *config_py_backup_corrupt_config(const char *config_path);

/* _warn_config_parse_failure — log + stderr a parse failure, snapshot backup. */
/* PoP: _warn_config_parse_failure @ hermes_cli/config.py:_warn_config_parse_failure */
void config_py_warn_config_parse_failure(const char *config_path, const char *exc_msg,
                                          int fallback_last_known_good);

/* _warn_once_per_provider — deduplicated warning. */
/* PoP: _warn_once_per_provider @ hermes_cli/config.py:_warn_once_per_provider */
void config_py_warn_once_per_provider(const char *provider_key, const char *signature,
                                       const char *msg);

/* atomic_config_write — require readable + atomic write of config.yaml. */
/* PoP: atomic_config_write @ hermes_cli/config.py:atomic_config_write */
int config_py_atomic_config_write(const char *config_path, const json_t *data);

/* require_readable_config_before_write — refuse to clobber unreadable config. */
/* PoP: require_readable_config_before_write @ hermes_cli/config.py:require_readable_config_before_write */
int config_py_require_readable_config_before_write(const char *config_path);

/* _persist_migration — persist a migrated config (delegates to save). */
/* PoP: _persist_migration @ hermes_cli/config.py:_persist_migration */
int config_py_persist_migration(const json_t *config);

/* Read raw config.yaml (no defaults merged). NULL if absent/empty. */
json_t *config_py_read_raw_config(void);

/* _load_config_impl — load + deep-merge + normalize + expand env refs. */
/* PoP: _load_config_impl @ hermes_cli/config.py:_load_config_impl */
json_t *config_py_load_config_impl(int want_deepcopy);

/* load_config_readonly — the read-only config accessor used by feature-flag
 * checkers (is_feature_on / *_enabled). Real thin wrapper over
 * _load_config_impl (readonly = no mutation, no write path). The Python side
 * reads the same merged config; this makes the dependent config-driven gaps
 * genuinely portable instead of NULL-stubbed.
 * PoP: load_config_readonly @ hermes_cli/config.py:load_config_readonly */
json_t *config_py_load_config_readonly(void);

/* _inject_profile_env_vars — inject profile env vars from config. */
/* PoP: _inject_profile_env_vars @ hermes_cli/config.py:_inject_profile_env_vars */
void config_py_inject_profile_env_vars(json_t *env, const json_t *config);

/* _inject_platform_plugin_env_vars — inject platform plugin env vars. */
/* PoP: _inject_platform_plugin_env_vars @ hermes_cli/config.py:_inject_platform_plugin_env_vars */
void config_py_inject_platform_plugin_env_vars(json_t *env, const json_t *config);

/* get_env_value_prefer_dotenv — read value preferring .env over config.yaml. */
/* PoP: get_env_value_prefer_dotenv @ hermes_cli/config.py:get_env_value_prefer_dotenv */
char *config_py_get_env_value_prefer_dotenv(const char *key);

/* write_platform_config_field — persist one platforms.<key>.<field>. */
/* PoP: write_platform_config_field @ hermes_cli/config.py:write_platform_config_field */
int config_py_write_platform_config_field(const char *platform_key, const char *field_key,
                                           const json_t *value, int raw);

/* get_compatible_custom_providers — unified custom-provider view. */
/* PoP: get_compatible_custom_providers @ hermes_cli/config.py:get_compatible_custom_providers */
json_t *config_py_get_compatible_custom_providers(const json_t *config);

/* get_custom_provider_tls_settings — TLS settings for a matching base_url. */
/* PoP: get_custom_provider_tls_settings @ hermes_cli/config.py:get_custom_provider_tls_settings */
json_t *config_py_get_custom_provider_tls_settings(const char *base_url, const json_t *custom_providers);

/* get_custom_provider_extra_headers — extra_headers for a matching base_url. */
/* PoP: get_custom_provider_extra_headers @ hermes_cli/config.py:get_custom_provider_extra_headers */
json_t *config_py_get_custom_provider_extra_headers(const char *base_url, const json_t *custom_providers);

/* apply_custom_provider_tls_to_client_kwargs — attach TLS knobs to kwargs. */
/* PoP: apply_custom_provider_tls_to_client_kwargs @ hermes_cli/config.py:apply_custom_provider_tls_to_client_kwargs */
void config_py_apply_custom_provider_tls_to_client_kwargs(json_t *client_kwargs, const char *base_url,
                                                           const json_t *custom_providers);

/* apply_custom_provider_extra_headers_to_client_kwargs — merge headers onto kwargs. */
/* PoP: apply_custom_provider_extra_headers_to_client_kwargs @ hermes_cli/config.py:apply_custom_provider_extra_headers_to_client_kwargs */
void config_py_apply_custom_provider_extra_headers_to_client_kwargs(json_t *client_kwargs, const char *base_url,
                                                                   const json_t *custom_providers);

/* recommended_update_command_for_method — update command for an install method. */
/* PoP: recommended_update_command_for_method @ hermes_cli/config.py:recommended_update_command_for_method */
void config_py_recommended_update_command_for_method(const char *method, char *out, size_t out_size);

#ifdef __cplusplus
}
#endif

#endif /* PORT_CONFIG_PY_HELPERS_H */
