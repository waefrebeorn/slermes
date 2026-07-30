/**
 * @file hermes_gateway_slack.h
 * @brief Slack platform declarations.
 */
#ifndef HERMES_GATEWAY_SLACK_H
#define HERMES_GATEWAY_SLACK_H

#include "hermes_gateway_types.h"
#include "hermes_http.h"

/* ================================================================
 *  Slack platform
 * ================================================================ */

void slack_set_token(const char *token);
void slack_set_channel(const char *id);
void slack_set_signing_secret(const char *secret);
bool slack_send_message(http_client_t *http, const char *text);
bool slack_send_blocks(http_client_t *http, const char *channel,
                        const char *text, json_node_t *blocks);
bool slack_update_message(http_client_t *http, const char *channel,
                           const char *ts, const char *text);
bool slack_join_channel(http_client_t *http, const char *channel);
bool slack_leave_channel(http_client_t *http, const char *channel);
bool slack_upload_file(http_client_t *http, const char *file_path,
                       const char *filename, const char *channel);
json_node_t *slack_poll_messages(http_client_t *http);
const char *slack_get_chat_id(json_node_t *update);
const char *slack_get_text(json_node_t *update);

#endif /* HERMES_GATEWAY_SLACK_H */