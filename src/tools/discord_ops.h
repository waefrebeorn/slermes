/*
 * discord_ops.h - Discord guild/channel/member operations.
 *
 * Module-internal header (discord.c <-> discord_ops.c).
 */
#ifndef DISCORD_OPS_H
#define DISCORD_OPS_H

#include <stddef.h>
#include <stdbool.h>
#include "discord_http.h"   /* discord_api_call, json_error */

#define DISCORD_API_BASE "https://discord.com/api/v10"

char * add_role(const char *guild_id, const char *user_id, const char *role_id);
char * ban_member(const char *guild_id, const char *user_id, const char *reason);
char * create_channel(const char *guild_id, const char *name, int type);
char * create_thread(const char *channel_id, const char *name, const char *message_id);
char * delete_message(const char *channel_id, const char *message_id);
char * kick_member(const char *guild_id, const char *user_id);
char * pin_message(const char *channel_id, const char *message_id);
char * remove_role(const char *guild_id, const char *user_id, const char *role_id);
char * search_members(const char *guild_id, const char *query, int limit);
char * send_message(const char *channel_id, const char *content);
char * unpin_message(const char *channel_id, const char *message_id);

#endif /* DISCORD_OPS_H */
