/**
 * @file hermes_gateway_signal.h
 * @brief Signal platform declarations (including P108 extended API).
 */
#ifndef HERMES_GATEWAY_SIGNAL_H
#define HERMES_GATEWAY_SIGNAL_H

#include "hermes_gateway_types.h"
#include "hermes_http.h"

/* ================================================================
 *  Signal platform
 * ================================================================ */

void signal_set_number(const char *number);
void signal_set_cli_path(const char *path);
bool signal_check_available(void);
bool signal_is_running(void);
bool signal_connect(void);
void signal_disconnect(void);
bool signal_send_message(http_client_t *http, const char *to,
                          const char *text);
json_node_t *signal_poll_messages(http_client_t *http);
const char *signal_get_chat_id(json_node_t *update);
const char *signal_get_text(json_node_t *update);

/* ================================================================
 *  P108: Extended Signal API — groups, reactions, quotes, attachments
 * ================================================================ */

/* Send a message to a Signal group (uses -g group_id flag) */
bool signal_send_group_message(http_client_t *http,
                                const char *group_id,
                                const char *text);

/* Send an emoji reaction to a specific message.
 * target_author: the phone number of the original message sender.
 * target_timestamp: the server timestamp of the original message (string). */
bool signal_send_reaction(http_client_t *http,
                           const char *recipient,
                           const char *target_author,
                           const char *target_timestamp,
                           const char *emoji);

/* Send a quoted reply to a specific message.
 * quote_author: the phone number of the original message sender.
 * quote_timestamp: the server timestamp of the original message (string). */
bool signal_send_quote_reply(http_client_t *http,
                              const char *recipient,
                              const char *text,
                              const char *quote_author,
                              const char *quote_timestamp);

/* Send a message with a file/image attachment.
 * file_path: absolute path to the file on disk. */
bool signal_send_attachment(http_client_t *http,
                             const char *recipient,
                             const char *text,
                             const char *file_path);

/* Extended poll: also populates "group_id" on group messages,
 * "reaction" on reaction updates, and "attachment" paths. */
const char *signal_get_group_id(json_node_t *update);
const char *signal_get_reaction(json_node_t *update);
const char *signal_get_attachment(json_node_t *update);

/* G08: Check if a signal-cli error message indicates a rate-limit failure.
 * Ported from Python signal_rate_limit._is_signal_rate_limit_error(). */
bool signal_is_rate_limit_error(const char *error_message);

/* G08: Compute HTTP timeout for Signal send RPC based on attachment count.
 * Ported from Python signal_rate_limit._signal_send_timeout(). */
int signal_send_timeout(int num_attachments);

/* G08: Extract per-token Retry-After seconds from a signal-cli rate-limit
 * error JSON string. Returns -1.0 when not found.
 * Ported from Python signal_rate_limit._extract_retry_after_seconds(). */
double signal_extract_retry_after(const char *error_json);

/* G08: Parse "Retry after N seconds" from a plain text message.
 * Returns seconds on success, -1.0 when not found. */
double signal_parse_retry_after_message(const char *msg);

#endif /* HERMES_GATEWAY_SIGNAL_H */