/* port_ms_providers_flow.h — hermes_cli/model_switch_providers.py FLOW port.
 *
 * Decision-sequence port of the picker provider-listing pipeline: credential
 * discovery ladders (env / SDK / auth-store / pool), curated-vs-live model-id
 * resolution ordering, the Nous free-tier / policy / usage branches, custom
 * endpoint probe/cache decisions and the row finalization post-passes. Every
 * config/credential/network dependency is an INJECTED seam: one universal
 * callback driven by scripted results, recording an ordered trace.
 *
 * Universal seam contract (shared with tests/oracle_ms_providers_flow.py):
 * the port calls msp_call(ctx, name, args); the harness supplies the recording
 * stub. args ownership is TRANSFERRED (NULL -> empty object); the result is a
 * NEW json_t (NULL == None).
 *
 * Seam names -> live call (None-valued arg keys are omitted on BOTH sides):
 *   read_env                       os.environ.get(name, "") (singleton double)
 *   load_auth_store                hermes_cli.auth._load_auth_store()
 *   credential_pool_is_usable      model_switch_providers._credential_pool_is_usable(provider, **kw)
 *   has_vertex_credentials         agent.vertex_adapter.has_vertex_credentials()
 *   get_model_config_dict          hermes_cli.models._get_model_config_dict()
 *   has_aws_credentials            agent.bedrock_adapter.has_aws_credentials()
 *   cached_provider_model_ids      hermes_cli.models.cached_provider_model_ids(slug, **kw)
 *   merge_with_models_dev          hermes_cli.models._merge_with_models_dev(slug, model_ids)
 *   without_generation_models      hermes_cli.chat_catalog.without_generation_models(model_ids)
 *   get_pricing_for_provider       hermes_cli.models_pricing.get_pricing_for_provider(provider, **kw)
 *   get_provider_auth_state        hermes_cli.auth.get_provider_auth_state(provider)
 *   check_nous_free_tier           hermes_cli.models.check_nous_free_tier(force_fresh=...)
 *   union_with_portal_free_recommendations  hermes_cli.models.union_with_portal_free_recommendations(ids, pricing, portal)
 *   union_with_portal_paid_recommendations  hermes_cli.models.union_with_portal_paid_recommendations(ids, pricing, portal)
 *   union_with_nous_on_sale_models hermes_cli.models.union_with_nous_on_sale_models(ids, pricing)
 *   nous_policy_allowed_ids        hermes_cli.models_pricing.nous_policy_allowed_ids()
 *   restrict_to_nous_policy        hermes_cli.models_pricing.restrict_to_nous_policy(ids, allowed, rescue_empty=...)
 *   nous_picker_usage              hermes_cli.model_picker_usage.nous_picker_usage(any_age=...)
 *   featured_first                 hermes_cli.model_picker_usage.featured_first(ids, featured)
 *   has_guest                      hermes_cli.anon_auth.has_guest()
 *   guest_allowed                  hermes_cli.anon_auth.guest_allowed()
 *   free_inference_active          hermes_cli.anon_auth.free_inference_active()
 *   fetch_picker_live_models       model_switch_providers._fetch_picker_live_models(api_key, api_url, ncp, preserve, **kw)
 *   cached_fetch_api_models        hermes_cli.models.cached_fetch_api_models(api_key, api_url, **kw)
 *   is_provider_enabled            hermes_cli.config.is_provider_enabled(cfg)
 *   model_requires_account_discovery  hermes_cli.models._model_requires_account_discovery(slug, model)
 *   load_config                    hermes_cli.config.load_config()
 *   same_base_url_origin           utils.same_base_url_origin(url, base_url)
 *
 * Scripted results (case "io" object, seam name -> script):
 *   - absent key            -> single None result (read_env: "" via the get default)
 *   - {"$seq": [v1, v2...]} -> one element consumed per call; exhaustion raises
 *                              RuntimeError("script exhausted: <name>")
 *   - {"__raise__": cls, "msg": m, ...} -> raises that exception class; the
 *     trace "ret" is the WHOLE marker object.
 *   - {"__native__": [...]} -> a _NativePickerModelList (fetch_picker_live_models
 *     / cached_fetch_api_models only); the marker is stripped to a plain array
 *     in canonical returns.
 *   - any other JSON value  -> reused for every call (NULL == None)
 *
 * Canonical value representation (both sides):
 *   - tuple/set/frozenset -> JSON array (sets sorted by code point).
 *   - _NativePickerModelList -> plain array in returns; nativeness is an
 *     internal decision only (the "__native__" script marker).
 *   - overlay / PROVIDER_REGISTRY / usage doubles -> attribute dicts.
 *   - None -> JSON null (C: NULL pointer or JSON_NULL node, both read None).
 *   - uncaught exception -> {"__raise__": "<ClassName>"} (class only).
 *   - str()/strip()/lower() operand fields are ASCII strings or null in
 *     fixtures; the only non-ASCII literals are the live FREE_TIER_LABEL
 *     ("Nous · free tier") and the em-dash separator in row names.
 *   - Raise markers use exact builtin class names (or HTTPException, mapped to
 *     http.client.HTTPException); the C matcher walks a builtin subclass chain.
 *
 * State window: _finalize_picker_rows mutates its results argument's row dicts
 * in place (current-model injection); the harness emits the post-call results
 * array as out."state" ({} for every other function).
 */
#ifndef PORT_MS_PROVIDERS_FLOW_H
#define PORT_MS_PROVIDERS_FLOW_H

#include "hermes_json.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct msp_ctx {
    json_t *io;        /* borrowed: seam scripts */
    json_t *env;       /* borrowed: scripted tables/constants */
    json_t *trace;     /* owned: ordered seam events */
    json_t *counters;  /* owned: per-seam $seq cursors */
    json_t *exc;       /* owned: pending exception marker or NULL */
} msp_ctx;

/* Core seam plumbing (harness wires ctx->io; port code only calls msp_call). */
json_t *msp_call(msp_ctx *ctx, const char *name, json_t *args);
bool msp_exc_pending(const msp_ctx *ctx);
void msp_exc_clear(msp_ctx *ctx);
void msp_raise(msp_ctx *ctx, const char *cls);
bool msp_exc_is(const msp_ctx *ctx, const char *cls);
/* True when the pending exception is caught by `except Exception`. */
bool msp_exc_is_exception(const msp_ctx *ctx);
/* True when the pending exception matches one of `classes` (subclass-aware). */
bool msp_exc_matches(const msp_ctx *ctx, const char *const *classes, size_t n);

/* Ported flows/leaves — PoP credits live in the .c file. */
json_t *msp_discovered_catalog_stale(msp_ctx *ctx, const json_t *entry, const json_t *model_ids);
json_t *msp_any_env(msp_ctx *ctx, const json_t *env_vars);
json_t *msp_skip(msp_ctx *ctx, const json_t *seen, const json_t *excluded, const json_t *keys);
json_t *msp_auth_store_has_provider(msp_ctx *ctx, const json_t *keys);
json_t *msp_raw_pool_usable(msp_ctx *ctx, const char *hermes_id, bool for_picker);
json_t *msp_pool_usable(msp_ctx *ctx, const char *slug, bool for_picker);
json_t *msp_overlay_has_env_creds(msp_ctx *ctx, const char *pid, const char *hermes_slug,
                                  const json_t *overlay);
json_t *msp_azure_entra_configured(msp_ctx *ctx);
json_t *msp_has_fast_aws_sdk_signal(msp_ctx *ctx);
json_t *msp_has_aws_sdk_creds_for_listing(msp_ctx *ctx, const char *slug, const char *current_provider);
json_t *msp_live_or_curated_ids(msp_ctx *ctx, const char *slug, const json_t *curated,
                                const json_t *fallback_keys, bool merge_models_dev, bool non_blocking);
json_t *msp_aws_live_or_curated_ids(msp_ctx *ctx, const char *slug, const json_t *curated,
                                    const json_t *fallback_keys, bool non_blocking);
json_t *msp_nous_picker_model_ids(msp_ctx *ctx, const json_t *curated, bool force_fresh_nous_tier);
json_t *msp_free_tier_nous_row(msp_ctx *ctx, const json_t *row);
json_t *msp_cap_models(msp_ctx *ctx, const json_t *model_ids, const json_t *max_models, const char *slug);
json_t *msp_discover_endpoint_models(msp_ctx *ctx, const json_t *api_key, const char *api_url,
                                     const char *native_catalog_provider, bool has_explicit_models,
                                     const json_t *headers, const json_t *api_mode, bool probe_live,
                                     bool discovery_allowed, bool fast_custom_probe);
json_t *msp_finalize_picker_rows(msp_ctx *ctx, json_t *results, const json_t *user_providers,
                                 const char *current_model);
json_t *msp_saved_entry_on_other_origin(msp_ctx *ctx, const char *name, const char *base_url);

#ifdef __cplusplus
}
#endif

#endif /* PORT_MS_PROVIDERS_FLOW_H */