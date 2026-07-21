/**
 * @file hermes_gateway_feishu.h
 * @brief Feishu (Lark) platform declarations.
 */
#ifndef HERMES_GATEWAY_FEISHU_H
#define HERMES_GATEWAY_FEISHU_H

#include "hermes_gateway_types.h"
#include "hermes_http.h"

/* ================================================================
 *  P114: Feishu (Lark) platform — card messages, buttons, docs, media
 * ================================================================ */

/* ---- Basic config ---- */
void feishu_set_webhook(const char *url);
bool feishu_send_message(http_client_t *http, const char *text);
bool feishu_send_image_webhook(http_client_t *http, const char *image_path);

/* ---- App API mode (Open API with tenant access token) ---- */
void feishu_set_app_credentials(const char *app_id, const char *app_secret);
void feishu_set_default_receive_id(const char *receive_id);

/* ---- Card messages ---- */

/* Send interactive card via webhook (no app credentials needed).
 * card: JSON node with Feishu card structure (config/header/elements). */
bool feishu_send_interactive(http_client_t *http, json_node_t *card);

/* Send interactive card to a specific receive_id via Open API.
 * card: JSON node with Feishu card structure. receive_id is auto-escaped as JSON string. */
bool feishu_send_card(http_client_t *http, const char *receive_id,
                       json_node_t *card);

/* Convenience: send a card with interactive buttons.
 * buttons: JSON array of {"text":{...},"type":"default","value":{...}} elements.
 * template: card header color template ("blue","green","red","purple","yellow", etc.) */
bool feishu_send_card_with_buttons(http_client_t *http,
                                    const char *receive_id,
                                    const char *title,
                                    const char *body_text,
                                    json_node_t *buttons,
                                    const char *template);

/* ---- Doc integration (Open API) ---- */

/* Create a new doc in the given folder (pass NULL for root).
 * Returns malloc'd JSON string with document info (caller free()s), or NULL on error.
 * Result JSON contains: document_id, title, url. */
char *feishu_doc_create(http_client_t *http, const char *folder_token,
                         const char *title);

/* Get doc raw content as plain text / markdown.
 * Returns malloc'd string (caller free()s), or NULL on error. */
char *feishu_doc_get_raw_content(http_client_t *http, const char *doc_id);

/* ---- Image/File messages (Open API) ---- */

/* Upload an image. Returns malloc'd image_key string (caller free()s), or NULL.
 * image_path: absolute path to the image file on disk. */
char *feishu_upload_image(http_client_t *http, const char *image_path);

/* Send an image by image_key to a specific receive_id. */
bool feishu_send_image(http_client_t *http, const char *receive_id,
                        const char *image_key);

/* Upload a file. Returns malloc'd file_key string (caller free()s), or NULL.
 * file_path: absolute path to the file. file_name: display name in chat. */
char *feishu_upload_file(http_client_t *http, const char *file_path,
                          const char *file_name);

/* Send a file by file_key to a specific receive_id. */
bool feishu_send_file(http_client_t *http, const char *receive_id,
                       const char *file_key);

/* ---- Poll / update extractors (unchanged) ---- */
json_node_t *feishu_poll_messages(http_client_t *http);
const char *feishu_get_chat_id(json_node_t *update);
const char *feishu_get_text(json_node_t *update);

#endif /* HERMES_GATEWAY_FEISHU_H */