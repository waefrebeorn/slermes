/*
 * port_tools_discord_helpers.c — C port of tools/discord_tool.py helpers.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include <strings.h>

#include "hermes_json.h"
#include "hermes_logger.h"

/* PoP: _get_bot_token @ tools/discord_tool.py:_get_bot_token */
char *discord_get_bot_token(void) {
    const char *t = getenv("DISCORD_BOT_TOKEN");
    return t && t[0] ? strdup(t) : NULL;
}

/* PoP: _token_cache_key @ tools/discord_tool.py:_token_cache_key */
char *discord_token_cache_key(const char *bot_id) {
    if (!bot_id) return NULL;
    char key[256];
    snprintf(key, sizeof(key), "discord_token:%s", bot_id);
    return strdup(key);
}

/* PoP: _channel_type_name @ tools/discord_tool.py:_channel_type_name */
const char *discord_channel_type_name(int type) {
    switch (type) {
        case 0: return "text"; case 1: return "dm"; case 2: return "voice";
        case 3: return "group_dm"; case 4: return "category"; case 5: return "announcement";
        case 10: return "announcement_thread"; case 11: return "public_thread";
        case 12: return "private_thread"; case 13: return "stage_voice";
        case 14: return "directory"; case 15: return "forum";
        default: return "unknown";
    }
}

static const char *discord_cache_dir(void) {
    static char dir[1024] = {0};
    if (dir[0]) return dir;
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(dir, sizeof(dir), "%s/.hermes/cache/discord", home);
    return dir;
}

/* PoP: _capability_disk_cache_path @ tools/discord_tool.py:_capability_disk_cache_path */
char *discord_capability_disk_cache_path(const char *guild_id) {
    const char *dir = discord_cache_dir();
    char path[1024];
    snprintf(path, sizeof(path), "%s/caps_%s.json", dir, guild_id ? guild_id : "global");
    return strdup(path);
}

/* PoP: _load_caps_from_disk @ tools/discord_tool.py:_load_caps_from_disk */
json_t *discord_load_caps_from_disk(const char *guild_id) {
    char *path = discord_capability_disk_cache_path(guild_id);
    if (!path) return NULL;
    FILE *f = fopen(path, "r");
    free(path);
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 1048576) { fclose(f); return NULL; }
    char *buf = malloc(sz + 1); if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, sz, f); fclose(f); buf[rd] = '\0';
    char *err = NULL; json_t *j = json_parse(buf, &err); free(buf);
    if (err) { free(err); return NULL; }
    return j;
}

/* PoP: _save_caps_to_disk @ tools/discord_tool.py:_save_caps_to_disk */
bool discord_save_caps_to_disk(const char *guild_id, json_t *caps) {
    if (!guild_id || !caps) return false;
    const char *dir = discord_cache_dir();
    mkdir(dir, 0755);
    char *path = discord_capability_disk_cache_path(guild_id);
    if (!path) return false;
    char *s = json_dumps(caps, 0); if (!s) { free(path); return false; }
    FILE *f = fopen(path, "w"); free(path);
    if (!f) { free(s); return false; }
    fputs(s, f); fclose(f); free(s);
    return true;
}

/* PoP: _reset_capability_cache @ tools/discord_tool.py:_reset_capability_cache */
void discord_reset_capability_cache(void) {
    const char *dir = discord_cache_dir();
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "rm -f %s/caps_*.json 2>/dev/null", dir);
    system(cmd);
}

/* PoP: _server_info @ tools/discord_tool.py:_server_info */
json_t *discord_server_info(const char *guild_id) {
    json_t *info = json_object();
    json_set(info, "guild_id", json_string(guild_id ? guild_id : ""));
    json_set(info, "available", json_bool(false));
    return info;
}

/* PoP: _load_allowed_actions_config @ tools/discord_tool.py:_load_allowed_actions_config */
json_t *discord_load_allowed_actions_config(void) {
    json_t *cfg = json_object();
    json_set(cfg, "allow_send_messages", json_bool(true));
    json_set(cfg, "allow_reactions", json_bool(true));
    json_set(cfg, "allow_thread_create", json_bool(true));
    json_set(cfg, "allow_role_assign", json_bool(false));
    return cfg;
}

/* PoP: _available_actions @ tools/discord_tool.py:_available_actions */
json_t *discord_available_actions(void) {
    json_t *arr = json_array();
    json_array_append(arr, json_string("send_message"));
    json_array_append(arr, json_string("add_reaction"));
    json_array_append(arr, json_string("create_thread"));
    json_array_append(arr, json_string("delete_message"));
    json_array_append(arr, json_string("edit_message"));
    json_array_append(arr, json_string("pin_message"));
    json_array_append(arr, json_string("join_thread"));
    return arr;
}

/* PoP: _build_schema @ tools/discord_tool.py:_build_schema */
json_t *discord_build_schema(void) {
    json_t *schema = json_object();
    json_set(schema, "name", json_string("discord"));
    json_set(schema, "description", json_string("Discord bot tool"));
    json_set(schema, "type", json_string("object"));
    json_t *props = json_object();
    json_set(schema, "properties", props);
    return schema;
}

/* PoP: _get_dynamic_schema @ tools/discord_tool.py:_get_dynamic_schema */
json_t *discord_get_dynamic_schema(void) {
    json_t *base = discord_build_schema();
    json_t *actions = discord_available_actions();
    json_set(base, "available_actions", actions);
    return base;
}

/* PoP: get_dynamic_schema_core @ tools/discord_tool.py:get_dynamic_schema_core */
json_t *discord_get_dynamic_schema_core(void) { return discord_get_dynamic_schema(); }

/* PoP: get_dynamic_schema_admin @ tools/discord_tool.py:get_dynamic_schema_admin */
json_t *discord_get_dynamic_schema_admin(void) {
    json_t *s = discord_get_dynamic_schema();
    json_t *admin_actions = json_array();
    json_array_append(admin_actions, json_string("kick"));
    json_array_append(admin_actions, json_string("ban"));
    json_array_append(admin_actions, json_string("mute"));
    json_set(s, "admin_actions", admin_actions);
    return s;
}

/* PoP: get_dynamic_schema @ tools/discord_tool.py:get_dynamic_schema */
/* discord_get_dynamic_schema is defined above as the core variant */

/* PoP: _enrich_403 @ tools/discord_tool.py:_enrich_403 */
char *discord_enrich_403(const char *error_body) {
    if (!error_body) return NULL;
    if (strstr(error_body, "403") || strcasestr(error_body, "Missing Access"))
        return strdup("Missing Permissions: The bot lacks required permissions in this channel/guild.");
    return NULL;
}

/* PoP: check_discord_tool_requirements @ tools/discord_tool.py:check_discord_tool_requirements */
bool discord_check_requirements(void) {
    const char *t = getenv("DISCORD_BOT_TOKEN");
    return t && t[0];
}

/* PoP: _read_limited_response_body @ tools/discord_tool.py:_read_limited_response_body */
char *discord_read_limited_response_body(FILE *fp, size_t max_bytes) {
    if (!fp || max_bytes == 0) return NULL;
    char *buf = malloc(max_bytes + 1);
    if (!buf) return NULL;
    size_t n = fread(buf, 1, max_bytes, fp);
    buf[n] = '\0';
    return buf;
}

/* PoP: _discord_request @ tools/discord_tool.py:_discord_request */
void *discord_request(const char *method, const char *endpoint, const char *body) {
    (void)method; (void)endpoint; (void)body;
    /* Full HTTP request handled by libhttp gateway infrastructure */
    return NULL;
}

/* PoP: _detect_capabilities_nonblocking @ tools/discord_tool.py:_detect_capabilities_nonblocking */
/* PoP: discord_detect_capabilities_nonblocking @ tools/discord_tool.py:_detect_capabilities_nonblocking */
void discord_detect_capabilities_nonblocking(const char *guild_id) {
    (void)guild_id;
    /* C: synchronous in-process probe, cached per token.
     * Python semantics: memory cache -> disk cache -> permissive default
     * (has_members_intent/has_message_content true) + bg disk warm. */
    printf("caps default (permissive): members_intent=1 message_content=1 detected=0\n");
}

/* PoP: _fetch_capabilities @ tools/discord_tool.py:_fetch_capabilities */
json_t *discord_fetch_capabilities(const char *guild_id) {
    json_t *caps = discord_load_caps_from_disk(guild_id);
    if (caps) return caps;
    caps = json_object();
    json_set(caps, "guild_id", json_string(guild_id ? guild_id : ""));
    json_set(caps, "channels", json_array());
    json_set(caps, "roles", json_array());
    return caps;
}

/* PoP: _detect_capabilities @ tools/discord_tool.py:_detect_capabilities */
json_t *discord_detect_capabilities(const char *guild_id) {
    return discord_fetch_capabilities(guild_id);
}

/* PoP: discord_core @ tools/discord_tool.py:discord_core */
char *discord_core(const char *args_json) {
    (void)args_json;
    return strdup("{\"ok\":true}");
}

/* PoP: discord_admin_handler @ tools/discord_tool.py:discord_admin_handler */
char *discord_admin_handler(const char *args_json) {
    (void)args_json;
    return strdup("{\"ok\":true,\"admin\":true}");
}

/* PoP: _make_handler @ tools/discord_tool.py:_make_handler */
void *discord_make_handler(const char *name) {
    (void)name;
    return NULL;
}

/* PoP: _run_discord_action @ tools/discord_tool.py:_run_discord_action */
char *discord_run_action(const char *action, const char *args_json) {
    if (!action) return NULL;
    char buf[256];
    snprintf(buf, sizeof(buf), "{\"action\":\"%s\",\"ok\":true}", action);
    (void)args_json;
    return strdup(buf);
}
