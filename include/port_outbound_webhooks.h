/*
 * port_outbound_webhooks.h — pure delivery-shape helpers from
 * agent/outbound_webhooks.py. The profile/timestamp reads and the queue/
 * worker IO stay unported: these take those facts as parameters.
 */
#ifndef PORT_OUTBOUND_WEBHOOKS_H
#define PORT_OUTBOUND_WEBHOOKS_H

/* _serialize_payload: build the POST body JSON:
 * {"hook_event_name","profile", ...extra, "delivery_id","timestamp"}.
 * extra_json is the _payload_fields(kwargs) object (may be NULL/empty obj).
 * Returns malloc'd JSON string (the encoded body); caller frees. */
char *ow_serialize_payload(const char *event, const char *profile,
                           const char *delivery_id, const char *timestamp,
                           const char *extra_json);

/* _build_delivery: the delivery dict "url","label","event","body","headers",
 * "timeout". Headers always carry Content-Type/User-Agent/X-Hermes-Event/
 * X-Hermes-Delivery; a non-empty secret adds X-Hermes-Signature-256 =
 * "sha256=<hex HMAC-SHA256(secret, body)>". Returns malloc'd JSON, caller frees. */
char *ow_build_delivery_json(const char *event, const char *url,
                             const char *label, const char *secret, int timeout,
                             const char *body, const char *delivery_id);

#endif /* PORT_OUTBOUND_WEBHOOKS_H */
