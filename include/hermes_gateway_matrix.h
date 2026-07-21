/**
 * @file hermes_gateway_matrix.h
 * @brief Matrix platform declarations.
 */
#ifndef HERMES_GATEWAY_MATRIX_H
#define HERMES_GATEWAY_MATRIX_H

#include "hermes_gateway_types.h"
#include "hermes_http.h"

/* ================================================================
 *  Matrix platform
 * ================================================================ */

void matrix_set_homeserver(const char *hs);
void matrix_set_token(const char *token);
void matrix_set_room(const char *id);
void matrix_set_user_id(const char *uid);
void matrix_set_event_filter(const char *types);
bool matrix_send_message(http_client_t *http, const char *text);
json_node_t *matrix_poll_messages(http_client_t *http);
const char *matrix_get_chat_id(json_node_t *update);
const char *matrix_get_text(json_node_t *update);
json_node_t *matrix_list_rooms(http_client_t *http);
bool matrix_mark_read(http_client_t *http, const char *room_id, const char *event_id);
bool matrix_send_typing(http_client_t *http, const char *room_id, int timeout_ms);

/* E55: Room management */
const char *matrix_create_room(http_client_t *http, const char *name,
                                const char *alias, bool is_public);
bool matrix_join_room(http_client_t *http, const char *room_id_or_alias);
bool matrix_leave_room(http_client_t *http, const char *r_id);

#endif /* HERMES_GATEWAY_MATRIX_H */