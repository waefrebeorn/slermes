/**
 * @file hermes_gateway_mattermost.h
 * @brief Mattermost platform declarations.
 */
#ifndef HERMES_GATEWAY_MATTERMOST_H
#define HERMES_GATEWAY_MATTERMOST_H

#include "hermes_gateway_types.h"
#include "hermes_http.h"

/* ================================================================
 *  Mattermost platform
 * ================================================================ */

void mattermost_set_url(const char *url);
void mattermost_set_token(const char *token);
void mattermost_set_channel(const char *id);
bool mattermost_send_message(http_client_t *http, const char *text);
json_node_t *mattermost_poll_messages(http_client_t *http);
const char *mattermost_get_chat_id(json_node_t *update);
const char *mattermost_get_text(json_node_t *update);

#endif /* HERMES_GATEWAY_MATTERMOST_H */