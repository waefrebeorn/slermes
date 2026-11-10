/*
 * discord_ops.c - Discord guild/channel/member operations.
 *
 * Split from discord.c (soul.md: self-contained modules). Each function is
 * one REST mutation. No file-scope state.
 */
#include "discord_ops.h"
#include "hermes_core_types.h"
#include "hermes_agent.h"
#include "hermes_json.h"
#include "hermes_tool_helpers.h"
#include "hermes_tool_config.h"
#include "registry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>


/* Port of Python gateway/platforms/weixin.py:_send_message(). */ char *send_message(const char *channel_id, const char *content) {
    char url[512];
    snprintf(url, sizeof(url), "%s/channels/%s/messages", DISCORD_API_BASE, channel_id);

    /* Build JSON body */
    json_t *body = json_object();
    json_set(body, "content", json_string(content ? content : ""));
    char *body_str = json_serialize(body);
    json_free(body);

    tool_api_result_t *r = discord_api_call(url, "POST", body_str, 15, 2);
    free(body_str);

    if (!tool_api_success(r)) {
        char *err = json_errorf("Send message failed: %s",
                        r->error ? r->error : "unknown");
        tool_api_result_free(r);
        return err;
    }

    /* Return message id + timestamp */
    json_t *result = json_object();
    if (r->json) {
        json_set(result, "id", json_copy(json_obj_get(r->json, "id")));
        json_set(result, "channel_id", json_string(channel_id));
        json_set(result, "timestamp", json_copy(json_obj_get(r->json, "timestamp")));
    }
    char *s = json_serialize(result);
    json_free(result);
    tool_api_result_free(r);
    return s;
}


/* Port of Python tools/discord_tool.py:_delete_message(). */
/* Port of Python gateway/platforms/base.py:delete_message(). */ char *delete_message(const char *channel_id, const char *message_id) {
    char url[512];
    snprintf(url, sizeof(url), "%s/channels/%s/messages/%s", DISCORD_API_BASE, channel_id, message_id);
    tool_api_result_t *r = discord_api_call(url, "DELETE", NULL, 15, 2);
    if (!tool_api_success(r)) {
        char *err = json_errorf("Delete failed: %s", r->error ? r->error : "unknown");
        tool_api_result_free(r); return err; }
    json_t *result = json_object();
    json_set(result, "success", json_bool(true));
    char msg[128];
    snprintf(msg, sizeof(msg), "Message %s deleted", message_id);
    json_set(result, "message", json_string(msg));
    char *s = json_serialize(result); json_free(result); tool_api_result_free(r);
    return s;
}


/* ================================================================
 *  Admin actions (P42)
 * ================================================================ */ char *create_channel(const char *guild_id, const char *name, int type) {
    char url[512];
    snprintf(url, sizeof(url), "%s/guilds/%s/channels", DISCORD_API_BASE, guild_id);
    json_t *body = json_object();
    json_set(body, "name", json_string(name));
    json_set(body, "type", json_number((double)type));
    char *body_str = json_serialize(body); json_free(body);
    tool_api_result_t *r = discord_api_call(url, "POST", body_str, 15, 2);
    free(body_str);
    if (!tool_api_success(r)) {
        char *err = json_errorf("Create channel failed: %s", r->error ? r->error : "unknown");
        tool_api_result_free(r); return err; }
    json_t *result = json_object();
    if (r->json) {
        json_set(result, "id", json_copy(json_obj_get(r->json, "id")));
        json_set(result, "name", json_copy(json_obj_get(r->json, "name")));
    }
    char *s = json_serialize(result); json_free(result); tool_api_result_free(r);
    return s;
}
 char *kick_member(const char *guild_id, const char *user_id) {
    char url[512];
    snprintf(url, sizeof(url), "%s/guilds/%s/members/%s", DISCORD_API_BASE, guild_id, user_id);
    tool_api_result_t *r = discord_api_call(url, "DELETE", NULL, 15, 2);
    if (!tool_api_success(r)) {
        char *err = json_errorf("Kick failed: %s", r->error ? r->error : "unknown");
        tool_api_result_free(r); return err; }
    json_t *result = json_object();
    json_set(result, "success", json_bool(true));
    char msg1[128];
    snprintf(msg1, sizeof(msg1), "User %s kicked from guild %s", user_id, guild_id);
    json_set(result, "message", json_string(msg1));
    char *s = json_serialize(result); json_free(result); tool_api_result_free(r);
    return s;
}
 char *ban_member(const char *guild_id, const char *user_id, const char *reason) {
    char url[512];
    snprintf(url, sizeof(url), "%s/guilds/%s/bans/%s", DISCORD_API_BASE, guild_id, user_id);
    json_t *body = json_object();
    if (reason) json_set(body, "reason", json_string(reason));
    char *body_str = json_serialize(body); json_free(body);
    tool_api_result_t *r = discord_api_call(url, "PUT", body_str, 15, 2);
    free(body_str);
    if (!tool_api_success(r)) {
        char *err = json_errorf("Ban failed: %s", r->error ? r->error : "unknown");
        tool_api_result_free(r); return err; }
    json_t *result = json_object();
    json_set(result, "success", json_bool(true));
    char msg2[128];
    snprintf(msg2, sizeof(msg2), "User %s banned from guild %s", user_id, guild_id);
    json_set(result, "message", json_string(msg2));
    char *s = json_serialize(result); json_free(result); tool_api_result_free(r);
    return s;
}


/* Port of Python tools/discord_tool.py:_search_members(). */
/* ================================================================
 *  Message actions (P43 — depth expansion)
 * ================================================================ */ char *search_members(const char *guild_id, const char *query, int limit) {
    char url[1024];
    if (limit < 1) limit = 20;
    if (limit > 1000) limit = 1000;
    snprintf(url, sizeof(url), "%s/guilds/%s/members/search?query=%s&limit=%d",
             DISCORD_API_BASE, guild_id, query, limit);
    tool_api_result_t *r = discord_api_call(url, "GET", NULL, 15, 2);
    if (!tool_api_success(r)) {
        char *err = json_errorf("Search members failed: %s", r->error ? r->error : "unknown");
        tool_api_result_free(r); return err; }
    json_t *result = json_object();
    json_t *members = json_array();
    if (r->json) {
        size_t len = json_len(r->json);
        for (size_t i = 0; i < len; i++) {
            json_t *m = json_get(r->json, i);
            if (!m) continue;
            json_t *e = json_object();
            json_t *user = json_obj_get(m, "user");
            if (user) {
                json_set(e, "id", json_copy(json_obj_get(user, "id")));
                json_set(e, "username", json_copy(json_obj_get(user, "username")));
                json_set(e, "global_name", json_copy(json_obj_get(user, "global_name")));
            }
            json_set(e, "nick", json_copy(json_obj_get(m, "nick")));
            json_set(e, "roles", json_copy(json_obj_get(m, "roles")));
            json_set(e, "joined_at", json_copy(json_obj_get(m, "joined_at")));
            json_append(members, e);
        }
    }
    json_set(result, "members", members);
    json_set(result, "count", json_number((double)json_len(members)));
    char *s = json_serialize(result); json_free(result); tool_api_result_free(r);
    return s;
}


/* Port of Python tools/discord_tool.py:_pin_message(). */ char *pin_message(const char *channel_id, const char *message_id) {
    char url[512];
    snprintf(url, sizeof(url), "%s/channels/%s/pins/%s", DISCORD_API_BASE, channel_id, message_id);
    tool_api_result_t *r = discord_api_call(url, "PUT", NULL, 15, 2);
    if (!tool_api_success(r)) {
        char *err = json_errorf("Pin failed: %s", r->error ? r->error : "unknown");
        tool_api_result_free(r); return err; }
    json_t *result = json_object();
    json_set(result, "success", json_bool(true));
    char msg[128];
    snprintf(msg, sizeof(msg), "Message %s pinned in channel %s", message_id, channel_id);
    json_set(result, "message", json_string(msg));
    char *s = json_serialize(result); json_free(result); tool_api_result_free(r);
    return s;
}


/* Port of Python tools/discord_tool.py:_unpin_message(). */ char *unpin_message(const char *channel_id, const char *message_id) {
    char url[512];
    snprintf(url, sizeof(url), "%s/channels/%s/pins/%s", DISCORD_API_BASE, channel_id, message_id);
    tool_api_result_t *r = discord_api_call(url, "DELETE", NULL, 15, 2);
    if (!tool_api_success(r)) {
        char *err = json_errorf("Unpin failed: %s", r->error ? r->error : "unknown");
        tool_api_result_free(r); return err; }
    json_t *result = json_object();
    json_set(result, "success", json_bool(true));
    char msg[128];
    snprintf(msg, sizeof(msg), "Message %s unpinned from channel %s", message_id, channel_id);
    json_set(result, "message", json_string(msg));
    char *s = json_serialize(result); json_free(result); tool_api_result_free(r);
    return s;
}


/* Port of Python tools/discord_tool.py:_create_thread(). */ char *create_thread(const char *channel_id, const char *name, const char *message_id) {
    char url[512];
    json_t *body = json_object();
    json_set(body, "name", json_string(name ? name : "new-thread"));
    if (message_id)
        snprintf(url, sizeof(url), "%s/channels/%s/messages/%s/threads", DISCORD_API_BASE, channel_id, message_id);
    else
        snprintf(url, sizeof(url), "%s/channels/%s/threads", DISCORD_API_BASE, channel_id);
    if (!message_id)
        json_set(body, "type", json_number(11));
    char *body_str = json_serialize(body); json_free(body);
    tool_api_result_t *r = discord_api_call(url, "POST", body_str, 15, 2);
    free(body_str);
    if (!tool_api_success(r)) {
        char *err = json_errorf("Create thread failed: %s", r->error ? r->error : "unknown");
        tool_api_result_free(r); return err; }
    json_t *result = json_object();
    if (r->json) {
        json_set(result, "id", json_copy(json_obj_get(r->json, "id")));
        json_set(result, "name", json_copy(json_obj_get(r->json, "name")));
        json_set(result, "type", json_copy(json_obj_get(r->json, "type")));
    }
    char *s = json_serialize(result); json_free(result); tool_api_result_free(r);
    return s;
}


/* Port of Python tools/discord_tool.py:_add_role(). */
/* ================================================================
 *  Role management actions (P43 — depth expansion)
 * ================================================================ */ char *add_role(const char *guild_id, const char *user_id, const char *role_id) {
    char url[512];
    snprintf(url, sizeof(url), "%s/guilds/%s/members/%s/roles/%s",
             DISCORD_API_BASE, guild_id, user_id, role_id);
    tool_api_result_t *r = discord_api_call(url, "PUT", NULL, 15, 2);
    if (!tool_api_success(r)) {
        char *err = json_errorf("Add role failed: %s", r->error ? r->error : "unknown");
        tool_api_result_free(r); return err; }
    json_t *result = json_object();
    json_set(result, "success", json_bool(true));
    char msg[256];
    snprintf(msg, sizeof(msg), "Role %s added to user %s in guild %s", role_id, user_id, guild_id);
    json_set(result, "message", json_string(msg));
    char *s = json_serialize(result); json_free(result); tool_api_result_free(r);
    return s;
}


/* Port of Python tools/discord_tool.py:_remove_role(). */ char *remove_role(const char *guild_id, const char *user_id, const char *role_id) {
    char url[512];
    snprintf(url, sizeof(url), "%s/guilds/%s/members/%s/roles/%s",
             DISCORD_API_BASE, guild_id, user_id, role_id);
    tool_api_result_t *r = discord_api_call(url, "DELETE", NULL, 15, 2);
    if (!tool_api_success(r)) {
        char *err = json_errorf("Remove role failed: %s", r->error ? r->error : "unknown");
        tool_api_result_free(r); return err; }
    json_t *result = json_object();
    json_set(result, "success", json_bool(true));
    char msg[256];
    snprintf(msg, sizeof(msg), "Role %s removed from user %s in guild %s", role_id, user_id, guild_id);
    json_set(result, "message", json_string(msg));
    char *s = json_serialize(result); json_free(result); tool_api_result_free(r);
    return s;
}
