/*
 * gateway_env.c -- C11 port of the WhatsApp (Baileys bridge) env -> gateway
 * config bridge, upstream Hermes plugins/platforms/whatsapp/gateway_env.py.
 *
 * PoP: w22_gatewayenv_bc3450_env_enable_credentials @ plugins/platforms/whatsapp/gateway_env.py:ENV_ENABLE_CREDENTIALS
 * PoP: w22_gatewayenv_bc3450_authz @ plugins/platforms/whatsapp/gateway_env.py:AUTHZ
 * PoP: w22_gatewayenv_bc3450_enabled_flag @ plugins/platforms/whatsapp/gateway_env.py:_enabled_flag
 * PoP: w22_gatewayenv_bc3450_home_channel @ plugins/platforms/whatsapp/gateway_env.py:home_step
 *
 * `getenv`, `enable_from_env` and the GatewayConfig mutation are the live
 * config seam and stay upstream: the C functions take one env reading and
 * answer the decision the step takes on it. Pure C11, ASCII, no VLA.
 */
#include "hermes_json.h"
#include "noc_w22_gatewayenv_bc3450.h"

#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* Python's str.strip().lower() for an ASCII reading of the variable. */
static char *genvbc_3450_fold(const char *raw) {
    const char *start = raw ? raw : "";
    const char *end;
    char *out;
    size_t i, n;
    while (*start && isspace((unsigned char)*start)) start++;
    end = start + strlen(start);
    while (end > start && isspace((unsigned char)end[-1])) end--;
    n = (size_t)(end - start);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)start[i];
        out[i] = (char)((c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c);
    }
    out[n] = '\0';
    return out;
}

/* utils.is_truthy_value over a string reading: strip().lower() in TRUTHY_STRINGS. */
static int genvbc_3450_truthy(const char *folded) {
    return !strcmp(folded, "1") || !strcmp(folded, "true") ||
           !strcmp(folded, "yes") || !strcmp(folded, "on");
}

/* `raw.lower() in {"false", "0", "no"}` -- no strip on this branch. */
static int genvbc_3450_falseish(const char *raw) {
    const char *s = raw ? raw : "";
    const char *words[] = {"false", "0", "no"};
    char buf[8];
    size_t i, n = strlen(s);
    if (n >= sizeof(buf)) return 0;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        buf[i] = (char)((c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c);
    }
    buf[n] = '\0';
    for (i = 0; i < sizeof(words) / sizeof(words[0]); i++) {
        if (!strcmp(buf, words[i])) return 1;
    }
    return 0;
}

json_t *w22_gatewayenv_bc3450_env_enable_credentials(void) {
    json_t *out = json_array();
    if (!out) return NULL;
    json_append(out, json_string("WHATSAPP_ENABLED"));
    return out;
}

json_t *w22_gatewayenv_bc3450_authz(void) {
    json_t *out = json_object();
    json_t *own;
    if (!out) return NULL;
    json_set(out, "allowed_users_env", json_string("WHATSAPP_ALLOWED_USERS"));
    json_set(out, "allow_all_env", json_string("WHATSAPP_ALLOW_ALL_USERS"));
    own = json_array();
    if (own) {
        json_append(own, json_string("WHATSAPP_DM_POLICY"));
        json_append(own, json_string("WHATSAPP_GROUP_POLICY"));
        json_set(out, "own_policy_env", own);
    }
    json_set(out, "allowlist_suppresses_pairing", json_bool(1));
    return out;
}

json_t *w22_gatewayenv_bc3450_enabled_flag(const char *raw, int has_platform, int yaml_enabled) {
    json_t *out = json_object();
    char *folded;
    const char *action = "none";
    if (!out) return NULL;
    folded = genvbc_3450_fold(raw);
    if (!folded) return out;
    if (has_platform && genvbc_3450_falseish(raw)) {
        action = "disable";
        yaml_enabled = 0;
    } else if (genvbc_3450_truthy(folded)) {
        action = "enable";
        yaml_enabled = 1;
    }
    json_set(out, "action", json_string(action));
    /* enable_from_env creates the entry when the config has none; the disable
     * branch needs an entry to flip, so only those two make one appear. */
    if (has_platform || !strcmp(action, "enable")) json_set(out, "enabled", json_bool(yaml_enabled));
    else json_set(out, "enabled", json_null());
    free(folded);
    return out;
}

json_t *w22_gatewayenv_bc3450_home_channel(const char *chat_id, const char *name,
                                           const char *thread_id, int platform_configured) {
    json_t *out;
    if (!platform_configured) return NULL;
    if (!chat_id || !chat_id[0]) return NULL;
    out = json_object();
    if (!out) return NULL;
    json_set(out, "platform", json_string("whatsapp"));
    json_set(out, "chat_id", json_string(chat_id));
    json_set(out, "name", json_string(name ? name : ""));
    json_set(out, "thread_id", (thread_id && thread_id[0]) ? json_string(thread_id) : json_null());
    return out;
}
