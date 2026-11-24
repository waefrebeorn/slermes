/*
 * noc_w24_auth_6661f1.h - C surface for upstream gateway/relay/auth.py
 * (gate w24_auth_6661f1).
 *
 * The relay's WS-upgrade bearer primitives, as pure value -> value functions:
 * HMAC-SHA256 signing (UTF-8), unpadded base64url framing, and the token
 * builders.  ``time.time()`` is an argument here (``now``) instead of a clock
 * read, and nothing here touches a socket, so the wire bytes are reproducible.
 */
#ifndef W24_AUTH_6661F1_H
#define W24_AUTH_6661F1_H

#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _DEFAULT_UPGRADE_TTL_SECONDS (connector makeUpgradeToken default). */
#define W24_AUTH_6661F1_DEFAULT_UPGRADE_TTL 300

/* HMAC-SHA256(secret, payload) as lower-case hex; malloc'd, caller frees.
 * NULL payload/secret are treated as "" (Python would raise AttributeError;
 * the JSON wrappers below return null for that seam instead). */
char *w24_auth_6661f1_sign_hex(const char *payload, const char *secret);

/* Unpadded base64url of ``len`` bytes (Node Buffer.toString("base64url"));
 * malloc'd, caller frees. */
char *w24_auth_6661f1_b64url(const unsigned char *data, size_t len);

/* base64url(f"{payload}:{exp}:{sig}"), exp = now + ttl when ttl > 0 else 0. */
json_t *w24_auth_6661f1_make_token(const char *payload, const char *secret,
                                   long long ttl_seconds, long long now);

/* {"payload","secret"} -> hex digest string; null when either is not a string. */
json_t *w24_auth_6661f1_sign(const json_t *args);

/* {"payload","secret","ttl_seconds","now"} -> token string; null on a
 * non-string payload/secret or a non-numeric ttl (Python raises there). */
json_t *w24_auth_6661f1_token(const json_t *args);

/* {"gateway_id","secret","ttl_seconds"} -> token string (ttl defaults to
 * W24_AUTH_6661F1_DEFAULT_UPGRADE_TTL, exactly like the Python default). */
json_t *w24_auth_6661f1_upgrade_token(const json_t *args);

/* The module's literal table. */
json_t *w24_auth_6661f1_constants(void);

#ifdef __cplusplus
}
#endif
#endif
