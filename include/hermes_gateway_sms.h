/**
 * @file hermes_gateway_sms.h
 * @brief SMS/MMS platform (Twilio) declarations.
 */
#ifndef HERMES_GATEWAY_SMS_H
#define HERMES_GATEWAY_SMS_H

#include "hermes_gateway_types.h"
#include "hermes_http.h"

/* ================================================================
 *  P111: SMS/MMS platform (Twilio) — carrier lookup, MMS, webhooks
 * ================================================================ */

/* Setup */
void sms_set_twilio(const char *sid, const char *token, const char *from);
void sms_set_status_callback(const char *url);
void sms_set_webhook_url(const char *url);

/* Send SMS (basic) */
bool sms_send_message(http_client_t *http, const char *to, const char *text);

/* Send MMS with media URL (image, video, audio) */
bool sms_send_mms(http_client_t *http, const char *to, const char *text,
                  const char *media_url);

/* Carrier lookup via Twilio Lookup API v2.
 * Returns JSON node with carrier info, or NULL on error.
 * Caller must json_free() the result. */
json_node_t *sms_lookup_carrier(http_client_t *http, const char *phone_number);

/* Webhook verification — called from the webhook server on GET /sms-webhook.
 * Twilio uses a simple GET request for number confirmation.
 * Returns the challenge response string, or NULL on failure. */
const char *sms_verify_webhook(const char *query_string);

/* Parse Twilio webhook POST body (form-urlencoded) into json_node_t updates.
 * Handles inbound SMS, MMS, and delivery status callbacks.
 * Returns a JSON array of update objects, or NULL.
 * Each update has: "from", "to", "text", "message_sid", "num_media",
 * "media_urls" (array of strings for MMS), "status" (delivery status),
 * "error_code", "error_message". */
json_node_t *sms_parse_webhook(const char *body);

/* Poll (no-op for SMS — inbound is webhook-driven) */
json_node_t *sms_poll_messages(http_client_t *http);
void sms_queue_message(const char *chat_id, const char *text,
                        const char *sender_id);
void sms_handle_webhook(const char *body);

/* Update extractors */
const char *sms_get_chat_id(json_node_t *update);
const char *sms_get_text(json_node_t *update);
const char *sms_get_message_sid(json_node_t *update);
const char *sms_get_status(json_node_t *update);
const char *sms_get_media_url(json_node_t *update, size_t index);
size_t      sms_get_num_media(json_node_t *update);

#endif /* HERMES_GATEWAY_SMS_H */