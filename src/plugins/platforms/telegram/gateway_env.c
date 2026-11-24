/*
 * gateway_env.c -- real C11 port of upstream Hermes
 *   plugins/platforms/telegram/gateway_env.py.
 *
 * PoP: w22_gatewayenv_1c1b2e_enable_credentials @ plugins/platforms/telegram/gateway_env.py:ENV_ENABLE_CREDENTIALS
 * PoP: w22_gatewayenv_1c1b2e_authz @ plugins/platforms/telegram/gateway_env.py:AUTHZ
 * PoP: w22_gatewayenv_1c1b2e_overrides @ plugins/platforms/telegram/gateway_env.py:ENV_OVERRIDES
 * PoP: w22_gatewayenv_1c1b2e_fallback_ips @ plugins/platforms/telegram/gateway_env.py:_fallback_ips
 *
 * The steps themselves (`EnvCred.__call__`, `env_reply_mode`,
 * `env_home_channel`) belong to gateway/platform_env.py and gateway/config_env.py
 * -- other C homes.  What this module contributes is the ordered declaration and
 * the one inline step that parses the fallback IP list, and that is what the
 * port reproduces verbatim.
 */
#include "hermes_json.h"
#include "noc_w22_gatewayenv_1c1b2e.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

json_t *w22_gatewayenv_1c1b2e_enable_credentials(void) {
    json_t *out = json_array();
    json_append(out, json_string("TELEGRAM_BOT_TOKEN"));
    return out;
}

json_t *w22_gatewayenv_1c1b2e_authz(void) {
    json_t *out = json_object();
    json_set(out, "allowed_users_env", json_string("TELEGRAM_ALLOWED_USERS"));
    json_set(out, "allow_all_env", json_string("TELEGRAM_ALLOW_ALL_USERS"));
    json_set(out, "allow_bots_env", json_string("TELEGRAM_ALLOW_BOTS"));
    json_set(out, "group_allowed_chats_env", json_string("TELEGRAM_GROUP_ALLOWED_CHATS"));
    json_set(out, "allowlist_suppresses_pairing", json_bool(1));
    return out;
}

json_t *w22_gatewayenv_1c1b2e_overrides(void) {
    json_t *out = json_array();
    json_t *cred, *creds, *group;

    cred = json_object();
    json_set(cred, "kind", json_string("cred"));
    json_set(cred, "platform", json_string("telegram"));
    creds = json_array();
    group = json_array();
    json_append(group, json_string("TELEGRAM_BOT_TOKEN"));
    json_append(creds, group);
    json_set(cred, "creds", creds);
    json_set(cred, "token", json_string("TELEGRAM_BOT_TOKEN"));
    json_append(out, cred);

    {
        json_t *reply = json_object();
        json_set(reply, "kind", json_string("reply_mode"));
        json_set(reply, "platform", json_string("telegram"));
        json_set(reply, "env", json_string("TELEGRAM_REPLY_TO_MODE"));
        json_append(out, reply);
    }
    {
        json_t *fallback = json_object();
        json_set(fallback, "kind", json_string("callable"));
        json_set(fallback, "name", json_string("_fallback_ips"));
        json_append(out, fallback);
    }
    {
        json_t *home = json_object();
        json_set(home, "kind", json_string("home"));
        json_set(home, "platform", json_string("telegram"));
        json_set(home, "env_base", json_string("TELEGRAM_HOME_CHANNEL"));
        json_set(home, "strip", json_bool(0));
        json_append(out, home);
    }
    return out;
}

/* str.strip() for ASCII whitespace, as platform_env.csv_list applies it. */
static char *genv_strip(const char *s) {
    const char *a = s ? s : "";
    const char *b;
    size_t n;
    char *out;
    while (*a && isspace((unsigned char)*a)) a++;
    b = a + strlen(a);
    while (b > a && isspace((unsigned char)b[-1])) b--;
    n = (size_t)(b - a);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, a, n);
    out[n] = '\0';
    return out;
}

/* csv_list: [part.strip() for part in value.split(",") if part.strip()] */
static json_t *genv_csv_list(const char *value) {
    json_t *arr = json_array();
    const char *p = value ? value : "";
    for (;;) {
        const char *comma = strchr(p, ',');
        size_t n = comma ? (size_t)(comma - p) : strlen(p);
        char *seg = (char *)malloc(n + 1);
        if (!seg) return arr;
        memcpy(seg, p, n);
        seg[n] = '\0';
        {
            char *part = genv_strip(seg);
            if (part && *part) json_append(arr, json_string(part));
            free(part);
        }
        free(seg);
        if (!comma) break;
        p = comma + 1;
    }
    return arr;
}

/* The getenv seam: env values are text; anything else reads as unset. */
static const char *genv_get(const json_t *env, const char *name) {
    json_t *v;
    if (!env || env->type != JSON_OBJECT || !name) return "";
    v = json_obj_get((json_t *)env, name);
    if (!v || v->type != JSON_STRING || !v->str_val) return "";
    return v->str_val;
}

json_t *w22_gatewayenv_1c1b2e_fallback_ips(const json_t *env) {
    const char *ips = genv_get(env, "TELEGRAM_FALLBACK_IPS");
    json_t *out;
    if (!ips || !ips[0]) return json_null(); /* `if ips := ...` is falsy */
    out = json_object();
    json_set(out, "fallback_ips", genv_csv_list(ips));
    return out;
}
