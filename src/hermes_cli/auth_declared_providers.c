/*
 * auth_declared_providers.c -- real C11 port of the credential/base-URL
 * resolution for API-key providers, upstream
 * hermes_cli/auth_declared_providers.py.
 *
 * PoP: w21_authdeclar_1a88fd_default_api_key_base_url @ hermes_cli/auth_declared_providers.py:default_api_key_base_url
 * PoP: w21_authdeclar_1a88fd_origin @ utils/urls.py:base_url_origin
 * PoP: w21_authdeclar_1a88fd_relay_probe_api_key @ hermes_cli/auth_declared_providers.py:relay_probe_api_key
 * PoP: w21_authdeclar_1a88fd_resolve_declared @ hermes_cli/auth_declared_providers.py:resolve_declared_api_key_provider
 * PoP: w21_authdeclar_1a88fd_custom_catalog @ hermes_cli/auth_declared_providers.py:custom_catalog_probe_api_key
 *
 * Ported in full here:
 *   find_provider_entry / is_provider_enabled   (hermes_cli/config_providers.py)
 *   ${VAR} / ${env:VAR} expansion               (hermes_cli/config.py)
 *   base_url_origin                             (utils/urls.py)
 *   has_usable_secret                           (hermes_cli/auth.py)
 * The config file read, the dotenv read, key_cmd minting and the owning-entry
 * credential lookup stay upstream and arrive as seams.
 *
 * ASCII source: the module carries no literal text.
 */
#include "hermes_json.h"
#include "noc_w21_authdeclar_1a88fd.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AD_INVALID_MSG " is not an API-key provider."

static char *ad_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

/* str.strip() */
static char *ad_strip(const char *s) {
    const char *b = s ? s : "";
    const char *e;
    size_t n;
    char *out;
    while (*b && isspace((unsigned char)*b)) b++;
    e = b + strlen(b);
    while (e > b && isspace((unsigned char)e[-1])) e--;
    n = (size_t)(e - b);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, b, n);
    out[n] = '\0';
    return out;
}

/* str.rstrip("/") */
static void ad_rstrip_slash(char *s) {
    size_t n;
    if (!s) return;
    n = strlen(s);
    while (n > 0 && s[n - 1] == '/') s[--n] = '\0';
}

/* Python repr of a float: shortest round-tripping decimal. */
static void ad_fmt_double(double d, char *buf, size_t cap) {
    int prec;
    for (prec = 15; prec <= 17; prec++) {
        double back;
        snprintf(buf, cap, "%.*g", prec, d);
        back = strtod(buf, NULL);
        if (back == d) return;
    }
}

/* Python str() for the values a config field can carry. */
static char *ad_py_str(const json_t *v) {
    char buf[80];
    if (!v || v->type == JSON_NULL) return ad_dup("None");
    switch (v->type) {
    case JSON_BOOL:   return ad_dup(v->bool_val ? "True" : "False");
    case JSON_STRING: return ad_dup(v->str_val ? v->str_val : "");
    case JSON_NUMBER:
        if (isfinite(v->num_val) && floor(v->num_val) == v->num_val &&
            v->num_val >= -9.007199254740992e15 && v->num_val <= 9.007199254740992e15) {
            snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        } else {
            ad_fmt_double(v->num_val, buf, sizeof(buf));
        }
        return ad_dup(buf);
    default: {
        char *dumped = json_dumps(v, 0);
        char *out = ad_dup(dumped ? dumped : "");
        free(dumped);
        return out;
    }
    }
}

static int ad_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

static void ad_lower(char *s) {
    if (!s) return;
    for (; *s; s++) *s = (char)tolower((unsigned char)*s);
}

/* --- ${VAR} / ${env:VAR} expansion -------------------------------------- */
static int ad_is_non_env_ref(const char *ref) {
    const char *p = ref;
    if (!strchr(ref, ':')) return 0;
    if (!isalpha((unsigned char)*p)) return 0;
    p++;
    while (*p && (isalnum((unsigned char)*p) || *p == '_' || *p == '-')) p++;
    return *p == ':' && p != ref;
}

static const char *ad_env_get(const json_t *env, const char *name) {
    json_t *v;
    if (!env || env->type != JSON_OBJECT || !name) return NULL;
    v = json_obj_get(env, name);
    if (!v || v->type == JSON_NULL) return NULL;
    return (v->type == JSON_STRING) ? v->str_val : NULL;
}

/* One `${...}` body -> the variable name it reads, or NULL to keep it verbatim. */
static char *ad_ref_name(const char *inner) {
    char *trimmed = ad_strip(inner);
    char *name;
    if (!trimmed) return NULL;
    if (!strncmp(trimmed, "env:", 4)) {
        name = ad_strip(trimmed + 4);
        free(trimmed);
        if (name && !name[0]) { free(name); return NULL; }
        return name;
    }
    if (ad_is_non_env_ref(trimmed)) { free(trimmed); return NULL; }
    return trimmed;
}

/* _ENV_REF_RE.sub(_env_expand_match, text) */
static char *ad_expand(const char *text, const json_t *env) {
    size_t cap, len = 0;
    char *out;
    const char *p = text ? text : "";
    if (!text) return ad_dup("");
    cap = strlen(text) + 1;
    out = (char *)malloc(cap);
    if (!out) return NULL;
    while (*p) {
        const char *close;
        if (p[0] == '$' && p[1] == '{' && (close = strchr(p + 2, '}')) != NULL) {
            size_t inner_n = (size_t)(close - (p + 2));
            char *inner = (char *)malloc(inner_n + 1);
            char *name;
            const char *val = NULL;
            if (!inner) break;
            memcpy(inner, p + 2, inner_n);
            inner[inner_n] = '\0';
            name = ad_ref_name(inner);
            free(inner);
            if (name) val = ad_env_get(env, name);
            if (val) {
                size_t vn = strlen(val);
                while (len + vn + 1 > cap) { cap *= 2; out = (char *)realloc(out, cap); }
                memcpy(out + len, val, vn);
                len += vn;
            } else {
                size_t rn = (size_t)(close + 1 - p);
                while (len + rn + 1 > cap) { cap *= 2; out = (char *)realloc(out, cap); }
                memcpy(out + len, p, rn);
                len += rn;
            }
            free(name);
            p = close + 1;
            continue;
        }
        if (len + 2 > cap) { cap *= 2; out = (char *)realloc(out, cap); }
        out[len++] = *p++;
    }
    out[len] = '\0';
    return out;
}

/* `str(entry.get(k1) or entry.get(k2) or ... or "")`, expanded and stripped. */
static char *ad_field(const json_t *obj, const char *const *keys, int nkeys, const json_t *env) {
    int i;
    char *raw = NULL, *expanded, *trimmed;
    for (i = 0; i < nkeys; i++) {
        json_t *v = json_obj_get(obj, keys[i]);
        if (ad_truthy(v)) { raw = ad_py_str(v); break; }
    }
    if (!raw) raw = ad_dup("");
    expanded = ad_expand(raw, env);
    free(raw);
    trimmed = ad_strip(expanded);
    free(expanded);
    return trimmed;
}

/* `str(obj.get(k1) or ... or "")` with strip only (config values arrive already
 * expanded, so no ref walk): the model_cfg shape. */
static char *ad_raw_field(const json_t *obj, const char *const *keys, int nkeys) {
    int i;
    for (i = 0; i < nkeys; i++) {
        json_t *v = json_obj_get(obj, keys[i]);
        if (ad_truthy(v)) {
            char *s = ad_py_str(v);
            char *trimmed = ad_strip(s);
            free(s);
            return trimmed;
        }
    }
    return ad_dup("");
}

/* --- has_usable_secret --------------------------------------------------- */
static const char *const AD_PLACEHOLDERS[] = {
    "*", "**", "***", "changeme", "your_api_key", "your_api_key_here", "your-api-key",
    "placeholder", "example", "dummy", "null", "none", NULL
};

static int ad_placeholder_shape(const char *value) {
    static const char *const PREFIXES[] = {"sk-", "ghp_", "hf_", NULL};
    char *lowered = ad_dup(value);
    size_t n, i, k;
    int all_x = 1, any = 0;
    if (!lowered) return 0;
    ad_lower(lowered);
    n = strlen(lowered);
    if (n >= 5 && !strncmp(lowered, "your_", 5) && !strcmp(lowered + n - 5, "_here")) {
        free(lowered);
        return 1;
    }
    for (i = 0; PREFIXES[i]; i++) {
        size_t pn = strlen(PREFIXES[i]);
        if (!strncmp(lowered, PREFIXES[i], pn)) {
            int all = 1, seen = 0;
            for (k = pn; k < n; k++) {
                seen = 1;
                if (lowered[k] != 'x') { all = 0; break; }
            }
            if (seen && all) { free(lowered); return 1; }
        }
    }
    for (i = 0; i < n; i++) {
        char c = lowered[i];
        if (c == ' ' || c == '-' || c == '_') continue;
        any = 1;
        if (c != 'x') { all_x = 0; break; }
    }
    free(lowered);
    return any && all_x;
}

static int ad_has_usable_secret(const char *value) {
    char *cleaned;
    size_t n, i;
    int usable = 1;
    if (!value) return 0;
    cleaned = ad_strip(value);
    if (!cleaned) return 0;
    n = strlen(cleaned);
    if (n < 4) { free(cleaned); return 0; }
    ad_lower(cleaned);
    for (i = 0; AD_PLACEHOLDERS[i]; i++) {
        if (!strcmp(cleaned, AD_PLACEHOLDERS[i])) { usable = 0; break; }
    }
    if (usable && ad_placeholder_shape(cleaned)) usable = 0;
    free(cleaned);
    return usable;
}

/* --- find_provider_entry / is_provider_enabled --------------------------- */
static json_t *ad_find_entry(const json_t *providers, const char *provider_id) {
    char *want;
    json_t *exact;
    size_t i, n;
    if (!providers || providers->type != JSON_OBJECT) return NULL;
    want = ad_strip(provider_id);
    if (!want || !want[0]) { free(want); return NULL; }
    exact = json_obj_get(providers, want);
    if (exact && exact->type == JSON_OBJECT) { free(want); return exact; }
    n = json_object_size(providers);
    for (i = 0; i < n; i++) {
        const char *k = json_object_get_key_at(providers, i);
        json_t *entry = json_object_get_at(providers, i);
        char *coerced;
        if (!k || !entry || entry->type != JSON_OBJECT) continue;
        coerced = ad_strip(k);
        if (coerced && !strcmp(coerced, want)) { free(coerced); free(want); return entry; }
        free(coerced);
    }
    free(want);
    return NULL;
}

static int ad_provider_enabled(const json_t *entry) {
    json_t *flag;
    static const char *const FALSE_WORDS[] = {"false", "0", "no", "off", NULL};
    int i;
    if (!entry || entry->type != JSON_OBJECT) return 1;
    flag = json_obj_get(entry, "enabled");
    if (!flag || flag->type == JSON_NULL) return 1;   /* .get("enabled", True) */
    if (flag->type == JSON_BOOL) return flag->bool_val ? 1 : 0;
    if (flag->type == JSON_STRING) {
        char *s = ad_strip(flag->str_val);
        int enabled = 1;
        ad_lower(s);
        for (i = 0; FALSE_WORDS[i]; i++) {
            if (!strcmp(s, FALSE_WORDS[i])) { enabled = 0; break; }
        }
        free(s);
        return enabled;
    }
    return ad_truthy(flag);
}

/* --- base_url_origin ----------------------------------------------------- */
typedef struct { char scheme[16]; char host[256]; long port; } ad_origin;

static void ad_parse_origin(const char *base_url, ad_origin *out) {
    char *raw, *work, *authority, *slash, *at, *colon;
    size_t n;
    memset(out, 0, sizeof(*out));
    raw = ad_strip(base_url);
    if (!raw) { free(raw); return; }
    work = raw;
    slash = strstr(work, "://");
    if (slash) {
        size_t sn = (size_t)(slash - work);
        if (sn >= sizeof(out->scheme)) sn = sizeof(out->scheme) - 1;
        memcpy(out->scheme, work, sn);
        out->scheme[sn] = '\0';
        ad_lower(out->scheme);
        work = slash + 3;
    }
    /* authority ends at the first '/', '?' or '#' */
    n = strcspn(work, "/?#");
    authority = (char *)malloc(n + 1);
    if (!authority) { free(raw); return; }
    memcpy(authority, work, n);
    authority[n] = '\0';
    at = strrchr(authority, '@');
    if (at) memmove(authority, at + 1, strlen(at + 1) + 1);
    colon = strrchr(authority, ':');
    if (colon) {
        char *endp = NULL;
        char *port_text = colon + 1;
        long port;
        if (!*port_text) { free(authority); free(raw); return; }
        port = strtol(port_text, &endp, 10);
        if (!endp || *endp) { free(authority); free(raw); return; }  /* ValueError */
        if (port < 0 || port > 65535) { free(authority); free(raw); return; }
        out->port = port;
        *colon = '\0';
    }
    if (!authority[0]) { free(authority); free(raw); return; }
    n = strlen(authority);
    if (n >= sizeof(out->host)) n = sizeof(out->host) - 1;
    memcpy(out->host, authority, n);
    out->host[n] = '\0';
    ad_lower(out->host);
    while (out->host[0] && out->host[strlen(out->host) - 1] == '.')
        out->host[strlen(out->host) - 1] = '\0';
    free(authority);
    free(raw);
}

json_t *w21_authdeclar_1a88fd_origin(const char *base_url) {
    ad_origin origin;
    json_t *out;
    long port;
    ad_parse_origin(base_url, &origin);
    out = json_object();
    if (!origin.host[0]) {
        json_set(out, "scheme", json_string(""));
        json_set(out, "host", json_string(""));
        json_set(out, "port", json_number(0));
        return out;
    }
    port = origin.port;
    if (!port) {
        if (!strcmp(origin.scheme, "https")) port = 443;
        else if (!strcmp(origin.scheme, "http")) port = 80;
    }
    json_set(out, "scheme", json_string(origin.scheme));
    json_set(out, "host", json_string(origin.host));
    json_set(out, "port", json_number((double)port));
    return out;
}

/* --- default_api_key_base_url ------------------------------------------- */
json_t *w21_authdeclar_1a88fd_default_api_key_base_url(const char *api_key,
                                                      const char *default_url,
                                                      const char *env_url) {
    char *value;
    (void)api_key;
    if (env_url && env_url[0]) {
        value = ad_dup(env_url);
        ad_rstrip_slash(value);
        {
            json_t *out = json_string(value);
            free(value);
            return out;
        }
    }
    /* `else default` -- the default is returned as-is, None included. */
    return default_url ? json_string(default_url) : json_null();
}

/* --- relay_probe_api_key ------------------------------------------------- */
json_t *w21_authdeclar_1a88fd_relay_probe_api_key(const char *provider_id,
                                                 const char *relay,
                                                 const char *api_key,
                                                 const char *cred_base_url,
                                                 int known_provider) {
    ad_origin a, b;
    (void)provider_id;
    if (!api_key || !api_key[0]) return json_string("");
    if (known_provider) return json_string(api_key);
    ad_parse_origin(relay, &a);
    ad_parse_origin(cred_base_url, &b);
    if (a.host[0] && !strcmp(a.scheme, b.scheme) && !strcmp(a.host, b.host) &&
        (a.port ? a.port : (!strcmp(a.scheme, "https") ? 443 : (!strcmp(a.scheme, "http") ? 80 : 0))) ==
        (b.port ? b.port : (!strcmp(b.scheme, "https") ? 443 : (!strcmp(b.scheme, "http") ? 80 : 0))))
        return json_string(api_key);
    return json_string("");
}

/* --- resolve_declared_api_key_provider ---------------------------------- */
static json_t *ad_auth_error(const char *provider_id) {
    json_t *out = json_object();
    size_t need = strlen(provider_id ? provider_id : "") + strlen(AD_INVALID_MSG) + 12;
    char *msg = (char *)malloc(need);
    if (msg) snprintf(msg, need, "Provider '%s'%s", provider_id ? provider_id : "", AD_INVALID_MSG);
    json_set(out, "status", json_string("error"));
    json_set(out, "type", json_string("AuthError"));
    json_set(out, "message", json_string(msg ? msg : ""));
    json_set(out, "provider", json_string(provider_id ? provider_id : ""));
    json_set(out, "code", json_string("invalid_provider"));
    free(msg);
    return out;
}

json_t *w21_authdeclar_1a88fd_resolve_declared(const char *provider_id,
                                              const json_t *config,
                                              const json_t *env,
                                              const json_t *key_cmd_values) {
    static const char *const URL_KEYS[] = {"base_url", "api", "url"};
    static const char *const KEY_ENV_KEYS[] = {"key_env", "api_key_env"};
    const json_t *providers = (config && config->type == JSON_OBJECT)
                                  ? json_obj_get(config, "providers") : NULL;
    json_t *entry = ad_find_entry(providers, provider_id);
    char *base_url, *api_key = NULL, *source = NULL;
    json_t *out, *value;

    if (!entry || !ad_provider_enabled(entry)) return ad_auth_error(provider_id);
    base_url = ad_field(entry, URL_KEYS, 3, env);
    ad_rstrip_slash(base_url);
    if (!base_url[0]) {
        free(base_url);
        return ad_auth_error(provider_id);
    }
    source = ad_dup("config");

    {
        static const char *const CMD_KEYS[] = {"key_cmd"};
        char *key_cmd = ad_field(entry, CMD_KEYS, 1, env);
        if (key_cmd[0]) {
            json_t *minted = (key_cmd_values && key_cmd_values->type == JSON_OBJECT)
                                 ? json_obj_get(key_cmd_values, key_cmd) : NULL;
            api_key = (minted && minted->type != JSON_NULL) ? ad_py_str(minted) : ad_dup("");
            if (api_key[0]) {
                free(source);
                source = ad_dup("key_cmd");
            }
        }
        free(key_cmd);
    }
    if (!api_key || !api_key[0]) {
        static const char *const INLINE_KEYS[] = {"api_key"};
        free(api_key);
        api_key = ad_field(entry, INLINE_KEYS, 1, env);
        if (api_key[0]) {
            free(source);
            source = ad_dup("api_key");
        }
    }
    if (!api_key[0]) {
        char *key_env = ad_field(entry, KEY_ENV_KEYS, 2, env);
        if (key_env[0]) {
            const char *value_text = ad_env_get(env, key_env);
            char *from_env = ad_strip(value_text ? value_text : "");
            if (from_env[0]) {
                free(api_key);
                api_key = from_env;
                free(source);
                source = ad_dup(key_env);
            } else {
                free(from_env);
            }
        }
        free(key_env);
    }

    value = json_object();
    json_set(value, "provider", json_string(provider_id ? provider_id : ""));
    json_set(value, "api_key", json_string(api_key));
    json_set(value, "base_url", json_string(base_url));
    json_set(value, "source", json_string(source));
    out = json_object();
    json_set(out, "status", json_string("ok"));
    json_set(out, "value", value);
    free(base_url);
    free(api_key);
    free(source);
    return out;
}

/* --- custom_catalog_probe_api_key --------------------------------------- */
/* `_model_cfg_key_env_for(model_cfg, base_url)`. */
static char *ad_model_cfg_key_env(const json_t *model_cfg, const char *base_url,
                                  const json_t *env) {
    static const char *const BASE_KEYS[] = {"base_url"};
    static const char *const ENV_KEYS[] = {"key_env", "api_key_env"};
    char *cfg_url = ad_raw_field(model_cfg, BASE_KEYS, 1);
    char *url = ad_strip(base_url ? base_url : "");
    char *result;
    ad_rstrip_slash(cfg_url);
    ad_rstrip_slash(url);
    if (!cfg_url[0] || strcmp(cfg_url, url)) {
        free(cfg_url);
        free(url);
        return ad_dup("");
    }
    {
        char *key_env = ad_raw_field(model_cfg, ENV_KEYS, 2);
        const char *value_text = key_env[0] ? ad_env_get(env, key_env) : NULL;
        result = ad_strip(value_text ? value_text : "");
        free(key_env);
    }
    free(cfg_url);
    free(url);
    return result;
}

json_t *w21_authdeclar_1a88fd_custom_catalog(const json_t *case_) {
    const json_t *model_cfg = (case_ && case_->type == JSON_OBJECT)
                                  ? json_obj_get(case_, "model_cfg") : NULL;
    const json_t *env = (case_ && case_->type == JSON_OBJECT)
                            ? json_obj_get(case_, "env") : NULL;
    const json_t *owning = (case_ && case_->type == JSON_OBJECT)
                               ? json_obj_get(case_, "owning") : NULL;
    const char *base_url = (case_ && case_->type == JSON_OBJECT)
                               ? json_get_str(case_, "base_url", "") : "";
    int minted = (case_ && case_->type == JSON_OBJECT) ? json_get_bool(case_, "minted", 1) : 1;
    char *cfg_url = ad_raw_field(model_cfg, (const char *const[]){ "base_url" }, 1);
    char *url = ad_strip(base_url);
    char *own = NULL;
    char *result = NULL;
    ad_rstrip_slash(cfg_url);
    ad_rstrip_slash(url);

    if (!strcmp(cfg_url, url)) {
        char *inline_key = ad_raw_field(model_cfg, (const char *const[]){ "api_key" }, 1);
        if (inline_key[0]) own = ad_dup(inline_key);
        free(inline_key);
    }
    if (!own) {
        /* `... or _model_cfg_key_env_for(model_cfg, base_url)` */
        own = ad_model_cfg_key_env(model_cfg, base_url, env);
    }
    if (ad_has_usable_secret(own)) {
        result = own;
    } else {
        free(own);
        if (owning && owning->type == JSON_OBJECT && json_get_bool(owning, "declared", 0)) {
            if (minted) {
                /* materialize_probe_api_key(entry_key): the mint map is keyed by
                 * the token source the owning entry returned. */
                json_t *entry_key = json_obj_get(owning, "key");
                char *source_id = ad_py_str(entry_key);
                json_t *mint_map = (case_ && case_->type == JSON_OBJECT)
                                       ? json_obj_get(case_, "key_cmd_values") : NULL;
                json_t *hit = (mint_map && mint_map->type == JSON_OBJECT)
                                  ? json_obj_get(mint_map, source_id) : NULL;
                result = (hit && hit->type != JSON_NULL) ? ad_py_str(hit) : ad_dup("");
                free(source_id);
            } else {
                json_t *entry_key = json_obj_get(owning, "key");
                result = ad_py_str(entry_key);
            }
        } else {
            /* candidates = [CUSTOM_API_KEY] + host-gated env keys */
            json_t *host_gated = (case_ && case_->type == JSON_OBJECT)
                                     ? json_obj_get(case_, "host_gated") : NULL;
            const char *custom = ad_env_get(env, "CUSTOM_API_KEY");
            char *first = ad_strip(custom ? custom : "");
            size_t i;
            result = ad_dup("");
            if (ad_has_usable_secret(first)) {
                free(result);
                result = first;
                first = NULL;
            }
            for (i = 0; first && i < json_len(host_gated); i++) {
                json_t *cand = json_get(host_gated, i);
                char *text = ad_py_str(cand);
                if (!result[0] && ad_has_usable_secret(text)) {
                    free(result);
                    result = text;
                    text = NULL;
                }
                free(text);
            }
            free(first);
        }
    }
    free(cfg_url);
    free(url);
    {
        json_t *out = json_string(result ? result : "");
        free(result);
        return out;
    }
}
