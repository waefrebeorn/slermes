/*
 * libproviderauth.h — Provider authentication registry (faithful port of
 * Python hermes_cli.auth.PROVIDER_REGISTRY, keyed by the same 45 aliased
 * provider names (regenerated 2026-07-31; `vertex` added, matching LIVE Python).
 *
 * This is a SELF-CONTAINED lookup table: it has no dependency on the
 * instance-oriented provider_metadata.c registry (which knows only ~26
 * distinct provider families and carries no auth_type) nor on the heavy
 * libdb/hermes.h chain. The table below is regenerated from the LIVE Python
 * PROVIDER_REGISTRY via tests/sta_oracle_provider_pool_setup.py, so the
 * oracle diffs the C table against LIVE Python and catches any drift.
 *
 * The registry answers one question used by hermes_cli/setup.py:
 *   _supports_same_provider_pool_setup(provider)  ->
 *       provider in {api_key, oauth_device_code}
 * plus "custom" -> False, "openrouter" -> True special cases.
 */

#ifndef LIB_PROVIDER_AUTH_H
#define LIB_PROVIDER_AUTH_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Opaque provider auth record (defined privately in provider_auth.c). */
typedef struct provider_auth_entry_t provider_auth_entry_t;

/*
 * Provider auth-type classification, mirroring the Python registry's
 * auth_type strings. Only API_KEY and OAUTH_DEVICE_CODE participate in the
 * "same provider pool" (multi-key rotation) feature.
 */
typedef enum {
    PROVIDER_AUTH_UNKNOWN = 0,
    PROVIDER_AUTH_API_KEY,          /* "api_key" */
    PROVIDER_AUTH_OAUTH_DEVICE_CODE,/* "oauth_device_code" */
    PROVIDER_AUTH_OAUTH_EXTERNAL,   /* "oauth_external" */
    PROVIDER_AUTH_AWS_SDK,          /* "aws_sdk" */
    PROVIDER_AUTH_EXTERNAL_PROCESS, /* "external_process" */
    PROVIDER_AUTH_OAUTH_MINIMAX     /* "oauth_minimax" */
} provider_auth_type_t;

/*
 * Look up a provider by its (case-insensitive) aliased name and return its
 * auth type. Returns PROVIDER_AUTH_UNKNOWN if the provider is not in the
 * registry.
 */
provider_auth_type_t provider_auth_lookup(const char *provider);

/*
 * Faithful port of hermes_cli/setup.py:_supports_same_provider_pool_setup().
 * "custom" -> false; "openrouter" -> true; otherwise the provider's auth_type
 * must be API_KEY or OAUTH_DEVICE_CODE. Unknown providers -> false.
 *
 * This is the live, registry-driven implementation (no hardcoded snapshot in
 * the caller) — the table is owned by this module.
 */
bool provider_auth_supports_pool(const char *provider);

/*
 * Iterate the registry (for oracle/exhaustive verification). `idx` starts at
 * 0 and increments; returns false when exhausted. Writes the provider name
 * (borrowed, do not free) and its auth type into the out-params.
 */
bool provider_auth_iterate(size_t *idx, const char **out_name,
                           provider_auth_type_t *out_type);

#ifdef __cplusplus
}
#endif

#endif /* LIB_PROVIDER_AUTH_H */
