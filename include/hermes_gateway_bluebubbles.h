/**
 * @file hermes_gateway_bluebubbles.h
 * @brief BlueBubbles (iMessage) platform declarations.
 */
#ifndef HERMES_GATEWAY_BLUEBUBBLES_H
#define HERMES_GATEWAY_BLUEBUBBLES_H

#include "hermes_gateway_types.h"
#include "hermes_http.h"

/* ================================================================
 *  BlueBubbles (iMessage) platform
 * ================================================================ */

void bluebubbles_set_url(const char *url);
void bluebubbles_set_password(const char *password);
bool bluebubbles_send_message(http_client_t *http, const char *to,
                              const char *text);
json_node_t *bluebubbles_poll_messages(http_client_t *http);
const char *bluebubbles_get_chat_id(json_node_t *update);
const char *bluebubbles_get_text(json_node_t *update);

/* ================================================================
 *  P115: Extended BlueBubbles API — tapbacks, attachments, group chat
 * ================================================================ */

/* Tapback reaction codes (associatedMessageType values) */
#define BLUEBUBBLES_TAPBACK_LOVE      2000
#define BLUEBUBBLES_TAPBACK_LIKE      2001
#define BLUEBUBBLES_TAPBACK_DISLIKE   2002
#define BLUEBUBBLES_TAPBACK_LAUGH     2003
#define BLUEBUBBLES_TAPBACK_EMPHASIZE 2004
#define BLUEBUBBLES_TAPBACK_QUESTION  2005

/* Send a tapback reaction to a specific message.
 * chat_id: BlueBubbles chat GUID.
 * message_guid: the GUID of the message to react to.
 * tapback_type: one of the BLUEBUBBLES_TAPBACK_* constants. */
bool bluebubbles_send_tapback(http_client_t *http,
                               const char *chat_id,
                               const char *message_guid,
                               int tapback_type);

/* Send a file attachment (image, video, document) to a chat.
 * Uses curl subprocess (libhttp does not support multipart).
 * file_path: absolute path to the file on disk.
 * filename: display name for the attachment (or NULL to use basename). */
bool bluebubbles_send_attachment(http_client_t *http,
                                  const char *chat_id,
                                  const char *file_path,
                                  const char *filename);

/* Group chat detection */
bool bluebubbles_is_group(json_node_t *update);
const char *bluebubbles_get_group_id(json_node_t *update);

/* Typing indicator */
bool bluebubbles_send_typing(http_client_t *http, const char *chat_id);
bool bluebubbles_stop_typing(http_client_t *http, const char *chat_id);

/* P115: Extended update parsers */
const char *bluebubbles_get_attachment_path(json_node_t *update);
int bluebubbles_get_tapback_type(json_node_t *update);
const char *bluebubbles_get_message_guid(json_node_t *update);

/* Set a specific chat GUID to poll for recent messages.
 * Without this, bluebubbles_poll_messages() returns NULL (webhook-driven). */
void bluebubbles_set_poll_guid(const char *guid);

#endif /* HERMES_GATEWAY_BLUEBUBBLES_H */