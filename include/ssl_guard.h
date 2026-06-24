/*
 * ssl_guard.h — Preventive SSL CA certificate checks for Hermes C.
 * AG40: Port of Python agent/ssl_guard.py
 *
 * Catches broken CA bundle paths before OpenSSL/httpx turns them into
 * opaque "No such file or directory" failures.
 */

#ifndef SSL_GUARD_H
#define SSL_GUARD_H

#include "hermes_error.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Check if SSL guard is disabled via HERMES_SKIP_SSL_GUARD env var */
bool ssl_guard_skip_enabled(void);

/* Verify configured and bundled CA certificates are present and loadable.
 *
 * Returns HERMES_OK on success, or an error code:
 *   HERMES_ERR_FILE_NOT_FOUND  - CA bundle path doesn't exist
 *   HERMES_ERR_INVALID_FORMAT    - CA bundle is not a valid file or cert
 *   HERMES_ERR_CONFIG            - certifi not available or no certs loaded
 *
 * If HERMES_SKIP_SSL_GUARD is set, returns HERMES_OK without checking. */
hermes_error_t ssl_guard_verify_ca_bundle(void);

/* Backward-compatible wrapper (same behavior as ssl_guard_verify_ca_bundle). */
hermes_error_t ssl_guard_verify_ca_bundle_with_fallback(void);

#ifdef __cplusplus
}
#endif

#endif /* SSL_GUARD_H */