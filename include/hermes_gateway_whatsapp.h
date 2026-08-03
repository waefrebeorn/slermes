/**
 * @file hermes_gateway_whatsapp.h
 * @brief WhatsApp Cloud API platform declarations.
 */
#ifndef HERMES_GATEWAY_WHATSAPP_H
#define HERMES_GATEWAY_WHATSAPP_H

#include "hermes_gateway_types.h"
#include "hermes_http.h"

/* ================================================================
 *  WhatsApp Cloud API platform
 * ================================================================ */

void whatsapp_set_token(const char *token);
void whatsapp_set_phone_id(const char *id);
bool whatsapp_cloud_is_active(void);
void whatsapp_cloud_set_active(bool active);
void whatsapp_set_verify_token(const char *token);
bool whatsapp_send_message(http_client_t *http, const char *to,
                            const char *text);
bool whatsapp_send_template(http_client_t *http, const char *to,
                             const char *template_name,
                             const char *language_code,
                             json_node_t *components);
bool whatsapp_send_interactive_buttons(http_client_t *http, const char *to,
                                        const char *header_text,
                                        const char *body_text,
                                        const char *footer_text,
                                        json_node_t *buttons);
bool whatsapp_send_interactive_list(http_client_t *http, const char *to,
                                     const char *body_text,
                                     const char *button_text,
                                     const char *section_title,
                                     json_node_t *rows);
bool whatsapp_mark_read(http_client_t *http, const char *message_id);
const char *whatsapp_verify_webhook(const char *query_string);
json_node_t *whatsapp_parse_webhook(const char *body);
const char *whatsapp_get_chat_id(json_node_t *update);
const char *whatsapp_get_text(json_node_t *update);

#endif /* HERMES_GATEWAY_WHATSAPP_H */