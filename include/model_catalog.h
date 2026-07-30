/*
 * model_catalog.h — opaque model-catalog API.
 *
 * Faithful C11 port of the static provider/model catalog and the pure
 * provider-resolution helpers from hermes_cli/models.py. The catalog itself
 * (PROVIDER_MODELS, PROVIDER_ALIASES, PROVIDER_GROUPS, PROVIDER_LABELS) lives
 * in model_catalog.c and is never exposed here — callers go through the
 * documented functions only.
 *
 * Network-dependent functions (provider_model_ids live fetch, pricing fetch)
 * degrade to the static catalog when no live hook is available, mirroring the
 * Python contract where "failures degrade to returning the static list".
 */

#ifndef SLERMES_MODEL_CATALOG_H
#define SLERMES_MODEL_CATALOG_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Provider normalization / labels ─────────────────────────────────── */

/* Normalize a provider alias to Hermes' canonical provider id.
 * "auto" passes through unchanged. Result is a static string (do not free). */
const char *model_normalize_provider(const char *provider);

/* Human-friendly label for a provider id or alias (static string). */
const char *model_provider_label(const char *provider);

/* Group id a provider slug belongs to, or "" if ungrouped (static string). */
const char *model_provider_group_for_slug(const char *slug);

/* ── Static catalog access ───────────────────────────────────────────── */

/* Number of providers in the static catalog. */
int model_catalog_provider_count(void);

/* Return the Nth provider id (static string), or NULL if out of range. */
const char *model_catalog_provider_at(int idx);

/* Cost-safe default model for a provider, or "" if unknown (static string).
 * For most providers this is the first entry in the static list; metered
 * aggregators carry an explicit low-cost override. */
const char *model_default_model_for_provider(const char *provider);

/* Number of curated models for a provider. */
int model_provider_model_count(const char *provider);

/* True when `model` (case-insensitive) appears in `provider`'s static catalog. */
int model_provider_has_model(const char *provider, const char *model);

/* Return the Nth curated model id for a provider (static string), or NULL. */
const char *model_provider_model_at(const char *provider, int idx);

/* ── Provider grouping for pickers ───────────────────────────────────── */

/* Opaque group row handle. */
typedef struct model_group_row model_group_row_t;

/* Fold an ordered, ';'-separated slug list into group rows.
 * Returns a heap-allocated, '\n'-terminated packed buffer:
 *   "kind\0slug\0group_id\0label\0desc\0members_csv\0" per row, ending \0\0.
 * Caller frees with free(). */
char *model_group_providers(const char *slug_csv);

/* Iterator over group rows produced by model_group_providers().
 * Pass *cursor = packed; each call returns the next row or NULL.
 * On a non-NULL return, the out-params point INTO the packed buffer. */
const model_group_row_t *model_group_next(const char *packed,
                                          const char **cursor,
                                          const char **kind,
                                          const char **slug,
                                          const char **group_id,
                                          const char **label,
                                          const char **desc,
                                          const char **members_csv);

/* ── Model name / provider resolution ────────────────────────────────── */

/* Strip a vendor/ prefix from a model id, returned in out (caller buffer). */
void model_strip_vendor_prefix(const char *model_id, char *out, size_t outsz);

/* Parse "provider:model" (or bare "model") input into (provider, model).
 * provider_out / model_out must be char* buffers. current_provider is used
 * when no explicit provider is present. */
void model_parse_model_input(const char *raw,
                              const char *current_provider,
                              char *provider_out, size_t poutsz,
                              char *model_out, size_t moutsz);

/* Faithful port of models.py:curated_models_for_provider (STATIC-CATALOG
 * fallback). Fills provider_out[i]/model_out[i] with (model_id, "") tuples
 * from the embedded _PROVIDER_MODELS catalog for the normalized provider,
 * returns the count. Live HTTP resolution (openrouter / provider_model_ids)
 * is a separate network-driven layer that calls this as fallback. */
int model_curated_models_for_provider(const char *provider,
                                      char provider_out[][64],
                                      char model_out[][256],
                                      int max);

/* Detect a provider for a model name using static catalogs only.
 * Returns 1 and fills provider_out/model_out (caller buffers) on a confident
 * match, else returns 0 (no match). */
int model_detect_static_provider_for_model(const char *model_name,
                                           const char *current_provider,
                                           char *provider_out, size_t poutsz,
                                           char *model_out, size_t moutsz);

/* ── OpenRouter slug resolution ─────────────────────────────────────── */

/* Find the full OpenRouter model slug for a bare/partial name.
 * Returns a malloc'd string (caller frees) or NULL if not found.
 * NOTE: in the offline port this resolves against the static OpenRouter
 * curated list; the live /v1/models fetch is a no-op that returns NULL. */
char *model_find_openrouter_slug(const char *model_name);

/* ── Fast-mode capability ──────────────────────────────────────────────
 * model_supports_fast_mode() resolves to the canonical definition in
 * port_models_helpers.c (is_openai_fast_model / is_anthropic_fast_model are
 * declared there). We avoid redeclaring them here to prevent symbol clashes. */

int model_supports_fast_mode(const char *model_id);

/* ── Disk-cached provider model ids ─────────────────────────────────── */

/* Load the persisted provider-models cache as a malloc'd JSON string, or NULL.
 * Caller frees. */
char *model_load_provider_models_cache(void);

/* Persist the cache (best-effort). json is the full cache object. */
void model_save_provider_models_cache(const char *json);

/* Disk path to the provider-models cache (malloc'd, caller frees). */
char *model_provider_models_cache_path(void);

/* Credential fingerprint for a provider (malloc'd, caller frees). */
char *model_credential_fingerprint(const char *provider);

/* Return cached-or-live model ids for a provider as a malloc'd JSON array
 * string ("[...]"), or NULL. force_refresh ignores the cache.
 * The live path degrades to the static catalog when unreachable. */
char *model_cached_provider_model_ids(const char *provider, int force_refresh);

/* Drop a provider's cache entry (provider==NULL wipes all). */
void model_clear_provider_models_cache(const char *provider);

/* ── Live catalog fallback (static in offline port) ──────────────────── */

/* Best-known model catalog for a provider as a malloc'd JSON array string,
 * or NULL. Degrades to the static PROVIDER_MODELS catalog. */
char *model_provider_model_ids(const char *provider, int force_refresh);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_MODEL_CATALOG_H */
