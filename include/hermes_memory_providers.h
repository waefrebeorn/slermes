/*
 * hermes_memory_providers.h — C port of hermes_cli/memory_providers.py
 *
 * Declarative configuration schema for desktop memory providers.
 * Pure data: a static registry of declared providers and their fields.
 * The web_server owns the generic read/write logic that interprets these
 * declarations against config.yaml / the provider config file / the env store.
 */

#ifndef HERMES_MEMORY_PROVIDERS_H
#define HERMES_MEMORY_PROVIDERS_H

#include <stddef.h>

typedef enum {
    HERMES_MP_KIND_TEXT = 0,
    HERMES_MP_KIND_SELECT,
    HERMES_MP_KIND_SECRET
} hermes_mp_kind_t;

/* A single choice for a `select` field. */
typedef struct {
    const char *value;
    const char *label;
    const char *description;
} hermes_mp_option_t;

/* One configurable field on a memory provider. */
typedef struct {
    const char        *key;
    const char        *label;
    hermes_mp_kind_t  kind;
    const char        *default_val;
    const char        *description;
    const char        *placeholder;
    const hermes_mp_option_t *options;
    int                noptions;
    const char        *env_key;   /* non-NULL only for KIND_SECRET */
} hermes_mp_field_t;

/* A declared memory provider and its configurable fields. */
typedef struct {
    const char            *name;
    const char            *label;
    const hermes_mp_field_t *fields;
    int                    nfields;
} hermes_mp_provider_t;

/*
 * Return the declared provider for `name`, or NULL if undeclared.
 * Faithful to memory_providers.get_memory_provider.
 */
const hermes_mp_provider_t *hermes_mp_get_provider(const char *name);

/*
 * Return the allowed option `value`s for field `key` on `provider`
 * (the set equivalent of ProviderField.allowed_values). Fills `out` with
 * pointers into the provider's static option table (do NOT free them) and
 * sets *out_n. Returns the count (which may be 0 for non-select fields).
 * Faithful to memory_providers.ProviderField.allowed_values.
 */
int hermes_mp_allowed_values(const hermes_mp_provider_t *provider,
                             const char *key,
                             const char *out[], int out_cap);

#endif /* HERMES_MEMORY_PROVIDERS_H */
