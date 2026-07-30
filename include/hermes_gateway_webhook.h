/**
 * @file hermes_gateway_webhook.h
 * @brief Webhook HTTP API platform declarations.
 */
#ifndef HERMES_GATEWAY_WEBHOOK_H
#define HERMES_GATEWAY_WEBHOOK_H

#include "hermes_gateway_types.h"
#include "hermes_http.h"

/* ================================================================
 *  Webhook HTTP API platform
 * ================================================================ */

/* Start HTTP API server on the given port. Blocks until g_gw.running
 * is set to false (SIGINT/SIGTERM). Runs in a thread. */
void webhook_server_run(int port);

/* ================================================================
 *  P112: Webhook subscription + HMAC verification + retry + rate limit
 * ================================================================ */

/* Set HMAC secret for verifying incoming webhook request signatures.
 * Calls with NULL or empty string disable verification. */
void webhook_set_verify_secret(const char *secret);

/* Verify an HMAC-SHA256 signature in "sha256=<hex>" format against body.
 * The signature value typically comes from X-Hub-Signature-256 header.
 * Returns true if signature matches, false otherwise. */
bool webhook_verify_hmac(const char *signature, const unsigned char *body, size_t body_len);

/* Register an outbound webhook subscription.
 * Returns subscription index on success, -1 on failure (table full or NULL args).
 * max_retries=3 and backoff_ms=1000 are reasonable defaults. */
int webhook_subscription_add(const char *endpoint, const char *secret,
                              int max_retries, int backoff_ms);

/* Remove a subscription by index. Returns true if removed. */
bool webhook_subscription_remove(int idx);

/* Add a custom header to an existing subscription.
 * Returns true on success. */
bool webhook_subscription_add_header(int idx, const char *key, const char *value);

/* Deliver a JSON payload to a subscription endpoint.
 * Uses exponential backoff retry with jitter, subject to per-endpoint
 * rate limiting. Returns true if at least one attempt succeeded. */
bool webhook_subscription_deliver(int idx, const char *payload);

/* Deliver payload to endpoint (one-shot, no subscription needed).
 * Useful for simple outgoing webhooks without registering a subscription.
 * No rate limiting or HMAC signing is applied in this mode. */
bool webhook_send_payload(const char *endpoint, const char *payload,
                           const char *custom_headers, int timeout_sec,
                           int max_retries, int backoff_ms);

/* Get count of active subscriptions */
int webhook_subscription_count(void);

/* List active subscriptions into provided array; returns count written. */
int webhook_subscription_list(webhook_subscription_t *out, int max_out);

#endif /* HERMES_GATEWAY_WEBHOOK_H */