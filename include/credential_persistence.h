/*
 * credential_persistence.h — minimal declaration surface for the
 * credential-pool disk-boundary sanitization helpers ported from
 * agent/credential_persistence.py (no god header, C11 only).
 *
 * This is the SINGLE authoritative surface for the shared sanitization
 * helpers. Both src/agent/credential_persistence.c (pure helpers) and
 * src/agent/credential_pool_persistence.c (sanitize / borrowed-source policy)
 * reuse these symbols rather than re-implementing them.
 */

#ifndef SLERMES_CREDENTIAL_PERSISTENCE_H
#define SLERMES_CREDENTIAL_PERSISTENCE_H

#include <stddef.h>
#include <stdbool.h>

/* Forward decl — libjson's json_t. We avoid pulling json.h into this slim
 * header; callers that pass json payloads include hermes_json.h themselves. */
struct json_t;

/* ---------------------------------------------------------------------------
 * Pure key helpers (ports of agent/credential_persistence.py)
 * ------------------------------------------------------------------------- */

/* Port of _normalize_key(): lowercases, replaces '-'/'.' with '_',
 * inserts '_' at [a-z0-9]|[A-Z] camelCase boundaries, strips surrounding
 * whitespace. */
void credential_normalize_key(const char *key, char *out, size_t out_sz);

/* Port of _is_secret_payload_key(): true if the (normalized) key names a
 * secret value (and is not a safe-metadata key). */
bool is_secret_payload_key(const char *key);

/* Port of _fingerprint_value(): SHA-256 of the value, rendered as
 * "sha256:<first 16 hex>". Returns NULL for NULL/empty input. Caller frees. */
char *fingerprint_value(const char *value);

/* Port of _credential_secret_fingerprint(): walk a JSON object looking for a
 * well-known or secret-named value field and return its fingerprint, or pass
 * through an existing "sha256:..." fingerprint. Caller frees. Returns NULL if
 * none found. */
char *credential_secret_fingerprint(const struct json_t *payload);

#endif /* SLERMES_CREDENTIAL_PERSISTENCE_H */
