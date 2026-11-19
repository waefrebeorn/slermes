/* port_models_catalog_flow.h — hermes_cli/models.py catalog/selection FLOW port.
 *
 * Decision-sequence port of the model-catalog listing/filtering ladder,
 * provider/model resolution, alias canonicalization and cache-serve decisions.
 * Every file/network/config dependency is an INJECTED seam: one universal
 * callback driven by scripted results, recording an ordered trace.
 *
 * Universal seam contract (shared with tests/oracle_models_catalog_flow.py):
 *   json_t *(*mcat_seam_fn)(void *ctx, const char *name, json_t *args)
 * is NOT used directly by callers; the port calls mcat_call(ctx, name, args)
 * which dispatches through ctx->seam. The harness supplies a recording stub.
 *
 * Seam names -> live call (all args are borrowed JSON objects; None-valued
 * keys are omitted on BOTH sides):
 *   load_config                          hermes_cli.config.load_config()
 *   get_model_config_dict                models._get_model_config_dict()
 *   get_provider_profile                 providers.get_provider_profile(provider)
 *   list_providers                       providers.list_providers()
 *   resolve_api_key_provider_credentials hermes_cli.auth.resolve_api_key_provider_credentials(provider)
 *   registry_entry                       hermes_cli.auth.PROVIDER_REGISTRY.get(provider)
 *   relay_probe_api_key                  auth_declared_providers.relay_probe_api_key(provider, relay, api_key, base_url)
 *   fetch_api_models                     models.fetch_api_models(api_key, base_url)
 *   fetch_models                         profile.fetch_models(**kwargs)
 *   model_alias_canonical                hermes_cli.model_search.model_alias_canonical(key)
 *   detect_static_provider_for_model     models.detect_static_provider_for_model(model_name, current_provider)
 *   model_in_provider_catalog            models._model_in_provider_catalog(name_lower, providers)
 *   find_openrouter_slug                 models._find_openrouter_slug(model_name)
 *   normalize_route_base_url             hermes_cli.route_identity.normalize_route_base_url(base_url)
 *
 * Scripted results (case "io" object, seam name -> script):
 *   - absent key            -> single None result
 *   - {"$seq": [v1, v2...]} -> one element consumed per call; exhaustion raises
 *                              RuntimeError("script exhausted: <name>")
 *   - {"__raise__": cls, "msg": m} -> raises that exception class
 *   - any other JSON value  -> reused for every call (NULL == None)
 *
 * Canonical value representation (both sides):
 *   - tuple/set/frozenset -> JSON array (sets sorted by code point)
 *   - CuratedFallbackModels -> {"$curated": [items...]} (list-subclass tag)
 *   - plain list -> JSON array; None -> JSON null (C: NULL pointer)
 *   - uncaught exception -> {"__raise__": "<ClassName>"} (class only)
 *   - str()/strip()/lower() operand fields are ASCII strings or null in
 *     fixtures (Python container-repr stringification is out of scope).
 *   - _BORROWED_MODEL_PROVIDERS iteration is modeled in fixture order (the
 *     live frozenset order is hash-randomized); membership sets are unordered.
 *
 * Pure claimed siblings (normalize_provider, _provider_keys,
 * _cache_entry_valid, _model_dedup_key) are transcribed as INTERNAL helpers
 * (no PoP claim) and stay REAL in the oracle via AST extraction.
 */
#ifndef PORT_MODELS_CATALOG_FLOW_H
#define PORT_MODELS_CATALOG_FLOW_H

#include "hermes_json.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mcat_ctx {
    json_t *io;        /* borrowed: seam scripts */
    json_t *env;       /* borrowed: scripted tables/constants */
    json_t *trace;     /* owned: ordered seam events */
    json_t *counters;  /* owned: per-seam $seq cursors */
    json_t *exc;       /* owned: pending exception marker or NULL */
} mcat_ctx;

/* Core seam plumbing (harness wires ctx->io; port code only calls mcat_call). */
json_t *mcat_call(mcat_ctx *ctx, const char *name, json_t *args);
bool mcat_exc_pending(const mcat_ctx *ctx);
void mcat_exc_clear(mcat_ctx *ctx);
void mcat_raise(mcat_ctx *ctx, const char *cls);

/* Ported flows/leaves — PoP credits live in the .c file. */
json_t *mcat_merge_unique(mcat_ctx *ctx, const json_t *primary, const json_t *secondary, bool dedup_key);
json_t *mcat_configured_provider_ids(mcat_ctx *ctx);
json_t *mcat_resolve_provider_prefix(mcat_ctx *ctx, const char *model_name);
json_t *mcat_static_catalog(mcat_ctx *ctx, const char *provider);
json_t *mcat_static_catalog_providers(mcat_ctx *ctx);
json_t *mcat_static_catalog_matches(mcat_ctx *ctx, const char *name, const char *current_provider);
json_t *mcat_detection_candidates(mcat_ctx *ctx, const char *name, const char *current_provider);
json_t *mcat_catalog_fetcher(mcat_ctx *ctx, const char *normalized);
json_t *mcat_api_key_credentials(mcat_ctx *ctx, const char *normalized);
json_t *mcat_api_key_provider_live(mcat_ctx *ctx, const char *normalized, bool force_refresh);
json_t *mcat_stepfun_catalog(mcat_ctx *ctx, const char *normalized, bool force_refresh);
json_t *mcat_profile_live_catalog(mcat_ctx *ctx, const char *normalized);
json_t *mcat_probe_profile_catalog(mcat_ctx *ctx, const char *normalized, const json_t *profile,
                                   const char *api_key, const char *base_url);
json_t *mcat_merge_profile_catalog(mcat_ctx *ctx, const char *normalized, const json_t *profile,
                                   const json_t *live);
json_t *mcat_drop_delisted_opencode_models(mcat_ctx *ctx, const char *normalized, json_t *rows);
json_t *mcat_configured_relay_base_url(mcat_ctx *ctx, const char *provider);
json_t *mcat_relay_model_catalog(mcat_ctx *ctx, const char *normalized, const char *relay);
json_t *mcat_disk_serve_tier(mcat_ctx *ctx, const json_t *entry, const char *fp, double now,
                             bool is_ollama, double ttl_seconds);

#ifdef __cplusplus
}
#endif

#endif /* PORT_MODELS_CATALOG_FLOW_H */
