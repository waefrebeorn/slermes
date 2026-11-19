/* port_model_switch_flow.h — hermes_cli/model_switch.py switching FLOW port.
 *
 * Decision-sequence port of the /model switching pipeline: alias resolution
 * ordering, PATH A / PATH B routing, the validation chain, credential
 * endpoint decisions and the config-write / rollback branches. Every
 * file/network/config/credential dependency is an INJECTED seam: one
 * universal callback driven by scripted results, recording an ordered trace.
 *
 * Universal seam contract (shared with tests/oracle_model_switch_flow.py):
 * the port calls msf_call(ctx, name, args); the harness supplies the
 * recording stub. args ownership is TRANSFERRED (NULL -> empty object);
 * the result is a NEW json_t (NULL == None).
 *
 * Seam names -> live call (None-valued arg keys are omitted on BOTH sides):
 *   resolve_provider_full          providers.resolve_provider_full(name, user_providers, custom_providers)
 *   normalize_provider             providers.normalize_provider(name)
 *   get_provider_profile           providers.get_provider_profile(provider)
 *   get_provider                   providers.get_provider(name, allow_network=False)
 *   list_provider_models           agent.models_dev.list_provider_models(provider)
 *   static_catalog                 hermes_cli.models.static_catalog(provider)
 *   is_aggregator                  providers.is_aggregator(provider)
 *   ensure_direct_aliases          model_switch._ensure_direct_aliases()
 *   direct_alias_api_key           model_switch.direct_alias_api_key(alias)
 *   direct_alias_runtime_request   model_switch.direct_alias_runtime_request(alias)
 *   may_reuse_session_credential   model_switch._may_reuse_session_credential(session_base_url, alias_base_url)
 *   resolve_runtime_provider       runtime_provider.resolve_runtime_provider(**kwargs)
 *   auto_detect_local_model        runtime_provider._auto_detect_local_model(base_url)
 *   moa_default_preset             model_switch._moa_default_preset()
 *   provider_install_hint          left_core_migration.provider_install_hint(provider)
 *   validate_config_structure      hermes_cli.config.validate_config_structure()
 *   get_authenticated_provider_slugs  model_switch.get_authenticated_provider_slugs(**kwargs)
 *   get_label                      providers.get_label(provider)
 *   resolve_alias_fallback         model_switch._resolve_alias_fallback(raw_input, authenticated_providers, up, cp)
 *   configured_provider_matches    model_switch._configured_provider_matches(model_name, up, cp)
 *   persisted_provider_id          runtime_provider_custom.persisted_provider_id(target_provider, user_providers)
 *   custom_provider_aliases        providers.custom_provider_aliases(name, provider_key)
 *   base_url_hostname              utils.base_url_hostname(url)
 *   route_is_welcome_host          anon_auth.route_is_welcome_host(base_url)
 *   detect_provider_for_model      hermes_cli.models.detect_provider_for_model(model_name, current_provider)
 *   load_config                    hermes_cli.config.load_config()
 *   normalize_moa_config           moa_config.normalize_moa_config(cfg)
 *   exact_moa_preset_name          moa_config.exact_moa_preset_name(normalized, raw_input)
 *   normalize_model_for_provider   model_normalize.normalize_model_for_provider(model, provider)
 *   is_known_non_chat_model        chat_catalog.is_known_non_chat_model(model)
 *   get_ollama_request_headers     models_local._get_ollama_request_headers()
 *   extra_headers_from_config      model_switch_providers._extra_headers_from_config(cfg)
 *   validate_requested_model       models_validate.validate_requested_model(model, provider, **kwargs)
 *   is_provider_enabled            hermes_cli.config.is_provider_enabled(cfg)
 *   same_ollama_native_root        models_local._same_ollama_native_root(base_url, configured_base)
 *   ollama_configured_base         model_switch._ollama_configured_base()
 *   read_env                       os.environ.get(name, "") in _apply_direct_alias_endpoint's lambda
 *   should_clear_context_pin       route_identity.should_clear_context_pin(cfg_model, model, cfg_base, base, cfg_prov, prov)
 *   normalize_route_base_url       route_identity.normalize_route_base_url(base_url)
 *   get_config_path                hermes_cli.config.get_config_path()
 *   read_user_config_raw           hermes_cli.config.read_user_config_raw(path)
 *   supports_file_tooling          config_backend.supports_file_tooling()
 *   write_changes                  config_backend.write_changes(path, Changes(set=..., unset=...))
 *   atomic_config_replace          hermes_cli.config.atomic_config_replace(path, config)
 *   chmod                          os.chmod(path, mode)
 *
 * Scripted results (case "io" object, seam name -> script):
 *   - absent key            -> single None result (read_env: "" via the get default)
 *   - {"$seq": [v1, v2...]} -> one element consumed per call; exhaustion raises
 *                              RuntimeError("script exhausted: <name>")
 *   - {"__raise__": cls, "msg": m, ...} -> raises that exception class; the
 *     trace "ret" is the WHOLE marker object. AmbiguousAliasError markers
 *     carry "alias"/"provider"/"candidates" (the live exception attributes).
 *   - any other JSON value  -> reused for every call (NULL == None)
 *
 * Canonical value representation (both sides):
 *   - tuple/set/frozenset -> JSON array (sets sorted by code point);
 *     custom_provider_aliases scripts are SORTED arrays (frozenset canon).
 *   - DirectAlias -> {"model","provider","base_url","api_key","key_env"} dict.
 *   - ProviderDef/profile doubles -> plain attribute dicts (vars); scripts
 *     carry every field the exercised path reads.
 *   - ModelSwitchResult -> full 18-field dict (dataclass defaults filled).
 *   - None -> JSON null (C: NULL pointer or JSON_NULL node, both read None).
 *   - uncaught exception -> {"__raise__": "<ClassName>"} (class only).
 *   - str()/strip()/lower() operand fields are ASCII strings or null in
 *     fixtures (Python container-repr stringification is out of scope).
 *   - DIRECT_ALIASES / MODEL_ALIASES iteration is fixture / source order.
 *
 * _Switch state double: case "in"."st" carries the dataclass fields; missing
 * optional fields are default-filled identically on both sides. Functions
 * that take st mutate it in place; the harness emits the post-call field
 * window as out."state" ({} for the non-st functions).
 *
 * Pure claimed siblings (_clean, _declared_model_ids, _declared_item_id,
 * _model_sort_key, _split_version_suffix, _ambiguous_alias_message,
 * _bare_custom_provider_def, _resolve_named_custom_model_id) and gap
 * siblings used only as internal steps (_custom_entries, _entry_aliases,
 * _current_provider_match, _route_alias_fallback, _convert_vendor_colon_slug,
 * _names_known_provider, _entry_configured_key) are transcribed as INTERNAL
 * helpers (no PoP claim) and stay REAL in the oracle via AST extraction.
 */
#ifndef PORT_MODEL_SWITCH_FLOW_H
#define PORT_MODEL_SWITCH_FLOW_H

#include "hermes_json.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct msf_ctx {
    json_t *io;        /* borrowed: seam scripts */
    json_t *env;       /* borrowed: scripted tables/constants */
    json_t *trace;     /* owned: ordered seam events */
    json_t *counters;  /* owned: per-seam $seq cursors */
    json_t *exc;       /* owned: pending exception marker or NULL */
} msf_ctx;

/* Core seam plumbing (harness wires ctx->io; port code only calls msf_call). */
json_t *msf_call(msf_ctx *ctx, const char *name, json_t *args);
bool msf_exc_pending(const msf_ctx *ctx);
void msf_exc_clear(msf_ctx *ctx);
void msf_raise(msf_ctx *ctx, const char *cls);
bool msf_exc_is(const msf_ctx *ctx, const char *cls);
/* True when the pending exception is caught by `except Exception`
 * (everything except SystemExit / KeyboardInterrupt / GeneratorExit). */
bool msf_exc_is_exception(const msf_ctx *ctx);

/* _Switch state-double helpers (live dataclass methods; no PoP claim). */
json_t *msf_switch_new(const json_t *fields);
json_t *msf_fail(msf_ctx *ctx, const json_t *st, json_t *message, const json_t *fields);
json_t *msf_fail_on_target(msf_ctx *ctx, const json_t *st, json_t *message);
bool msf_provider_changed(const json_t *st);

/* Ported flows/leaves — PoP credits live in the .c file. */
json_t *msf_provider_identity(msf_ctx *ctx, const char *name, const json_t *user_providers,
                              const json_t *custom_providers);
json_t *msf_resolve_alias(msf_ctx *ctx, const char *raw_input, const char *current_provider,
                          const json_t *user_providers, const json_t *custom_providers);
json_t *msf_external_process_catalog(msf_ctx *ctx, const char *provider);
json_t *msf_external_process_match(msf_ctx *ctx, const json_t *catalog, const json_t *aliases,
                                   const char *typed, const char *provider);
json_t *msf_aggregator_catalog_match(msf_ctx *ctx, const char *new_model, const json_t *catalog);
json_t *msf_config_declares_model(msf_ctx *ctx, const char *new_model, const char *target_provider,
                                  const char *base_url, const json_t *user_providers,
                                  const json_t *custom_providers);
json_t *msf_unknown_provider_message(msf_ctx *ctx, const char *explicit_provider);
json_t *msf_aggregator_alias_error(msf_ctx *ctx, const char *explicit_provider, const char *target_provider,
                                   const char *current_provider, const json_t *user_providers,
                                   const json_t *custom_providers);
json_t *msf_apply_direct_alias_endpoint(msf_ctx *ctx, json_t *st, const json_t *da);
json_t *msf_route_explicit_provider(msf_ctx *ctx, json_t *st);
json_t *msf_route_from_model_input(msf_ctx *ctx, json_t *st);
json_t *msf_route_configured_provider(msf_ctx *ctx, json_t *st);
json_t *msf_validate_switch(msf_ctx *ctx, json_t *st);
json_t *msf_model_selection_config_updates(msf_ctx *ctx, const json_t *result, const json_t *current_model_cfg);
json_t *msf_selection_route_changed(msf_ctx *ctx, const json_t *model_cfg, const json_t *result);
json_t *msf_merge_selection_updates(msf_ctx *ctx, const json_t *model_cfg, const json_t *updates,
                                    bool mask_cleared);
json_t *msf_apply_model_selection(msf_ctx *ctx, const json_t *model_cfg, const json_t *result,
                                  bool mask_cleared);
json_t *msf_persist_model_selection(msf_ctx *ctx, const json_t *result, const char *config_path);

#ifdef __cplusplus
}
#endif

#endif /* PORT_MODEL_SWITCH_FLOW_H */
