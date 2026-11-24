/*
 * gateway_env.c -- real C11 port of the declarative QQ (Official Bot API v2)
 * env bridge of upstream Hermes plugins/platforms/qqbot/gateway_env.py.
 *
 * PoP: w21_gatewayenv_2a9ae5_home @ plugins/platforms/qqbot/gateway_env.py:_home
 * PoP: w21_gatewayenv_2a9ae5_authz @ plugins/platforms/qqbot/gateway_env.py:AUTHZ
 * PoP: w21_gatewayenv_2a9ae5_env_enable @ plugins/platforms/qqbot/gateway_env.py:ENV_ENABLE_CREDENTIALS
 * PoP: w21_gatewayenv_2a9ae5_override_plan @ plugins/platforms/qqbot/gateway_env.py:ENV_OVERRIDES
 *
 * `_home` reads the environment through gateway.platform_env.getenv, so the
 * port takes the environment as a value (the env seam) instead of mutating the
 * process environment; `_getenv_str(name, default)` returns the variable's
 * text when it is set, even when blank, and the default only when absent.
 */
#include "hermes_json.h"
#include "noc_w21_gatewayenv_2a9ae5.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* getenv(name, default) for a decoded env object. */
static const char *ge(const json_t *env, const char *name, const char *fallback) {
    json_t *v;
    if (!env || env->type != JSON_OBJECT) return fallback;
    v = json_obj_get(env, name);
    if (!v || v->type == JSON_NULL) return fallback;
    if (v->type != JSON_STRING) return fallback;      /* os.environ holds text */
    return v->str_val ? v->str_val : fallback;
}

/* `value.strip()` into a caller buffer. */
static void ge_strip(const char *value, char *out, size_t cap) {
    size_t len, start = 0, j = 0;
    if (!value) value = "";
    len = strlen(value);
    while (start < len && isspace((unsigned char)value[start])) start++;
    while (len > start && isspace((unsigned char)value[len - 1])) len--;
    for (; start < len && j + 1 < cap; start++) out[j++] = value[start];
    out[j] = '\0';
}

json_t *w21_gatewayenv_2a9ae5_home(const json_t *env) {
    char qq_home[512];
    char name_env[64];
    char chat_id[512], name[512], thread[512];
    int warned = 0;
    json_t *out = json_object();

    ge_strip(ge(env, "QQBOT_HOME_CHANNEL", ""), qq_home, sizeof(qq_home));
    snprintf(name_env, sizeof(name_env), "QQBOT_HOME_CHANNEL_NAME");
    if (!qq_home[0]) {
        char legacy[512];
        ge_strip(ge(env, "QQ_HOME_CHANNEL", ""), legacy, sizeof(legacy));
        if (legacy[0]) {
            /* Back-compat: accept the pre-rename name and warn. */
            snprintf(qq_home, sizeof(qq_home), "%s", legacy);
            snprintf(name_env, sizeof(name_env), "QQ_HOME_CHANNEL_NAME");
            warned = 1;
        }
    }

    json_set(out, "name_env", json_string(name_env));
    json_set(out, "warned", json_bool(warned));
    if (!qq_home[0]) {
        json_set(out, "home_channel", json_null());
        return out;
    }

    snprintf(chat_id, sizeof(chat_id), "%s", qq_home);
    {
        const char *first = ge(env, "QQBOT_HOME_CHANNEL_NAME", "");
        if (first && first[0]) {
            snprintf(name, sizeof(name), "%s", first);
        } else {
            snprintf(name, sizeof(name), "%s", ge(env, name_env, "Home"));
        }
    }
    {
        const char *first = ge(env, "QQBOT_HOME_CHANNEL_THREAD_ID", "");
        if (first && first[0]) {
            snprintf(thread, sizeof(thread), "%s", first);
        } else {
            const char *second = ge(env, "QQ_HOME_CHANNEL_THREAD_ID", "");
            snprintf(thread, sizeof(thread), "%s", (second && second[0]) ? second : "");
        }
    }
    {
        json_t *home = json_object();
        json_set(home, "platform", json_string("qqbot"));
        json_set(home, "chat_id", json_string(chat_id));
        json_set(home, "name", json_string(name));
        json_set(home, "thread_id", thread[0] ? json_string(thread) : json_null());
        json_set(out, "home_channel", home);
    }
    return out;
}

json_t *w21_gatewayenv_2a9ae5_authz(void) {
    json_t *out = json_object();
    json_set(out, "allowed_users_env", json_string("QQ_ALLOWED_USERS"));
    json_set(out, "allow_all_env", json_string("QQ_ALLOW_ALL_USERS"));
    json_set(out, "own_policy_env", json_array());
    {
        json_t *pair = json_obj_get(out, "own_policy_env");
        json_append(pair, json_string(""));
        json_append(pair, json_string(""));
    }
    json_set(out, "group_allowed_chats_env", json_string("QQ_GROUP_ALLOWED_USERS"));
    json_set(out, "allowlist_suppresses_pairing", json_bool(1));
    return out;
}

json_t *w21_gatewayenv_2a9ae5_env_enable(void) {
    json_t *out = json_array();
    json_append(out, json_string("QQ_APP_ID"));
    json_append(out, json_string("QQ_CLIENT_SECRET"));
    return out;
}

static json_t *pair(const char *a, const char *b) {
    json_t *row = json_array();
    json_append(row, json_string(a));
    json_append(row, json_string(b));
    return row;
}

json_t *w21_gatewayenv_2a9ae5_override_plan(void) {
    json_t *out = json_object();
    json_t *creds = json_array();
    json_t *optional = json_array();
    json_t *stripped = json_array();

    json_set(out, "platform", json_string("qqbot"));
    /* creds: all-of the groups; each group is any-of its names. */
    json_append(creds, pair("QQ_APP_ID", "QQ_CLIENT_SECRET"));
    json_set(out, "creds", creds);
    json_set(out, "token", json_null());
    json_set(out, "fixed", json_array());
    json_append(optional, pair("app_id", "QQ_APP_ID"));
    json_append(optional, pair("client_secret", "QQ_CLIENT_SECRET"));
    json_set(out, "optional", optional);
    json_append(stripped, pair("allow_from", "QQ_ALLOWED_USERS"));
    json_append(stripped, pair("group_allow_from", "QQ_GROUP_ALLOWED_USERS"));
    json_set(out, "optional_stripped", stripped);
    json_set(out, "warn_missing", json_null());
    json_set(out, "then", json_string("_home"));
    json_set(out, "home", json_null());
    json_set(out, "home_strip", json_bool(0));
    return out;
}
