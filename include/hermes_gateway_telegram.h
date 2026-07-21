/**
 * @file hermes_gateway_telegram.h
 * @brief Telegram platform declarations.
 */
#ifndef HERMES_GATEWAY_TELEGRAM_H
#define HERMES_GATEWAY_TELEGRAM_H

#include "hermes_gateway_types.h"
#include "hermes_http.h"

/* ================================================================
 *  Telegram platform
 * ================================================================ */

void telegram_set_token(const char *token);
void telegram_set_username(const char *username);
const char *telegram_get_username(void);
bool telegram_get_me(http_client_t *http);
bool telegram_is_mentioned(json_node_t *update);
bool telegram_is_group(json_node_t *update);

/* Telegram sendMessage wrapper. Supports parse_mode, thread_id, disable_notification, disable_preview. */
bool telegram_send_message(http_client_t *http, const char *chat_id,
                            const char *text, const char *parse_mode,
                            const char *thread_id, bool disable_notification, bool disable_preview,
                            const char *reply_to_message_id);
bool telegram_send_chat_action(http_client_t *http, const char *chat_id,
                                const char *action);
json_node_t *telegram_get_updates(http_client_t *http, int offset, int timeout);
const char *telegram_get_chat_id(json_node_t *update);
const char *telegram_get_text(json_node_t *update);
const char *telegram_get_chat_type(json_node_t *update);
const char *telegram_get_chat_name(json_node_t *update);
const char *telegram_get_user_id(json_node_t *update);
const char *telegram_get_user_name(json_node_t *update);
bool telegram_is_bot(json_node_t *update);

/* ================================================================
 *  P104: Extended Telegram API
 * ================================================================ */

/* Keyboard/reply markup messages */
bool telegram_send_message_with_keyboard(http_client_t *http,
                                          const char *chat_id,
                                          const char *text,
                                          const char *parse_mode,
                                          const char *thread_id,
                                          json_node_t *reply_markup,
                                          bool disable_notification, bool disable_preview,
                                          const char *reply_to_message_id);

/* Edit/delete */
bool telegram_edit_message_text(http_client_t *http, const char *chat_id,
                                 const char *message_id, const char *text,
                                 const char *parse_mode);
bool telegram_delete_message(http_client_t *http, const char *chat_id,
                              const char *message_id);

/* Interactive queries */
bool telegram_answer_inline_query(http_client_t *http,
                                   const char *inline_query_id,
                                   json_node_t *results);
bool telegram_answer_callback_query(http_client_t *http,
                                     const char *callback_query_id,
                                     const char *text, bool show_alert);

/* Polls */
bool telegram_send_poll(http_client_t *http, const char *chat_id,
                         const char *question, json_node_t *options,
                         bool is_anonymous, const char *poll_type,
                         bool is_closed);

/* Media */
bool telegram_send_media_group(http_client_t *http, const char *chat_id,
                                json_node_t *media);

/* E01-E05: Media send methods */
bool telegram_send_photo(http_client_t *http, const char *chat_id,
                          const char *photo, const char *caption,
                          const char *parse_mode);
bool telegram_send_document(http_client_t *http, const char *chat_id,
                             const char *document, const char *caption,
                             const char *parse_mode);
bool telegram_send_voice(http_client_t *http, const char *chat_id,
                          const char *voice, const char *caption,
                          const char *parse_mode);
bool telegram_send_video(http_client_t *http, const char *chat_id,
                          const char *video, const char *caption,
                          const char *parse_mode);
bool telegram_send_animation(http_client_t *http, const char *chat_id,
                              const char *animation, const char *caption,
                              const char *parse_mode);

/* E14: Forward message */
bool telegram_forward_message(http_client_t *http, const char *chat_id,
                               const char *from_chat_id,
                               const char *message_id);

/* E15: Pin/unpin message */
bool telegram_pin_chat_message(http_client_t *http, const char *chat_id,
                                const char *message_id);
bool telegram_unpin_chat_message(http_client_t *http, const char *chat_id,
                                  const char *message_id);

/* E16: Message reactions */
bool telegram_set_message_reaction(http_client_t *http, const char *chat_id,
                                    const char *message_id, const char *emoji);

/* Forum topics */
bool telegram_create_forum_topic(http_client_t *http, const char *chat_id,
                                  const char *name);
bool telegram_edit_forum_topic(http_client_t *http, const char *chat_id,
                                const char *message_thread_id, const char *name);
bool telegram_close_forum_topic(http_client_t *http, const char *chat_id,
                                 const char *message_thread_id);
bool telegram_reopen_forum_topic(http_client_t *http, const char *chat_id,
                                  const char *message_thread_id);

/* Update parsers */
const char *telegram_get_update_type(json_node_t *update);
const char *telegram_get_callback_query_id(json_node_t *update);
const char *telegram_get_inline_query_id(json_node_t *update);
const char *telegram_get_message_thread_id(json_node_t *update);

/* Port of Python TelegramAdapter._message_thread_id_for_send().
 * Maps thread_id "1" (General topic) to NULL for sendMessage. */
const char *telegram_message_thread_id_for_send(const char *thread_id);

/* Port of Python telegram_network.parse_fallback_ip_env() +
 * _normalize_fallback_ips(). Parses comma-separated IPv4 addresses,
 * filtering private/loopback/link-local/unspecified. Returns malloc'd
 * array; caller must free each string and the array. */
char **telegram_parse_fallback_ips(const char *env_value, size_t *count);

/* Port of Python send_message_tool._is_telegram_thread_not_found().
 * Returns true if the error text suggests a Telegram thread-not-found failure. */
bool telegram_is_thread_not_found(const char *error_text);

/* E07-E12: Interactive Telegram send methods with inline keyboards */
bool telegram_send_draft(http_client_t *http, const char *chat_id,
                          const char *text, const char *parse_mode);
bool telegram_send_clarify(http_client_t *http, const char *chat_id,
                            const char *question, const char **options, int n_options,
                            const char *parse_mode);
bool telegram_send_approval_prompt(http_client_t *http, const char *chat_id,
                                    const char *command, const char *reason,
                                    const char *parse_mode);
bool telegram_send_confirm_prompt(http_client_t *http, const char *chat_id,
                                   const char *action, const char *detail,
                                   const char *parse_mode);
bool telegram_send_model_picker(http_client_t *http, const char *chat_id,
                                 const char **models, int n_models,
                                 const char *current_model);
bool telegram_send_update_prompt(http_client_t *http, const char *chat_id,
                                  const char *diff_text, const char *summary,
                                  const char *parse_mode);

#endif /* HERMES_GATEWAY_TELEGRAM_H */