/*
 * register.c - C11 port of upstream plugins/platforms/signal/register.py
 * (gate w25_register_bdae35).
 *
 * PoP: w25_register_bdae35_parse_send_target @ plugins/platforms/signal/register.py:_parse_send_target
 * PoP: w25_register_bdae35_is_connected @ plugins/platforms/signal/register.py:_is_connected
 * PoP: w25_register_bdae35_platform_hint @ plugins/platforms/signal/register.py:_PLATFORM_HINT
 * PoP: w25_register_bdae35_register_spec @ plugins/platforms/signal/register.py:register
 *
 * The SignalAdapter, the interactive setup and the gateway facade are seams.
 * What is ported is the parse of a "group:<id>" target ref, the configured
 * predicate over config.extra plus the scoped env names, and the literal
 * PlatformEntry table the entry point registers.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_register_bdae35.h"

/* Cached system-prompt platform hint (bytes kept stable: cached-prompt input). */
static const char SG_PLATFORM_HINT[] =
    "You are on Signal. Standard markdown (**bold**, *italic*, ~~strike~~, # headers, `code`) "
    "auto-converts to Signal formatting; bullets render as \xe2\x80\xa2. No tables \xe2\x80\x94 "
    "use bullets or labeled lines. You can send files natively: write "
    "MEDIA:/absolute/path/to/file in your response. Images (.png, .jpg, .webp) send as photos, "
    "other files as documents; ![alt](url) sends as photos.";

/* Python str.strip() over the ASCII whitespace class. */
static char *sg_strip(const char *s) {
    size_t b = 0, e;
    if (!s) s = "";
    e = strlen(s);
    while (b < e && isspace((unsigned char)s[b])) b++;
    while (e > b && isspace((unsigned char)s[e - 1])) e--;
    {
        char *out = (char *)malloc(e - b + 1);
        if (!out) return NULL;
        memcpy(out, s + b, e - b);
        out[e - b] = '\0';
        return out;
    }
}

/* Python truthiness of a JSON value. */
static int sg_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

json_t *w25_register_bdae35_parse_send_target(const json_t *req) {
    const json_t *ref = json_obj_get(req, "ref");
    char *stripped, *group_id, *target;
    size_t need;
    json_t *pair;

    if (!ref || ref->type != JSON_STRING) return json_null();
    stripped = sg_strip(ref->str_val ? ref->str_val : "");
    if (!stripped) return json_null();
    if (strncmp(stripped, "group:", 6) != 0) {
        free(stripped);
        return json_null();
    }
    group_id = sg_strip(stripped + 6);
    free(stripped);
    if (!group_id) return json_null();
    if (group_id[0] == '\0') {
        /* NOT_EXPLICIT: stop parsing, the target is not explicit. */
        free(group_id);
        return json_string("NOT_EXPLICIT");
    }
    need = strlen(group_id) + 7;
    target = (char *)malloc(need);
    if (!target) { free(group_id); return json_null(); }
    snprintf(target, need, "group:%s", group_id);
    free(group_id);
    pair = json_array();
    json_append(pair, json_string(target));
    json_append(pair, json_null());
    free(target);
    return pair;
}

/* extra.get(key) or env[name], then .strip(); non-string extras are ignored. */
static char *sg_signal_part(const json_t *extra, const json_t *env,
                            const char *key, const char *env_name) {
    const json_t *v = extra ? json_obj_get(extra, key) : NULL;
    if (v && v->type == JSON_STRING && sg_truthy(v))
        return sg_strip(v->str_val);
    return sg_strip(json_get_str(env, env_name, ""));
}

json_t *w25_register_bdae35_is_connected(const json_t *req) {
    const json_t *extra = json_obj_get(req, "extra");
    const json_t *env = json_obj_get(req, "env");
    char *url, *account, *e_url, *e_account;
    int via_config, via_env;

    if (extra && extra->type != JSON_OBJECT) extra = NULL;
    if (env && env->type != JSON_OBJECT) env = NULL;
    url = sg_signal_part(extra, env, "http_url", "SIGNAL_HTTP_URL");
    account = sg_signal_part(extra, env, "account", "SIGNAL_ACCOUNT");
    e_url = sg_strip(json_get_str(env, "SIGNAL_HTTP_URL", ""));
    e_account = sg_strip(json_get_str(env, "SIGNAL_ACCOUNT", ""));
    if (!url || !account || !e_url || !e_account) {
        free(url); free(account); free(e_url); free(e_account);
        return json_bool(false);
    }
    via_config = url[0] != '\0' && account[0] != '\0';
    via_env = e_url[0] != '\0' && e_account[0] != '\0';
    free(url); free(account); free(e_url); free(e_account);
    /* validate_signal_config(config) or _env_configured(config) */
    return json_bool(via_config || via_env);
}

json_t *w25_register_bdae35_platform_hint(void) {
    return json_string(SG_PLATFORM_HINT);
}

json_t *w25_register_bdae35_register_spec(void) {
    json_t *o = json_object();
    json_t *dash_env, *required, *standalone;

    json_set(o, "name", json_string("signal"));
    json_set(o, "label", json_string("Signal"));
    json_set(o, "dashboard_description", json_string("Connect through a signal-cli REST bridge."));
    json_set(o, "docs_url", json_string("https://github.com/bbernhard/signal-cli-rest-api"));

    dash_env = json_array();
    json_append(dash_env, json_string("SIGNAL_HTTP_URL"));
    json_append(dash_env, json_string("SIGNAL_ACCOUNT"));
    json_append(dash_env, json_string("SIGNAL_ALLOWED_USERS"));
    json_set(o, "dashboard_env_vars", dash_env);

    required = json_array();
    json_append(required, json_string("SIGNAL_HTTP_URL"));
    json_append(required, json_string("SIGNAL_ACCOUNT"));
    json_set(o, "required_env", required);

    json_set(o, "cron_deliver_env_var", json_string("SIGNAL_HOME_CHANNEL"));
    json_set(o, "install_hint", json_string("Signal: runtime requirements not met"));
    json_set(o, "emoji", json_string("\xf0\x9f\x93\xa1"));
    json_set(o, "pii_safe", json_bool(true));
    json_set(o, "allow_update_command", json_bool(true));
    json_set(o, "display_tier", json_string("low"));
    json_set(o, "max_message_length", json_number(8000.0));
    json_set(o, "platform_hint", json_string(SG_PLATFORM_HINT));

    /* StandaloneSendPolicy(label="Signal", media=True,
     *                      empty_list_between_chunks=True, text_direct=True) */
    standalone = json_object();
    json_set(standalone, "label", json_string("Signal"));
    json_set(standalone, "media", json_bool(true));
    json_set(standalone, "all_sends", json_bool(false));
    json_set(standalone, "captionable", json_bool(false));
    json_set(standalone, "empty_list_between_chunks", json_bool(true));
    json_set(standalone, "forward_force_document", json_bool(false));
    json_set(standalone, "mentions", json_bool(false));
    json_set(standalone, "text_direct", json_bool(true));
    json_set(standalone, "reply_file_over", json_number(0.0));
    json_set(o, "standalone_send", standalone);

    /* **gateway_env.AUTHZ */
    json_set(o, "allowed_users_env", json_string("SIGNAL_ALLOWED_USERS"));
    json_set(o, "allow_all_env", json_string("SIGNAL_ALLOW_ALL_USERS"));
    json_set(o, "allowlist_suppresses_pairing", json_bool(true));
    return o;
}
