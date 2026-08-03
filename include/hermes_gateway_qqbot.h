/**
 * @file hermes_gateway_qqbot.h
 * @brief QQ Bot platform declarations.
 */
#ifndef HERMES_GATEWAY_QQBOT_H
#define HERMES_GATEWAY_QQBOT_H

#include "hermes_gateway_types.h"
#include "hermes_http.h"

/* ================================================================
 *  P113: QQ Bot platform — full feature parity
 * ================================================================ */

void qqbot_set_webhook(const char *url);
void qqbot_set_token(const char *token);
bool qqbot_is_running(void);
void qqbot_set_running(bool running);
void qqbot_stop(void);
bool qqbot_send_message(http_client_t *http, const char *text);
bool qqbot_send_markdown(http_client_t *http, const char *text);
bool qqbot_send_image(http_client_t *http, const char *image_url);
bool qqbot_send_with_keyboard(http_client_t *http, const char *text,
                               const char *keyboard_json);
bool qqbot_send_with_at(http_client_t *http, const char *text,
                          const char *at_user_id);
void qqbot_queue_message(const char *chat_id, const char *text,
                          const char *sender_id);
void qqbot_handle_webhook(const char *body);
json_node_t *qqbot_poll_messages(http_client_t *http);
bool qqbot_post_api(const char *endpoint, json_node_t *root);
const char *qqbot_get_chat_id(json_node_t *update);
const char *qqbot_get_text(json_node_t *update);

#endif /* HERMES_GATEWAY_QQBOT_H */