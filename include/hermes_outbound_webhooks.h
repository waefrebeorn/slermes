#ifndef HERMES_OUTBOUND_WEBHOOKS_H
#define HERMES_OUTBOUND_WEBHOOKS_H

/*
 * hermes_outbound_webhooks.h — Outbound webhook notification port.
 * Port of agent/outbound_webhooks.py.
 * C11, no C++.
 */

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OW_DEFAULT_TIMEOUT_SECONDS  10
#define OW_MAX_TIMEOUT_SECONDS      60
#define OW_MAX_DELIVERY_ATTEMPTS    2
#define OW_RETRY_BACKOFF_SECONDS    1.0
#define OW_QUEUE_MAX_SIZE           256

/* ── WebhookTarget dataclass ──────────────────────────────── */

typedef struct ow_webhook_target_s {
    char   *url;
    char  **events;          /* NULL-terminated string array */
    char   *name;
    char   *secret;          /* optional */
    char   *matcher;         /* optional */
    int     timeout;
    void   *compiled_matcher; /* opaque — not exposed in C */
} ow_webhook_target_t;

/* PoP: WebhookTarget.label @ agent/outbound_webhooks.py:WebhookTarget.label */
const char *ow_target_label(const ow_webhook_target_t *t);

/* PoP: WebhookTarget.matches_tool @ agent/outbound_webhooks.py:WebhookTarget.matches_tool */
bool ow_target_matches_tool(const ow_webhook_target_t *t, const char *tool_name);

/* Constructor/destructor */
ow_webhook_target_t *ow_target_create(const char *url, const char **events,
                                       const char *name, const char *secret,
                                       const char *matcher, int timeout);
void ow_target_free(ow_webhook_target_t *t);

/* ── Config parsing ───────────────────────────────────────── */

/* PoP: _parse_outbound_block @ agent/outbound_webhooks.py:_parse_outbound_block */
ow_webhook_target_t **ow_parse_outbound_block(const json_t *raw);

/* PoP: _parse_single_target @ agent/outbound_webhooks.py:_parse_single_target */
ow_webhook_target_t *ow_parse_single_target(int index, const json_t *raw);

/* PoP: _resolve_secret @ agent/outbound_webhooks.py:_resolve_secret */
char *ow_resolve_secret(int index, const json_t *raw);

/* ── Payload / delivery ──────────────────────────────────── */

/* PoP: _serialize_payload @ agent/outbound_webhooks.py:_serialize_payload */
char *ow_serialize_payload(const char *event, const json_t *kwargs,
                            const char *delivery_id);

/* PoP: _build_delivery @ agent/outbound_webhooks.py:_build_delivery */
json_t *ow_build_delivery(const char *event, const ow_webhook_target_t *target,
                           const char *body, size_t body_len,
                           const char *delivery_id);

/* PoP: _enqueue @ agent/outbound_webhooks.py:_enqueue */
bool ow_enqueue(const json_t *delivery);

/* PoP: _ensure_worker @ agent/outbound_webhooks.py:_ensure_worker */
void ow_ensure_worker(void);

/* PoP: flush @ agent/outbound_webhooks.py:flush */
bool ow_flush(double timeout_seconds);

/* PoP: reset_for_tests @ agent/outbound_webhooks.py:reset_for_tests */
void ow_reset_for_tests(void);

/* ── Public API ───────────────────────────────────────────── */

/* PoP: register_from_config @ agent/outbound_webhooks.py:register_from_config */
ow_webhook_target_t **ow_register_from_config(const json_t *cfg);

/* PoP: iter_configured_targets @ agent/outbound_webhooks.py:iter_configured_targets */
ow_webhook_target_t **ow_iter_configured_targets(const json_t *cfg);

/* PoP: _make_callback @ agent/outbound_webhooks.py:_make_callback */
void *ow_make_callback(const char *event, const ow_webhook_target_t *target);

/* PoP: _deliver @ agent/outbound_webhooks.py:_deliver */
void ow_deliver(const json_t *delivery);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_OUTBOUND_WEBHOOKS_H */