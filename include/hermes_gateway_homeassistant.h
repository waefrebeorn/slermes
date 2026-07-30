/**
 * @file hermes_gateway_homeassistant.h
 * @brief HomeAssistant platform declarations.
 */
#ifndef HERMES_GATEWAY_HOMEASSISTANT_H
#define HERMES_GATEWAY_HOMEASSISTANT_H

#include "hermes_gateway_types.h"
#include "hermes_http.h"

/* ================================================================
 *  HomeAssistant platform
 * ================================================================ */

void ha_set_url(const char *url);
void ha_set_token(const char *token);
void ha_set_notify_entity(const char *entity);
bool ha_send_notification(const char *title, const char *message);
json_node_t *ha_poll_messages(http_client_t *http);
const char *ha_get_chat_id(json_node_t *update);
const char *ha_get_text(json_node_t *update);

#endif /* HERMES_GATEWAY_HOMEASSISTANT_H */