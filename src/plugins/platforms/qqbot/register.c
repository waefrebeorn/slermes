/*
 * register.c -- real C11 port of the declarative surface of upstream Hermes
 * plugins/platforms/qqbot/register.py.
 *
 * PoP: w21_register_55dd31_is_connected @ plugins/platforms/qqbot/register.py:is_connected
 * PoP: w21_register_55dd31_config_connected @ plugins/platforms/qqbot/register.py:register
 * PoP: w21_register_55dd31_platform_hint @ plugins/platforms/qqbot/register.py:_PLATFORM_HINT
 * PoP: w21_register_55dd31_register_meta @ plugins/platforms/qqbot/register.py:register
 *
 * `_env_configured` is `env_is_connected("QQ_APP_ID", "QQ_CLIENT_SECRET")`, a
 * profile/.env read: the C port takes the two resolved values as arguments.
 * `setup_qqbot` (interactive prompts, save_env_value, QR onboarding),
 * `standalone_send` (REST sender) and `register` itself (live PluginContext,
 * QQAdapter, gateway_env bridge) stay upstream.
 */
#include "hermes_json.h"
#include "noc_w21_register_55dd31.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PlatformConfig.extra.get(k) or "" -> str() -> .strip(), non-blank? */
static int qq55_extra_nonblank(const json_t *extra, const char *key) {
    const json_t *v;
    char buf[64];
    const char *text;
    size_t n;

    if (!extra || extra->type != JSON_OBJECT) return 0;   /* `or {}` */
    v = json_obj_get(extra, key);
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;                           /* falsy -> "" */
    case JSON_BOOL:   if (!v->bool_val) return 0; text = "True"; break;
    case JSON_NUMBER:
        if (v->num_val == 0.0) return 0;                  /* falsy -> "" */
        if (v->num_val == (double)(long long)v->num_val) {
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        } else {
            snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        }
        text = buf;
        break;
    case JSON_STRING: text = v->str_val ? v->str_val : ""; break;
    default:          if (v->c.count == 0) return 0; text = "{}"; break;
    }
    while (*text == ' ' || *text == '\t' || *text == '\n' || *text == '\r' ||
           *text == '\f' || *text == '\v') text++;
    n = strlen(text);
    while (n > 0 && (text[n - 1] == ' ' || text[n - 1] == '\t' || text[n - 1] == '\n' ||
                     text[n - 1] == '\r' || text[n - 1] == '\f' || text[n - 1] == '\v')) {
        n--;
    }
    return n > 0;
}

/* (get_env_value(name) or "").strip() for one resolved value. */
static int qq55_env_nonblank(const char *value) {
    const char *p = value ? value : "";
    size_t n;
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\f' || *p == '\v') p++;
    n = strlen(p);
    while (n > 0 && (p[n - 1] == ' ' || p[n - 1] == '\t' || p[n - 1] == '\n' ||
                     p[n - 1] == '\r' || p[n - 1] == '\f' || p[n - 1] == '\v')) {
        n--;
    }
    return n > 0;
}

/* Python truthiness of a decoded value (config.extra.get(k) has no strip()). */
static int qq55_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0;
    }
    return 0;
}

static int qq55_truthy_extra(const json_t *extra, const char *key) {
    if (!extra || extra->type != JSON_OBJECT) return 0;
    return qq55_truthy(json_obj_get(extra, key));
}

int w21_register_55dd31_is_connected(const json_t *extra,
                                     const char *env_app_id,
                                     const char *env_client_secret) {
    if (qq55_extra_nonblank(extra, "app_id") &&
        qq55_extra_nonblank(extra, "client_secret")) {
        return 1;
    }
    /* _env_configured: all((get_env_value(name) or "").strip() for name in names) */
    return qq55_env_nonblank(env_app_id) && qq55_env_nonblank(env_client_secret);
}

int w21_register_55dd31_config_connected(const json_t *extra,
                                         const json_t *token,
                                         const json_t *api_key) {
    /* bool(config.token or config.api_key) or all(config.extra.get(k) for k in keys) */
    if (qq55_truthy(token) || qq55_truthy(api_key)) return 1;
    return qq55_truthy_extra(extra, "app_id") && qq55_truthy_extra(extra, "client_secret");
}

static const char QQ55_PLATFORM_HINT[] =
    "You are on QQ, a popular Chinese messaging platform. QQ supports markdown formatting and"
    " emoji. You can send media files natively: include MEDIA:/absolute/path/to/file in your "
    "response. Images are sent as native photos, and other files arrive as downloadable "
    "documents.";

const char *w21_register_55dd31_platform_hint(void) {
    return QQ55_PLATFORM_HINT;
}

json_t *w21_register_55dd31_register_meta(void) {
    json_t *meta = json_object();
    json_t *required = json_array();
    json_t *dash = json_array();
    json_t *policy = json_object();

    json_set(meta, "name", json_string("qqbot"));
    json_set(meta, "label", json_string("QQ Bot"));
    json_set(meta, "emoji", json_string("\xf0\x9f\x90\xa7"));
    json_set(meta, "docs_url", json_string("https://q.qq.com"));
    json_set(meta, "dashboard_description",
             json_string("Connect Hermes to a QQ Bot from the QQ Open Platform."));
    json_append(required, json_string("QQ_APP_ID"));
    json_append(required, json_string("QQ_CLIENT_SECRET"));
    json_set(meta, "required_env", required);
    json_append(dash, json_string("QQ_APP_ID"));
    json_append(dash, json_string("QQ_CLIENT_SECRET"));
    json_append(dash, json_string("QQ_ALLOWED_USERS"));
    json_set(meta, "dashboard_env_vars", dash);
    json_set(meta, "install_hint",
             json_string("QQBot: aiohttp/httpx missing or QQ_APP_ID/QQ_CLIENT_SECRET not configured"));
    json_set(meta, "cron_deliver_env_var", json_string("QQBOT_HOME_CHANNEL"));
    json_set(meta, "platform_hint", json_string(QQ55_PLATFORM_HINT));
    json_set(policy, "label", json_string("QQ Bot"));
    json_set(policy, "media", json_bool(1));
    json_set(policy, "captionable", json_bool(1));
    json_set(policy, "text_direct", json_bool(1));
    json_set(meta, "standalone_send", policy);
    return meta;
}
