/**
 * @file hermes_gateway_discord.h
 * @brief Discord platform declarations.
 */
#ifndef HERMES_GATEWAY_DISCORD_H
#define HERMES_GATEWAY_DISCORD_H

#include "hermes_gateway_types.h"
#include "hermes_http.h"

/* ================================================================
 *  Discord platform
 * ================================================================ */

void discord_set_token(const char *token);
void discord_set_channel(const char *id);
void discord_set_application_id(const char *id);
void discord_add_channel(const char *id);
bool discord_send_message(http_client_t *http, const char *text);
bool discord_send_message_to(http_client_t *http, const char *channel,
                              const char *text);
bool discord_send_embed(http_client_t *http, const char *channel,
                         const char *title, const char *description,
                         const char *color_hex);
void discord_send_typing(http_client_t *http);
void discord_send_typing_to(http_client_t *http, const char *channel);
bool discord_start_thread(http_client_t *http, const char *channel,
                           const char *message_id, const char *name,
                           int auto_archive_duration);
bool discord_join_thread(http_client_t *http, const char *thread_id);
bool discord_register_slash_command(http_client_t *http,
                                     const char *name,
                                     const char *description,
                                     json_node_t *options);
bool discord_bulk_overwrite_commands(http_client_t *http,
                                      json_node_t *commands);
bool discord_create_interaction_response(http_client_t *http,
                                          const char *interaction_id,
                                          const char *interaction_token,
                                          json_node_t *response_data);
bool discord_edit_interaction_response(http_client_t *http,
                                        const char *application_id_str,
                                        const char *interaction_token,
                                        json_node_t *response_data);
json_node_t *discord_poll_messages(http_client_t *http);
const char *discord_get_chat_id(json_node_t *update);
const char *discord_get_text(json_node_t *update);

/* ================================================================
 *  E48: Interaction handling — get interaction metadata from update
 * ================================================================ */
int discord_get_interaction_type(json_node_t *update);
const char *discord_get_interaction_id(json_node_t *update);
const char *discord_get_interaction_token(json_node_t *update);
bool discord_defer_interaction(http_client_t *http,
                                const char *interaction_id,
                                const char *interaction_token);
bool discord_show_modal(http_client_t *http,
                         const char *interaction_id,
                         const char *interaction_token,
                         const char *custom_id,
                         const char *title,
                         json_node_t *components);

/* ================================================================
 *  E52: Typing with graceful 429 handling
 * ================================================================ */
bool discord_send_typing_graceful(http_client_t *http, const char *channel);

#endif /* HERMES_GATEWAY_DISCORD_H */