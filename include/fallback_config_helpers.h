/*
 * fallback_config_helpers.h — public API for the pure hermes_cli/
 * fallback_config.py helpers. Opaque, minimal includes (forward-declares json_t).
 */

#ifndef FALLBACK_CONFIG_HELPERS_H
#define FALLBACK_CONFIG_HELPERS_H

#include <stddef.h>

typedef struct json_t json_t;

typedef struct {
    char *provider;   /* malloc'd */
    char *model;      /* malloc'd */
    char *base_url;   /* malloc'd ("" when absent) */
} fallback_entry_t;

/* Strip whitespace + trailing '/'. Non-string -> "". Caller frees.
 * (PoP: _normalized_base_url) */
char *fallback_config_normalize_base_url(const char *value);

/* Normalize a dict or list of dicts into entries (skips empty provider/model).
 * Caller frees via fallback_config_free_entries. (PoP: _iter_fallback_entries) */
fallback_entry_t *fallback_config_iter_entries(const json_t *raw, int *out_count);

/* Lowercased (provider, model, base_url) identity triple. (PoP: _entry_identity) */
void fallback_config_entry_identity(const fallback_entry_t *entry,
                                    char *prov, char *model, char *base, size_t sz);

/* Merge fallback_providers then fallback_model; drop duplicates by identity.
 * Caller frees via fallback_config_free_entries. (PoP: get_fallback_chain) */
fallback_entry_t *fallback_config_get_chain(const json_t *config, int *out_count);

void fallback_config_free_entries(fallback_entry_t *entries, int count);

#endif /* FALLBACK_CONFIG_HELPERS_H */
