/*
 * browser_control_artifacts.h — C11 port of gateway/browser_control_artifacts.py
 *
 * One-shot, scope-bound, TTL-bounded artifact transport store for browser
 * control: server-minted 32-hex ids, exact MIME allowlist, byte caps,
 * SHA-256 provenance, atomic temp+rename writes, one-shot consume on load.
 */
#ifndef SLERMES_BROWSER_CONTROL_ARTIFACTS_H
#define SLERMES_BROWSER_CONTROL_ARTIFACTS_H

#include "hermes_json.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BCA_DEFAULT_TTL_SECONDS 300.0
#define BCA_DEFAULT_MAX_BYTES (10 * 1024 * 1024)
#define BCA_ARTIFACT_ID_HEX 32

/* Error codes (ArtifactError subclasses in Python). */
typedef enum {
    BCA_OK = 0,
    BCA_ERR_ARTIFACT = 1,
    BCA_ERR_NOT_FOUND = 2,
    BCA_ERR_EXPIRED = 3,
    BCA_ERR_TOO_LARGE = 4,
    BCA_ERR_MIME_REJECTED = 5,
    BCA_ERR_SCOPE_MISMATCH = 6,
    BCA_ERR_CHECKSUM = 7,
    BCA_ERR_TRAVERSAL = 8,
    BCA_ERR_OVERWRITE = 9
} bca_error_t;

/* Default exact MIME allowlist (NULL-terminated). */
extern const char *const BCA_DEFAULT_ALLOWED_MIME_TYPES[];

typedef struct bca_store bca_store_t;

/* Provenance receipt returned by store / exposed by validate & load. */
typedef struct {
    char artifact_id[33];
    char sha256[65];
    size_t size_bytes;
    char content_type[128];
    char *filename;      /* owned, bounded */
    double created_at;
    double expires_at;
    double ttl_seconds;
    char scope_key[65];
} bca_receipt_t;

/* Scope identity: only principal_id + transport_family participate. */
typedef struct {
    const char *principal_id;
    const char *transport_family;
} bca_scope_t;

/* artifact_scope_key @ gateway/browser_control_artifacts.py:artifact_scope_key
 * sha256(principal\x00family) hex. Returns BCA_ERR_ARTIFACT (and writes
 * nothing) when the scope carries no resolved principal — fail closed. */
bca_error_t bca_artifact_scope_key(const bca_scope_t *scope, char out[65]);

/* _sha256 @ ...:_sha256 — lowercase hex digest of data. */
void bca_sha256_hex(const unsigned char *data, size_t len, char out[65]);

/* to_dict @ ArtifactReceipt.to_dict — wire receipt (never file paths). */
json_t *bca_receipt_to_dict(const bca_receipt_t *receipt,
                            const char *download_path);

/* _normalize_content_type @ ...:_normalize_content_type — canonical MIME or
 * "" for malformed input. */
void bca_normalize_content_type(const char *value, char *out, size_t out_sz);

/* _bounded_filename @ ...:_bounded_filename — display-only sanitize, limit 160. */
char *bca_bounded_filename(const char *value);

/* Store lifecycle. clock==NULL → wall time; injectable for tests. */
bca_store_t *bca_store_new(const char *root, double ttl_seconds,
                           long max_bytes, const char *const *allowed_mime,
                           double (*clock_fn)(void *));
void bca_store_free(bca_store_t *store);

/* Property accessors. */
const char *bca_store_root(const bca_store_t *store);
double bca_store_ttl_seconds(const bca_store_t *store);
long bca_store_max_bytes(const bca_store_t *store);

/* _sweep_orphan_files @ ...:_sweep_orphan_files — delete id-shaped/*.tmp
 * files with no live index entry; returns count removed. */
int bca_store_sweep_orphan_files(bca_store_t *store);

/* store @ ArtifactStore.store — validate + persist one artifact.
 * On success returns BCA_OK and fills *receipt; else the error code. */
bca_error_t bca_store_store(bca_store_t *store, const unsigned char *data,
                            size_t len, const char *filename,
                            const char *content_type, const bca_scope_t *scope,
                            bca_receipt_t *out_receipt);

/* validate @ ArtifactStore.validate — existence + TTL + scope, no consume. */
bca_error_t bca_store_validate(bca_store_t *store, const char *artifact_id,
                               const bca_scope_t *scope,
                               bca_receipt_t *out_receipt);

/* load @ ArtifactStore.load — verify checksum, read, consume atomically.
 * data is malloc'd (*out_len set), caller frees. */
bca_error_t bca_store_load(bca_store_t *store, const char *artifact_id,
                           const bca_scope_t *scope, unsigned char **out_data,
                           size_t *out_len, bca_receipt_t *out_receipt);

/* prune_expired @ ArtifactStore.prune_expired — count removed. */
int bca_store_prune_expired(bca_store_t *store, const double *now);

/* count @ ArtifactStore.count — live (unconsumed) artifacts. */
int bca_store_count(const bca_store_t *store);

/* ------------------------------------------------------------------ */
/* ArtifactRateLimiter                                                 */
/* ------------------------------------------------------------------ */

typedef struct bca_rate_limiter bca_rate_limiter_t;

bca_rate_limiter_t *bca_rate_limiter_new(double window_seconds,
                                         int max_requests,
                                         double (*clock_fn)(void *));
void bca_rate_limiter_free(bca_rate_limiter_t *lim);

/* allow @ ArtifactRateLimiter.allow */
int bca_rate_limiter_allow(bca_rate_limiter_t *lim, const char *key);

/* reset @ ArtifactRateLimiter.reset */
void bca_rate_limiter_reset(bca_rate_limiter_t *lim, const char *key);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_BROWSER_CONTROL_ARTIFACTS_H */
