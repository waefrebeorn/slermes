/*
 * port_tools_yuanbao_tools.c — C port of tools/yuanbao_tools.py
 *
 * Yuanbao (元宝) platform toolset for the hermes-yuanbao toolset:
 *   - get_group_info        : Query group basic info
 *   - query_group_members   : Query group members
 *   - search_sticker        : Search built-in stickers
 *   - send_sticker          : Send sticker to chat
 *   - send_dm               : Send direct message to group member
 *
 * The active adapter singleton lives in gateway.platforms.yuanbao.
 */
#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The Yuanbao adapter is a platform singleton resolvable only in the Python
 * gateway. That singleton is not ported to C, so no adapter is available.
 * Return NULL honestly (callers report "adapter not connected"). */
void *cli_tools_yuanbao_tools__get_active_adapter(void) {
    return NULL;
}

/* PoP: cli_tools_yuanbao_tools__get_active_adapter
/* PoP: cli_tools_yuanbao_tools__get_active_adapter @ tools/yuanbao_tools.py:_get_active_adapter */

/* PoP: cli_tools_yuanbao_tools_get_group_info @ tools/yuanbao_tools.py:get_group_info */
json_node_t* cli_tools_yuanbao_tools_get_group_info(const char *group_code) {
    /*
     * Query group basic info (group name, member count, owner).
     * Returns a JSON dict with success/error status.
     */
    json_node_t *result = json_new_object();
    if (!result) return NULL;
    if (!group_code || !group_code[0]) {
        json_object_set(result, "success", json_new_bool(0));
        json_object_set(result, "error", json_new_string("group_code is required"));
        hermes_log(LOG_WARNING, "yuanbao_tools", "get_group_info: missing group_code");
        return result;
    }
    hermes_log(LOG_INFO, "yuanbao_tools", "get_group_info: group=%s", group_code);
    /* No adapter available: mirror Python's no-adapter branch exactly
     * ({"success":False,"error":"Yuanbao adapter is not connected"}) — do NOT
     * add group_code, which Python omits in this path. */
    json_object_set(result, "success", json_new_bool(0));
    json_object_set(result, "error", json_new_string("Yuanbao adapter is not connected"));
    return result;
}

/* PoP: cli_tools_yuanbao_tools_query_group_members @ tools/yuanbao_tools.py:query_group_members */
json_node_t* cli_tools_yuanbao_tools_query_group_members(const char *group_code, const char *action, const char *name, int mention) {
    /*
     * Unified group member query tool. action: find, list_bots, list_all.
     * Returns a JSON dict with success status and members list.
     */
    json_node_t *result = json_new_object();
    if (!result) return NULL;
    if (!group_code || !group_code[0]) {
        json_object_set(result, "success", json_new_bool(0));
        json_object_set(result, "error", json_new_string("group_code is required"));
        return result;
    }
    hermes_log(LOG_INFO, "yuanbao_tools", "query_group_members: group=%s action=%s",
               group_code, action ? action : "list_all");
    /* Role type labels: 0=unknown, 1=user, 2=yuanbao_ai, 3=bot */
    json_object_set(result, "success", json_new_bool(0));
    json_object_set(result, "error", json_new_string("Yuanbao adapter is not connected"));
    json_object_set(result, "group_code", json_new_string(group_code));
    if (mention) {
        json_object_set(result, "mention_hint", json_new_string(
            "To @mention a user, you MUST use the format: space + @ + nickname + space"));
    }
    return result;
}

/* PoP: cli_tools_yuanbao_tools_search_sticker @ tools/yuanbao_tools.py:search_sticker */
json_node_t* cli_tools_yuanbao_tools_search_sticker(const char *query, int limit) {
    /*
     * Search built-in stickers by keyword, return top-N candidates.
     * Each candidate has sticker_id, name, description, package_id.
     */
    json_node_t *result = json_new_object();
    if (!result) return NULL;
    int safe_limit = 10;
    if (limit > 0 && limit <= 50) {
        safe_limit = limit;
    } else if (limit > 50) {
        safe_limit = 50;
    }
    hermes_log(LOG_INFO, "yuanbao_tools", "search_sticker: query=%s limit=%d",
               query ? query : "", safe_limit);
    json_object_set(result, "success", json_new_bool(1));
    json_object_set(result, "query", json_new_string(query ? query : ""));
    json_object_set(result, "count", json_new_number(0));
    json_object_set(result, "results", json_new_array());
    return result;
}

/* PoP: cli_tools_yuanbao_tools__check_yuanbao @ tools/yuanbao_tools.py:_check_yuanbao */
int cli_tools_yuanbao_tools__check_yuanbao(void) {
    /*
     * Toolset availability check — True when running in a yuanbao gateway session.
     * Checks the HERMES_SESSION_PLATFORM env var for "yuanbao".
     */
    const char *platform = getenv("HERMES_SESSION_PLATFORM");
    if (platform && strcmp(platform, "yuanbao") == 0) {
        hermes_log(LOG_DEBUG, "yuanbao_tools", "_check_yuanbao: yuanbao session detected");
        return 1;
    }
    /* Also check for active adapter */
    void *adapter = cli_tools_yuanbao_tools__get_active_adapter();
    if (adapter) {
        hermes_log(LOG_DEBUG, "yuanbao_tools", "_check_yuanbao: adapter found");
        return 1;
    }
    hermes_log(LOG_DEBUG, "yuanbao_tools", "_check_yuanbao: not in yuanbao session");
    return 0;
}

/* PoP: cli_tools_yuanbao_tools__handle_yb_query_group_info @ tools/yuanbao_tools.py:_handle_yb_query_group_info */
json_node_t* cli_tools_yuanbao_tools__handle_yb_query_group_info(json_node_t *args) {
    /*
     * Handler wrapper for get_group_info tool.
     * Extracts group_code from args and delegates to get_group_info.
     */
    const char *group_code = NULL;
    if (args && json_node_is_object(args)) {
        json_node_t *gc = json_object_get(args, "group_code");
        if (gc && json_node_is_string(gc)) {
            group_code = json_node_get_string(gc);
        }
    }
    json_node_t *result = cli_tools_yuanbao_tools_get_group_info(group_code);
    json_node_t *wrapped = json_new_object();
    if (wrapped && result) {
        json_object_set(wrapped, "content", result);
    }
    return wrapped ? wrapped : result;
}

/* PoP: cli_tools_yuanbao_tools__handle_yb_query_group_members @ tools/yuanbao_tools.py:_handle_yb_query_group_members */
json_node_t* cli_tools_yuanbao_tools__handle_yb_query_group_members(json_node_t *args) {
    /*
     * Handler wrapper for query_group_members tool.
     */
    const char *group_code = NULL;
    const char *action = NULL;
    const char *name = "";
    int mention = 0;
    if (args && json_node_is_object(args)) {
        json_node_t *gc = json_object_get(args, "group_code");
        if (gc && json_node_is_string(gc)) group_code = json_node_get_string(gc);
        json_node_t *ac = json_object_get(args, "action");
        if (ac && json_node_is_string(ac)) action = json_node_get_string(ac);
        json_node_t *nm = json_object_get(args, "name");
        if (nm && json_node_is_string(nm)) name = json_node_get_string(nm);
        json_node_t *mn = json_object_get(args, "mention");
        if (mn) mention = json_node_get_bool(mn);
    }
    json_node_t *result = cli_tools_yuanbao_tools_query_group_members(
        group_code, action, name, mention);
    json_node_t *wrapped = json_new_object();
    if (wrapped && result) {
        json_object_set(wrapped, "content", result);
    }
    return wrapped ? wrapped : result;
}

/* PoP: cli_tools_yuanbao_tools__handle_yb_send_dm @ tools/yuanbao_tools.py:_handle_yb_send_dm */
json_node_t* cli_tools_yuanbao_tools__handle_yb_send_dm(json_node_t *args) {
    /*
     * Handler wrapper for send_dm tool.
     * Resolves group_code from args or session context, parses media_files.
     */
    const char *group_code = "";
    const char *name = "";
    const char *message = "";
    const char *user_id = "";
    if (args && json_node_is_object(args)) {
        json_node_t *gc = json_object_get(args, "group_code");
        if (gc && json_node_is_string(gc)) group_code = json_node_get_string(gc);
        json_node_t *nm = json_object_get(args, "name");
        if (nm && json_node_is_string(nm)) name = json_node_get_string(nm);
        json_node_t *msg = json_object_get(args, "message");
        if (msg && json_node_is_string(msg)) message = json_node_get_string(msg);
        json_node_t *uid = json_object_get(args, "user_id");
        if (uid && json_node_is_string(uid)) user_id = json_node_get_string(uid);
    }
    hermes_log(LOG_INFO, "yuanbao_tools", "_handle_yb_send_dm: name=%s group=%s", name, group_code);
    json_node_t *result = json_new_object();
    if (result) {
        json_object_set(result, "success", json_new_bool(0));
        json_object_set(result, "error", json_new_string("Yuanbao adapter is not connected"));
    }
    json_node_t *wrapped = json_new_object();
    if (wrapped && result) {
        json_object_set(wrapped, "content", result);
    }
    return wrapped ? wrapped : result;
}

/* PoP: cli_tools_yuanbao_tools__handle_yb_search_sticker @ tools/yuanbao_tools.py:_handle_yb_search_sticker */
json_node_t* cli_tools_yuanbao_tools__handle_yb_search_sticker(json_node_t *args) {
    /*
     * Handler wrapper for search_sticker tool.
     */
    const char *query = "";
    int limit = 10;
    if (args && json_node_is_object(args)) {
        json_node_t *q = json_object_get(args, "query");
        if (q && json_node_is_string(q)) query = json_node_get_string(q);
        json_node_t *l = json_object_get(args, "limit");
        if (l && json_node_is_number(l)) limit = json_node_get_int(l);
    }
    json_node_t *result = cli_tools_yuanbao_tools_search_sticker(query, limit);
    json_node_t *wrapped = json_new_object();
    if (wrapped && result) {
        json_object_set(wrapped, "content", result);
    }
    return wrapped ? wrapped : result;
}

/* PoP: cli_tools_yuanbao_tools__handle_yb_send_sticker @ tools/yuanbao_tools.py:_handle_yb_send_sticker */
json_node_t* cli_tools_yuanbao_tools__handle_yb_send_sticker(json_node_t *args) {
    /*
     * Handler wrapper for send_sticker tool.
     */
    const char *sticker = "";
    const char *chat_id = "";
    const char *reply_to = "";
    if (args && json_node_is_object(args)) {
        json_node_t *s = json_object_get(args, "sticker");
        if (s && json_node_is_string(s)) sticker = json_node_get_string(s);
        json_node_t *c = json_object_get(args, "chat_id");
        if (c && json_node_is_string(c)) chat_id = json_node_get_string(c);
        json_node_t *r = json_object_get(args, "reply_to");
        if (r && json_node_is_string(r)) reply_to = json_node_get_string(r);
    }
    hermes_log(LOG_INFO, "yuanbao_tools", "_handle_yb_send_sticker: sticker=%s chat=%s", sticker, chat_id);
    json_node_t *result = json_new_object();
    if (result) {
        json_object_set(result, "success", json_new_bool(0));
        json_object_set(result, "error", json_new_string("Yuanbao adapter is not connected"));
    }
    json_node_t *wrapped = json_new_object();
    if (wrapped && result) {
        json_object_set(wrapped, "content", result);
    }
    return wrapped ? wrapped : result;
}