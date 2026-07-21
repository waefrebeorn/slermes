/**
 * @file hermes_gateway_dingtalk.h
 * @brief DingTalk platform declarations.
 */
#ifndef HERMES_GATEWAY_DINGTALK_H
#define HERMES_GATEWAY_DINGTALK_H

#include "hermes_gateway_types.h"
#include "hermes_http.h"

/* ================================================================
 *  P113: DingTalk platform — full feature parity
 * ================================================================ */

void dingtalk_set_webhook(const char *url);
void dingtalk_set_app_credentials(const char *app_id, const char *app_secret);
bool dingtalk_send_message(http_client_t *http, const char *text);
bool dingtalk_send_markdown(http_client_t *http, const char *title, const char *text);
bool dingtalk_send_text_with_at(http_client_t *http, const char *text,
                                 const char *at_mobiles_json,
                                 const char *at_user_ids_json,
                                 bool is_at_all);
bool dingtalk_send_markdown_with_at(http_client_t *http, const char *title,
                                     const char *text,
                                     const char *at_mobiles_json,
                                     const char *at_user_ids_json,
                                     bool is_at_all);
bool dingtalk_send_action_card(http_client_t *http,
                                const char *title, const char *text,
                                const char *btns_json,
                                const char *btn_orientation);
bool dingtalk_send_link(http_client_t *http,
                         const char *title, const char *text,
                         const char *message_url, const char *pic_url);
bool dingtalk_send_image_by_url(http_client_t *http, const char *image_url);
void dingtalk_queue_message(const char *chat_id, const char *text,
                             const char *sender_id);
void dingtalk_handle_webhook(const char *body);
json_node_t *dingtalk_poll_messages(http_client_t *http);
const char *dingtalk_get_chat_id(json_node_t *update);
const char *dingtalk_get_text(json_node_t *update);

/* Get a valid DingTalk Open API access token, refreshing if needed.
 * Returns NULL if credentials are not configured.
 * Uses http_client_t for the refresh request (may be NULL for auto-create). */
const char *dingtalk_get_access_token(http_client_t *http);

#endif /* HERMES_GATEWAY_DINGTALK_H */