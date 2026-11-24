/*
 * gateway_env.c -- real C11 port of the pure env -> config shaping of upstream
 * Hermes plugins/platforms/slack/gateway_env.py.
 *
 * PoP: w22_gatewayenv_e84967_home @ plugins/platforms/slack/gateway_env.py:_home
 * PoP: w22_gatewayenv_e84967_authz @ plugins/platforms/slack/gateway_env.py:AUTHZ
 * PoP: w22_gatewayenv_e84967_env_enable_credentials @ plugins/platforms/slack/gateway_env.py:ENV_ENABLE_CREDENTIALS
 * PoP: w22_gatewayenv_e84967_env_overrides @ plugins/platforms/slack/gateway_env.py:ENV_OVERRIDES
 *
 * `getenv` (the process environment behind the active profile secret scope) and
 * the GatewayConfig objects are the seam and are passed in: the platform map,
 * the HomeChannel dataclass and the EnvCred step stay upstream.
 */
#include "hermes_json.h"
#include "noc_w22_gatewayenv_e84967.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* os.environ.get(name, "") -- a missing or null env value reads as "". */
static const char *ge9_env(const json_t *env, const char *name) {
    json_t *v;
    if (!env || env->type != JSON_OBJECT) return "";
    v = json_obj_get(env, name);
    if (!v || v->type != JSON_STRING) return "";
    return v->str_val ? v->str_val : "";
}

static const char *ge9_opt_str(const json_t *obj, const char *key) {
    json_t *v;
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    v = json_obj_get(obj, key);
    if (!v || v->type != JSON_STRING) return NULL;
    return v->str_val;
}

json_t *w22_gatewayenv_e84967_home(const json_t *env, const json_t *existing) {
    const char *slack_home = ge9_env(env, "SLACK_HOME_CHANNEL");
    const char *name;
    const char *thread_raw;
    const char *existing_chat;
    int created, enabled = 0, same_home;
    json_t *out;

    if (!slack_home[0]) {
        /* `if not (slack_home := getenv(...)): return` -- nothing changes. */
        out = json_object();
        json_set(out, "changed", json_bool(0));
        return out;
    }

    created = !(existing && existing->type == JSON_OBJECT);
    if (!created) enabled = json_get_bool(existing, "enabled", 0) ? 1 : 0;

    existing_chat = ge9_opt_str(existing, "chat_id");
    same_home = existing_chat && !strcmp(existing_chat, slack_home);

    name = ge9_env(env, "SLACK_HOME_CHANNEL_NAME");
    thread_raw = ge9_env(env, "SLACK_HOME_CHANNEL_THREAD_ID");

    out = json_object();
    json_set(out, "changed", json_bool(1));
    json_set(out, "created", json_bool(created));
    json_set(out, "enabled", json_bool(enabled));
    json_set(out, "chat_id", json_string(slack_home));
    /* name is handed over verbatim: no `or None`, so "" survives. */
    json_set(out, "name", json_string(name));
    /* `getenv(...) or None` */
    json_set(out, "thread_id", thread_raw[0] ? json_string(thread_raw) : json_null());
    /* Provenance survives only while the chat_id is unchanged. */
    {
        const char *uid = same_home ? ge9_opt_str(existing, "user_id") : NULL;
        const char *sid = same_home ? ge9_opt_str(existing, "scope_id") : NULL;
        json_set(out, "user_id", uid ? json_string(uid) : json_null());
        json_set(out, "scope_id", sid ? json_string(sid) : json_null());
    }
    return out;
}

json_t *w22_gatewayenv_e84967_authz(void) {
    json_t *out = json_object();
    json_set(out, "allowed_users_env", json_string("SLACK_ALLOWED_USERS"));
    json_set(out, "allow_all_env", json_string("SLACK_ALLOW_ALL_USERS"));
    json_set(out, "allow_bots_env", json_string("SLACK_ALLOW_BOTS"));
    json_set(out, "allowlist_suppresses_pairing", json_bool(1));
    return out;
}

json_t *w22_gatewayenv_e84967_env_enable_credentials(void) {
    json_t *arr = json_array();
    json_append(arr, json_string("SLACK_BOT_TOKEN"));
    return arr;
}

json_t *w22_gatewayenv_e84967_env_overrides(void) {
    json_t *arr = json_array();
    json_t *cred = json_object();
    json_t *creds = json_array();
    json_t *step = json_object();
    json_append(creds, json_string("SLACK_BOT_TOKEN"));
    json_set(cred, "kind", json_string("cred"));
    json_set(cred, "platform", json_string("slack"));
    json_set(cred, "creds", creds);
    json_set(cred, "token", json_string("SLACK_BOT_TOKEN"));
    json_append(arr, cred);
    json_set(step, "kind", json_string("step"));
    json_set(step, "name", json_string("_home"));
    json_append(arr, step);
    return arr;
}
