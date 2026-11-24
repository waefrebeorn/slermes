
/*
 * onepassword.c - real C11 port of the pure helpers of upstream Hermes
 * agent/vault_backends/onepassword.py (origin normalization, the `op item get`
 * selector, and the op child environment). The `op` CLI calls stay in Python.
 * PoP: w19_onepasswor_6beff4_all_origins @ agent/vault_backends/onepassword.py:_all_origins
 * PoP: w19_onepasswor_6beff4_web_origins @ agent/vault_backends/onepassword.py:_web_origins
 * PoP: w19_onepasswor_6beff4_item_selector @ agent/vault_backends/onepassword.py:OnePasswordLoginBackend._item_selector
 * PoP: w19_onepasswor_6beff4_env @ agent/vault_backends/onepassword.py:OnePasswordLoginBackend._env
 */
#include "hermes_json.h"
#include "noc_w19_onepasswor_6beff4.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* _OP_ENV_ALLOWLIST of agent/secret_sources/onepassword.py (the launch env names op may see). */
static const char *const OP_ENV_ALLOWLIST[] = {
    "PATH", "HOME", "USERPROFILE", "APPDATA", "LOCALAPPDATA", "SystemRoot",
    "TMPDIR", "TMP", "TEMP", "XDG_CONFIG_HOME", "XDG_RUNTIME_DIR", "OP_CONFIG_DIR",
    "OP_ACCOUNT", "OP_CONNECT_HOST", "OP_CONNECT_TOKEN", "OP_LOAD_DESKTOP_APP_SETTINGS",
    NULL
};

static int is_space(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* str.strip() of a NUL-terminated string. */
static void strip_copy(const char *in, char *out, size_t cap) {
    const char *s = in ? in : "";
    const char *e = s + strlen(s);
    while (*s && is_space((unsigned char)*s)) s++;
    while (e > s && is_space((unsigned char)e[-1])) e--;
    size_t n = (size_t)(e - s);
    if (n >= cap) n = cap - 1;
    memcpy(out, s, n);
    out[n] = '\0';
}

static void lower_into(const char *src, size_t len, char *out, size_t cap) {
    size_t n = len < cap - 1 ? len : cap - 1;
    for (size_t i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)src[i]);
    out[n] = '\0';
}

/* normalize_origin of agent/vault_store.py: scheme://host[:port], default ports dropped.
 * Returns 1 on success, 0 where the live code raises VaultError. */
static int normalize_origin(const char *url, char *out, size_t cap) {
    char tmp[2048];
    strip_copy(url, tmp, sizeof(tmp));
    if (!tmp[0]) return 0;                          /* origin is required */
    const char *sep = strstr(tmp, "://");
    if (!sep) return 0;                             /* origin must include a scheme */
    char scheme[64];
    lower_into(tmp, (size_t)(sep - tmp), scheme, sizeof(scheme));
    if (!scheme[0]) return 0;
    const char *rest = sep + 3;
    size_t alen = strcspn(rest, "/?#");
    const char *authority = rest;
    const char *at = NULL;
    for (size_t i = 0; i < alen; i++) if (authority[i] == '@') at = authority + i;
    if (at) { alen -= (size_t)(at + 1 - authority); authority = at + 1; }

    char host[256];
    int have_port = 0;
    long port = 0;
    if (alen && authority[0] == '[') {
        const char *close = memchr(authority, ']', alen);
        if (!close) return 0;
        lower_into(authority + 1, (size_t)(close - authority - 1), host, sizeof(host));
        if ((size_t)(close + 1 - authority) < alen && close[1] == ':') {
            const char *ps = close + 2;
            const char *pe = authority + alen;
            if (ps == pe) return 0;
            for (const char *q = ps; q < pe; q++) if (!isdigit((unsigned char)*q)) return 0;
            port = strtol(ps, NULL, 10);
            have_port = 1;
        }
    } else {
        const char *colon = NULL;
        for (size_t i = 0; i < alen; i++) if (authority[i] == ':') colon = authority + i;
        size_t hlen = colon ? (size_t)(colon - authority) : alen;
        lower_into(authority, hlen, host, sizeof(host));
        if (colon) {
            const char *ps = colon + 1;
            const char *pe = authority + alen;
            if (ps == pe) return 0;
            for (const char *q = ps; q < pe; q++) if (!isdigit((unsigned char)*q)) return 0;
            port = strtol(ps, NULL, 10);
            have_port = 1;
        }
    }
    if (!host[0]) return 0;                         /* could not parse an origin */
    int is_http = !strcmp(scheme, "http");
    int is_https = !strcmp(scheme, "https");
    long default_port = is_http ? 80 : (is_https ? 443 : -1);
    if (!have_port || port == default_port) snprintf(out, cap, "%s://%s", scheme, host);
    else snprintf(out, cap, "%s://%s:%ld", scheme, host, port);
    return 1;
}

json_t *w19_onepasswor_6beff4_all_origins(json_t *urls) {
    json_t *out = json_array();
    if (!urls || urls->type != JSON_ARRAY) return out;
    for (size_t i = 0; i < json_len(urls); i++) {
        json_t *u = json_get(urls, i);
        if (!u || u->type != JSON_STRING) continue;      /* except Exception: continue */
        char origin[2200];
        if (!normalize_origin(u->str_val, origin, sizeof(origin))) continue;
        int seen = 0;
        for (size_t j = 0; j < json_len(out); j++) {
            json_t *have = json_get(out, j);
            if (have && have->type == JSON_STRING && !strcmp(have->str_val, origin)) { seen = 1; break; }
        }
        if (!seen) json_append(out, json_string(origin));
    }
    return out;
}

json_t *w19_onepasswor_6beff4_web_origins(json_t *origins) {
    json_t *web = json_array();
    if (origins && origins->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(origins); i++) {
            json_t *o = json_get(origins, i);
            if (!o || o->type != JSON_STRING || !o->str_val) continue;
            if (!strncmp(o->str_val, "http://", 7) || !strncmp(o->str_val, "https://", 8))
                json_append(web, json_string(o->str_val));
        }
        if (json_len(web) == 0 && json_len(origins) > 0) {   /* web or (origins[0],) */
            json_t *first = json_get(origins, 0);
            json_append(web, first ? json_copy(first) : json_null());
        }
    }
    return web;
}

json_t *w19_onepasswor_6beff4_item_selector(const char *prefix, const char *handle,
                                           json_t *item_vaults, json_t *listed_vaults) {
    json_t *out = json_object();
    const char *h = handle ? handle : "";
    const char *p = prefix ? prefix : "";
    size_t plen = strlen(p);
    const char *item_id = strlen(h) >= plen ? h + plen : "";
    json_t *map = (item_vaults && item_vaults->type == JSON_OBJECT) ? item_vaults : NULL;
    json_t *found = map ? json_obj_get(map, item_id) : NULL;
    if (!found) {
        /* not listed yet: list_items() replaces the item -> vault map wholesale */
        map = (listed_vaults && listed_vaults->type == JSON_OBJECT) ? listed_vaults : NULL;
        found = map ? json_obj_get(map, item_id) : NULL;
    }
    if (!found) {
        json_set(out, "error", json_string("1Password login not found; list logins again"));
        return out;
    }
    json_t *args = json_array();
    json_append(args, json_string(item_id));
    if (found->type == JSON_STRING && found->str_val && found->str_val[0]) {
        json_append(args, json_string("--vault"));
        json_append(args, json_string(found->str_val));
    }
    json_set(out, "args", args);
    return out;
}

json_t *w19_onepasswor_6beff4_env(json_t *cfg, const char *service_token,
                                 const char *session_token, json_t *env_map, json_t *secrets) {
    json_t *out = json_object();
    for (int i = 0; OP_ENV_ALLOWLIST[i]; i++) {
        const char *k = OP_ENV_ALLOWLIST[i];
        if (!strncmp(k, "OP_CONNECT_", 11)) continue;   /* never from the launch environment */
        json_t *v = (env_map && env_map->type == JSON_OBJECT) ? json_obj_get(env_map, k) : NULL;
        if (v && v->type == JSON_STRING) json_set(out, k, json_string(v->str_val));
    }
    const char *connect[] = { "OP_CONNECT_HOST", "OP_CONNECT_TOKEN" };
    for (int i = 0; i < 2; i++) {
        json_t *v = (secrets && secrets->type == JSON_OBJECT) ? json_obj_get(secrets, connect[i]) : NULL;
        if (v && v->type == JSON_STRING && v->str_val && v->str_val[0])
            json_set(out, connect[i], json_string(v->str_val));
    }
    json_set(out, "NO_COLOR", json_string("1"));
    json_t *account = (cfg && cfg->type == JSON_OBJECT) ? json_obj_get(cfg, "account") : NULL;
    const char *acct = (account && account->type == JSON_STRING) ? account->str_val : "";
    if (acct && acct[0]) json_set(out, "OP_ACCOUNT", json_string(acct));
    if (service_token && service_token[0]) {
        json_set(out, "OP_SERVICE_ACCOUNT_TOKEN", json_string(service_token));
    } else if (session_token && session_token[0]) {
        char key[600];
        if (acct && acct[0]) snprintf(key, sizeof(key), "OP_SESSION_%s", acct);
        else snprintf(key, sizeof(key), "OP_SESSION");
        json_set(out, key, json_string(session_token));
    }
    return out;
}
