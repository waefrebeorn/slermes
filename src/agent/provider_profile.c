/*
 * provider_profile.c — port of providers/base.py + providers/__init__.py.
 *
 * Base ProviderProfile struct + registry (name/alias lookup, last-writer-
 * wins), base-class hook defaults (get_hostname, fetch_models via libhttp),
 * and the shared _reasoning_config_for_model helper from
 * agent/transports/chat_completions.py.
 *
 * The bundled per-provider profiles live in provider_profiles_builtin.c
 * (port of plugins/model-providers/<name>/__init__.py) and register through
 * provider_profiles_register_builtin().
 *
 * PoP: provider_profile @ providers/base.py:ProviderProfile
 * PoP: provider_profile @ providers/base.py:_profile_user_agent
 * PoP: provider_profile @ providers/base.py:ProviderProfile.get_hostname
 * PoP: provider_profile @ providers/base.py:ProviderProfile.fetch_models
 * PoP: provider_profile @ providers/base.py:ProviderProfile.get_max_tokens
 * PoP: provider_profile @ providers/__init__.py:register_provider
 * PoP: provider_profile @ providers/__init__.py:get_provider_profile
 * PoP: provider_profile @ providers/__init__.py:list_providers
 * PoP: provider_profile @ agent/transports/chat_completions.py:_reasoning_config_for_model
 */
#define _POSIX_C_SOURCE 200809L
#include "provider_profile.h"
#include "http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

/* HERMES_VERSION comes from the build (-DHERMES_VERSION="...") */
#ifndef HERMES_VERSION
#define HERMES_VERSION "0.0.0-slermes"
#endif

static char *xstrdup_pp(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* ── registry ─────────────────────────────────────────────────────────── */

typedef struct {
    provider_profile_t **items;
    size_t n, cap;
} registry_t;

static registry_t g_registry;   /* canonical profiles, in registration order */
bool g_builtin_done;            /* shared latch: set once builtin profiles registered */

static void registry_push(provider_profile_t *p)
{
    if (g_registry.n == g_registry.cap) {
        g_registry.cap = g_registry.cap ? g_registry.cap * 2 : 32;
        g_registry.items = realloc(g_registry.items,
                                   g_registry.cap * sizeof(*g_registry.items));
    }
    g_registry.items[g_registry.n++] = p;
}

void provider_profile_register(provider_profile_t *p)
{
    if (!p || !p->name) return;
    /* Later registrations with the same name replace earlier ones. */
    for (size_t i = 0; i < g_registry.n; i++) {
        if (strcmp(g_registry.items[i]->name, p->name) == 0) {
            provider_profile_free(g_registry.items[i]);
            g_registry.items[i] = p;
            return;
        }
    }
    registry_push(p);
}

provider_profile_t *provider_profile_get(const char *name)
{
    if (!name) return NULL;
    if (!g_builtin_done) provider_profiles_register_builtin();
    /* canonical name first */
    for (size_t i = 0; i < g_registry.n; i++)
        if (strcmp(g_registry.items[i]->name, name) == 0)
            return g_registry.items[i];
    /* aliases */
    for (size_t i = 0; i < g_registry.n; i++) {
        provider_profile_t *p = g_registry.items[i];
        if (!p->aliases) continue;
        for (size_t j = 0; p->aliases[j]; j++)
            if (strcmp(p->aliases[j], name) == 0)
                return p;
    }
    return NULL;
}

provider_profile_t **provider_profile_list(size_t *out_n)
{
    if (!g_builtin_done) provider_profiles_register_builtin();
    provider_profile_t **out = malloc((g_registry.n ? g_registry.n : 1) * sizeof(*out));
    for (size_t i = 0; i < g_registry.n; i++) out[i] = g_registry.items[i];
    if (out_n) *out_n = g_registry.n;
    return out;
}

void provider_profile_registry_reset(void)
{
    for (size_t i = 0; i < g_registry.n; i++)
        provider_profile_free(g_registry.items[i]);
    free(g_registry.items);
    g_registry.items = NULL;
    g_registry.n = g_registry.cap = 0;
    g_builtin_done = false;
}

/* Marker used by provider_profiles_builtin.c to flip the discovery latch. */
void provider_profile_mark_builtin_done(void);
void provider_profile_mark_builtin_done(void) { g_builtin_done = true; }

/* ── construction / destruction ──────────────────────────────────────── */

provider_profile_t *provider_profile_new(const char *name)
{
    provider_profile_t *p = calloc(1, sizeof(*p));
    p->name = xstrdup_pp(name);
    p->api_mode = xstrdup_pp("chat_completions");
    p->auth_type = xstrdup_pp("api_key");
    p->supports_health_check = true;
    p->supports_vision_tool_messages = true;
    p->fixed_temperature_mode = PROFILE_TEMP_DEFAULT;
    p->default_max_tokens = 0;
    return p;
}

static void free_strv(char **v)
{
    if (!v) return;
    for (size_t i = 0; v[i]; i++) free(v[i]);
    free(v);
}

void provider_profile_free(provider_profile_t *p)
{
    if (!p) return;
    free(p->name); free(p->api_mode);
    free_strv(p->aliases);
    free(p->display_name); free(p->description); free(p->signup_url);
    free_strv(p->env_vars);
    free(p->base_url); free(p->models_url); free(p->auth_type);
    free_strv(p->fallback_models);
    free(p->hostname);
    free(p->default_headers_json);
    free(p->default_aux_model);
    free(p->hook_state);
    free(p);
}

/* ── base hook defaults ──────────────────────────────────────────────── */

/* Port of providers/base.py:_profile_user_agent(). */
static const char *profile_user_agent(void)
{
    static char ua[64];
    if (!ua[0]) snprintf(ua, sizeof(ua), "hermes-cli/%s", HERMES_VERSION);
    return ua;
}

/* Port of ProviderProfile.get_hostname(). */
char *provider_profile_get_hostname(provider_profile_t *p)
{
    if (!p) return NULL;
    if (p->hostname && p->hostname[0]) return xstrdup_pp(p->hostname);
    if (!p->base_url || !p->base_url[0]) return xstrdup_pp("");
    /* urlparse(base_url).hostname: skip scheme://, cut at / : @ */
    const char *s = strstr(p->base_url, "://");
    s = s ? s + 3 : p->base_url;
    /* strip userinfo@ */
    const char *at = strchr(s, '@');
    const char *slash = strchr(s, '/');
    if (at && (!slash || at < slash)) s = at + 1;
    size_t len = strcspn(s, ":/");
    char *host = malloc(len + 1);
    memcpy(host, s, len);
    host[len] = '\0';
    for (size_t i = 0; i < len; i++) host[i] = (char)tolower((unsigned char)host[i]);
    return host;
}

/* Port of ProviderProfile.get_max_tokens() default. */
static int profile_get_max_tokens_base(provider_profile_t *p, const char *model)
{
    (void)model;
    return p->default_max_tokens;
}

/* Port of ProviderProfile.fetch_models() default: GET the models endpoint,
 * Bearer auth when api_key given, forward default_headers, parse
 * {"data":[{"id":...}]} or bare [{"id":...}]. */
char *provider_profile_fetch_models_default(provider_profile_t *p,
                                            const char *api_key,
                                            const char *base_url,
                                            double timeout)
{
    if (!p) return NULL;
    const char *effective_base = (base_url && base_url[0]) ? base_url : p->base_url;
    char url[1024] = {0};
    if (p->models_url && p->models_url[0]) {
        snprintf(url, sizeof(url), "%s", p->models_url);
    } else {
        if (!effective_base || !effective_base[0]) return NULL;
        size_t blen = strlen(effective_base);
        while (blen && effective_base[blen-1] == '/') blen--;
        snprintf(url, sizeof(url), "%.*s/models", (int)blen, effective_base);
    }

    /* headers: Authorization, Accept, User-Agent + default_headers */
    char headers[2048];
    size_t off = 0;
    if (api_key && api_key[0])
        off += (size_t)snprintf(headers + off, sizeof(headers) - off,
                                "Authorization: Bearer %s\r\n", api_key);
    off += (size_t)snprintf(headers + off, sizeof(headers) - off,
                            "Accept: application/json\r\nUser-Agent: %s\r\n",
                            profile_user_agent());
    if (p->default_headers_json) {
        json_t *hdrs = json_parse(p->default_headers_json, NULL);
        if (hdrs && hdrs->type == JSON_OBJECT) {
            for (size_t i = 0; i < hdrs->c.count && off < sizeof(headers) - 8; i++) {
                json_t *v = hdrs->c.items[i];
                if (v && v->type == JSON_STRING)
                    off += (size_t)snprintf(headers + off, sizeof(headers) - off,
                                            "%s: %s\r\n", hdrs->c.keys[i], v->str_val);
            }
        }
        json_free(hdrs);
    }

    http_t *h = http_new(timeout > 0 ? (int)timeout : 8);
    if (!h) return NULL;
    http_resp_t *r = http_get(h, url, headers);
    char *result = NULL;
    if (r && r->status >= 200 && r->status < 300 && r->body) {
        json_t *doc = json_parse(r->body, NULL);
        if (doc) {
            const json_t *items = doc;
            if (doc->type == JSON_OBJECT) items = json_obj_get(doc, "data");
            if (items && items->type == JSON_ARRAY) {
                json_t *ids = json_array();
                for (size_t i = 0; i < json_len(items); i++) {
                    json_t *m = json_get(items, i);
                    if (m && m->type == JSON_OBJECT) {
                        const char *id = json_get_str(m, "id", NULL);
                        if (id) json_append(ids, json_string(id));
                    }
                }
                result = json_serialize(ids);
                json_free(ids);
            }
            json_free(doc);
        }
    }
    if (r) http_resp_free(r);
    http_free(h);
    return result;
}

/* ── chat_completions.py:_reasoning_config_for_model ─────────────────── */
/* PoP: profile_reasoning_config_for_model @ agent/transports/chat_completions.py:_reasoning_config_for_model */
char *profile_reasoning_config_for_model(const char *model,
                                         const char *reasoning_config_json)
{
    if (!reasoning_config_json) return NULL;
    json_t *rc = json_parse(reasoning_config_json, NULL);
    if (!rc) return NULL;
    if (rc->type != JSON_OBJECT) {
        char *out = json_serialize(rc);
        json_free(rc);
        return out;
    }
    /* gpt-5.6 + effort=ultra -> effort=max */
    char ml[256] = {0};
    if (model) {
        size_t n = strlen(model);
        if (n >= sizeof(ml)) n = sizeof(ml) - 1;
        for (size_t i = 0; i < n; i++) ml[i] = (char)tolower((unsigned char)model[i]);
    }
    const char *effort = json_get_str(rc, "effort", NULL);
    if (strstr(ml, "gpt-5.6") && effort && strcasecmp(effort, "ultra") == 0)
        json_set(rc, "effort", json_string("max"));
    char *out = json_serialize(rc);
    json_free(rc);
    return out;
}
