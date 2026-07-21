/**
 * @file hermes_gateway_wecom.h
 * @brief WeCom (WeChat Work) platform declarations.
 */
#ifndef HERMES_GATEWAY_WECOM_H
#define HERMES_GATEWAY_WECOM_H

#include "hermes_gateway_types.h"
#include "hermes_http.h"

/* ================================================================
 *  P113: WeCom (WeChat Work) platform — full feature parity
 * ================================================================ */

void wecom_set_webhook(const char *url);
void wecom_set_app_credentials(const char *corp_id, const char *corp_secret,
                                const char *agent_id);
bool wecom_send_message(http_client_t *http, const char *text);
bool wecom_send_markdown(http_client_t *http, const char *text);
bool wecom_send_text_with_at(http_client_t *http, const char *text,
                              const char *mentioned_list_json,
                              const char *mentioned_mobile_list_json);
bool wecom_send_image(http_client_t *http, const char *base64_data,
                       const char *md5_hex);
bool wecom_send_file(http_client_t *http, const char *media_id);
bool wecom_send_news(http_client_t *http, const char *articles_json);
bool wecom_send_taskcard(http_client_t *http,
                          const char *title, const char *description,
                          const char *url, const char *task_id,
                          const char *btns_json);
void wecom_queue_message(const char *chat_id, const char *text,
                          const char *sender_id);
void wecom_handle_webhook(const char *body);
json_node_t *wecom_poll_messages(http_client_t *http);
const char *wecom_get_chat_id(json_node_t *update);
const char *wecom_get_text(json_node_t *update);

#endif /* HERMES_GATEWAY_WECOM_H */