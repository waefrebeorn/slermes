/* gateway/msgraph_webhook_security.h — C11 port of gateway/platforms/msgraph_webhook.py
 *
 * Self-contained MS Graph webhook security + data-path helpers:
 * source-IP allowlisting (CIDR), constant-time clientState verification,
 * resource accept filtering, {key} template rendering, seen-receipt
 * idempotency, and the validation/notification handlers.
 *
 * Opaque adapter struct; callers in msgraph_webhook.c (the HTTP daemon) use
 * only this public surface. No god headers — depends only on hermes_json.h
 * + libcrypto/crypto.h.
 */

#ifndef HERMES_GATEWAY_MSGRAPH_WEBHOOK_SECURITY_H
#define HERMES_GATEWAY_MSGRAPH_WEBHOOK_SECURITY_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A single CIDR allowlist entry (IPv4 or IPv6). */
typedef struct {
    bool  is_v6;
    unsigned char addr[16];   /* network address (4 or 16 bytes significant) */
    unsigned char mask[16];   /* network mask (1 bits in the prefix) */
    int   bits;               /* prefix length (32 or 128) */
} msgraph_cidr_t;

/* Adapter configuration (mirrors MSGraphWebhookAdapter.__init__). */
typedef struct {
    const char *host;                  /* bind host; NULL => "0.0.0.0" */
    const char *client_state;          /* expected clientState (constant-time cmp) */
    const char *allowed_source_cidrs;  /* newline/comma/JSON-list of CIDRs */
    const char *accepted_resources;    /* newline/comma-separated prefixes/wildcards */
    const char *prompt_template;       /* optional {key} template for the prompt */
    int         max_seen_receipts;    /* dedup ring-buffer cap (0 => 5000) */
} msgraph_webhook_config_t;

/* Scheduler callback invoked per accepted notification (schedule_notification). */
typedef void (*msgraph_notification_scheduler_fn)(const char *notification_json,
                                                  const char *event_json,
                                                  void *user);

typedef struct msgraph_webhook_adapter msgraph_webhook_adapter_t;

/* Construct/destroy the adapter. */
msgraph_webhook_adapter_t *msgraph_webhook_create(const msgraph_webhook_config_t *cfg);
void msgraph_webhook_destroy(msgraph_webhook_adapter_t *a);

/* PoP: msgraph_normalize_resource_value @ gateway/platforms/msgraph_webhook.py:_normalize_resource_value */
char *msgraph_normalize_resource_value(const char *resource);

/* PoP: msgraph_parse_allowed_source_cidrs @ gateway/platforms/msgraph_webhook.py:_parse_allowed_source_cidrs */
int msgraph_parse_allowed_source_cidrs(const char *raw, msgraph_cidr_t *out, int max_out);

/* PoP: msgraph_resource_accepted @ gateway/platforms/msgraph_webhook.py:_resource_accepted */
bool msgraph_resource_accepted(const char *resource, const char *accepted_patterns);

/* PoP: msgraph_verify_client_state @ gateway/platforms/msgraph_webhook.py:_verify_client_state */
bool msgraph_verify_client_state(const char *provided, const char *expected);

/* PoP: msgraph_render_template @ gateway/platforms/msgraph_webhook.py:_render_template */
char *msgraph_render_template(const char *template_str, const char *payload_json);

/* PoP: msgraph_render_prompt @ gateway/platforms/msgraph_webhook.py:_render_prompt */
char *msgraph_render_prompt(const char *notification_json, const char *prompt_template);

/* PoP: msgraph_source_allowlist_required_but_missing @ gateway/platforms/msgraph_webhook.py:_source_allowlist_required_but_missing */
bool msgraph_source_allowlist_required_but_missing(const char *host,
                                                   const msgraph_cidr_t *cidrs,
                                                   int n_cidrs);

/* PoP: msgraph_source_ip_allowed @ gateway/platforms/msgraph_webhook.py:_source_ip_allowed */
bool msgraph_source_ip_allowed(const char *peer_ip, const char *host,
                               const msgraph_cidr_t *cidrs, int n_cidrs);

/* PoP: set_notification_scheduler @ gateway/platforms/msgraph_webhook.py:set_notification_scheduler */
void msgraph_webhook_set_scheduler(msgraph_webhook_adapter_t *a,
                                   msgraph_notification_scheduler_fn fn, void *user);

/* PoP: msgraph_has_seen_receipt @ gateway/platforms/msgraph_webhook.py:_has_seen_receipt */
bool msgraph_has_seen_receipt(msgraph_webhook_adapter_t *a, const char *receipt_key);

/* PoP: msgraph_remember_receipt @ gateway/platforms/msgraph_webhook.py:_remember_receipt */
void msgraph_remember_receipt(msgraph_webhook_adapter_t *a, const char *receipt_key);

/* PoP: msgraph_build_message_event @ gateway/platforms/msgraph_webhook.py:_build_message_event */
char *msgraph_build_message_event(const char *notification_json, const char *receipt_key);

/* PoP: msgraph_schedule_notification @ gateway/platforms/msgraph_webhook.py:_schedule_notification */
void msgraph_schedule_notification(msgraph_webhook_adapter_t *a,
                                   const char *notification_json,
                                   const char *event_json);

/* PoP: msgraph_handle_validation @ gateway/platforms/msgraph_webhook.py:_handle_validation */
int msgraph_handle_validation(const char *validation_token, char **out_body);

/* PoP: msgraph_handle_notification @ gateway/platforms/msgraph_webhook.py:_handle_notification */
int msgraph_handle_notification(msgraph_webhook_adapter_t *a,
                                const char *body_json, size_t body_len,
                                int *out_scheduled);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_GATEWAY_MSGRAPH_WEBHOOK_SECURITY_H */
