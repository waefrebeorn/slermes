/*
 * models_local.c — C port of upstream Hermes hermes_cli/models_local.py.
 *
 * PoP: w9_modelslocal_6756db_normalize_openai_base_url @ hermes_cli/models_local.py:_normalize_openai_base_url
 * PoP: w9_modelslocal_6756db_root_for_ollama_native_api @ hermes_cli/models_local.py:_root_for_ollama_native_api
 * PoP: w9_modelslocal_6756db_ollama_host_from_env @ hermes_cli/models_local.py:_ollama_host_from_env
 * PoP: w9_modelslocal_6756db_parse_ollama_tags @ hermes_cli/models_local.py:_parse_ollama_tags
 * PoP: w9_modelslocal_6756db_strip_ollama_cloud_suffix @ hermes_cli/models_local.py:_strip_ollama_cloud_suffix
 * PoP: w9_modelslocal_6756db_positive_int @ hermes_cli/models_local.py:_positive_int
 *
 * Faithful ports of the dependency-free string/collection helpers used by
 * Ollama and LM Studio endpoint resolution.
 */
#include "noc_w9_modelslocal_6756db.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static int is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* Python str.strip() over the ASCII whitespace class. */
static void trim_copy(const char *src, char *dst, size_t cap) {
    const char *p = src ? src : "";
    while (*p && is_ws((unsigned char)*p)) p++;
    size_t n = 0, last = 0;
    for (; *p && n + 1 < cap; p++) {
        dst[n++] = *p;
        if (!is_ws((unsigned char)*p)) last = n;
    }
    dst[n] = '\0';
    dst[last] = '\0';
}

static void rstrip_slashes(char *s) {
    size_t n = strlen(s);
    while (n > 0 && s[n - 1] == '/') s[--n] = '\0';
}

static int ends_with(const char *s, const char *suffix) {
    size_t a = strlen(s), b = strlen(suffix);
    return a >= b && strcmp(s + a - b, suffix) == 0;
}

static int has_scheme_sep(const char *s) { return strstr(s, "://") != NULL; }

struct json_t *w9_modelslocal_6756db_normalize_openai_base_url(const char *base_url) {
    char val[8192];
    trim_copy(base_url, val, sizeof(val));
    char out[8200];
    if (val[0] == ':')
        snprintf(out, sizeof(out), "http://127.0.0.1%s", val);
    else if (val[0] && !has_scheme_sep(val))
        snprintf(out, sizeof(out), "http://%s", val);
    else
        snprintf(out, sizeof(out), "%s", val);
    return json_string(out);
}

struct json_t *w9_modelslocal_6756db_root_for_ollama_native_api(const char *base_url) {
    char root[8192];
    trim_copy(base_url, root, sizeof(root));
    rstrip_slashes(root);
    char tmp[8200];
    if (root[0] == ':')
        snprintf(tmp, sizeof(tmp), "http://127.0.0.1%s", root);
    else if (root[0] && !has_scheme_sep(root))
        snprintf(tmp, sizeof(tmp), "http://%s", root);
    else
        snprintf(tmp, sizeof(tmp), "%s", root);

    /* _strip_suffixes returns after the first matching suffix. */
    static const char *suffixes[] = {"/api/tags", "/v1/models", "/api", "/v1", NULL};
    for (int i = 0; suffixes[i]; i++) {
        if (ends_with(tmp, suffixes[i])) {
            tmp[strlen(tmp) - strlen(suffixes[i])] = '\0';
            rstrip_slashes(tmp);
            break;
        }
    }
    return json_string(tmp);
}

struct json_t *w9_modelslocal_6756db_ollama_host_from_env(const char *env_host) {
    const char *e = env_host ? env_host : "";
    char out[8192];

    if (e[0] == ':' && !(e[0] == ':' && e[1] == ':')) {
        snprintf(out, sizeof(out), "127.0.0.1%s", e);
        return json_string(out);
    }
    size_t len = strlen(e);
    if (len >= 2 && e[0] == '[' && e[len - 1] == ']') {
        snprintf(out, sizeof(out), "%s:11434", e);
        return json_string(out);
    }

    const char *proto = strstr(e, "://");
    if (proto) {
        size_t scheme_len = (size_t)(proto - e);
        const char *netloc = proto + 3;
        const char *end = netloc;
        while (*end && *end != '/' && *end != '?' && *end != '#') end++;
        const char *at = NULL;
        for (const char *p = netloc; p < end; p++) if (*p == '@') at = p;
        const char *hostport = at ? at + 1 : netloc;
        size_t hp_len = at ? (size_t)(end - (at + 1)) : (size_t)(end - netloc);

        int has_port = 0;
        char hostname[2048] = "";
        if (hp_len > 0 && hostport[0] == '[') {
            const char *rb = NULL;
            for (const char *p = hostport; p < end; p++) if (*p == ']') { rb = p; break; }
            if (!rb) return json_string(e); /* urlsplit ValueError -> unchanged */
            size_t hl = (size_t)(rb - (hostport + 1));
            if (hl >= sizeof(hostname)) return json_string(e);
            memcpy(hostname, hostport + 1, hl);
            hostname[hl] = '\0';
            const char *after = rb + 1;
            if (after < end && *after == ':') has_port = 1;
        } else {
            const char *colon = NULL;
            for (const char *p = hostport; p < end; p++) if (*p == ':') colon = p;
            size_t hl;
            if (colon) { has_port = 1; hl = (size_t)(colon - hostport); }
            else hl = hp_len;
            if (hl >= sizeof(hostname)) return json_string(e);
            memcpy(hostname, hostport, hl);
            hostname[hl] = '\0';
        }
        if (!hostname[0] || has_port) return json_string(e);

        char userinfo[2048] = "";
        if (at) {
            size_t ul = (size_t)(at - netloc) + 1; /* include the '@' */
            if (ul >= sizeof(userinfo)) return json_string(e);
            memcpy(userinfo, netloc, ul);
            userinfo[ul] = '\0';
        }
        for (char *q = hostname; *q; q++) *q = (char)tolower((unsigned char)*q);
        char hbuf[2100];
        if (strchr(hostname, ':') && hostname[0] != '[')
            snprintf(hbuf, sizeof(hbuf), "[%s]", hostname);
        else
            snprintf(hbuf, sizeof(hbuf), "%s", hostname);
        snprintf(out, sizeof(out), "%.*s://%s%s:11434%s",
                 (int)scheme_len, e, userinfo, hbuf, end);
        return json_string(out);
    }

    int colons = 0;
    for (const char *p = e; *p; p++) if (*p == ':') colons++;
    if (colons > 1 && e[0] != '[') {
        snprintf(out, sizeof(out), "[%s]:11434", e);
        return json_string(out);
    }
    if (colons == 0) {
        snprintf(out, sizeof(out), "%s:11434", e);
        return json_string(out);
    }
    return json_string(e);
}

/* str(item.get("model") or item.get("name") or "").strip() for string values. */
static const char *nonempty_str(const json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    const json_t *n = json_obj_get(obj, key);
    if (!n || n->type != JSON_STRING || !n->str_val || !n->str_val[0]) return NULL;
    return n->str_val;
}

struct json_t *w9_modelslocal_6756db_parse_ollama_tags(const json_t *payload) {
    if (!payload || payload->type != JSON_OBJECT) return json_null();
    const json_t *raw = json_obj_get(payload, "models");
    if (!raw || raw->type != JSON_ARRAY) return json_null();

    json_t *models = json_array();
    size_t added = 0, total = json_len(raw);
    for (size_t i = 0; i < total; i++) {
        const json_t *item = json_get(raw, i);
        if (!item || item->type != JSON_OBJECT) { json_free(models); return json_null(); }
        const char *m = nonempty_str(item, "model");
        if (!m) m = nonempty_str(item, "name");
        char mid[4096];
        if (m) trim_copy(m, mid, sizeof(mid)); else mid[0] = '\0';
        if (!mid[0]) continue;
        int dup = 0;
        for (size_t j = 0; j < json_len(models); j++) {
            const char *prior = json_string_value(json_get(models, j));
            if (prior && !strcmp(prior, mid)) { dup = 1; break; }
        }
        if (!dup) { json_append(models, json_string(mid)); added++; }
    }
    if (total > 0 && added == 0) { json_free(models); return json_null(); }
    return models;
}

struct json_t *w9_modelslocal_6756db_strip_ollama_cloud_suffix(const char *model_id) {
    char out[8192];
    snprintf(out, sizeof(out), "%s", model_id ? model_id : "");
    static const char *suffixes[] = {":cloud", "-cloud", NULL};
    for (int i = 0; suffixes[i]; i++) {
        if (ends_with(out, suffixes[i])) {
            out[strlen(out) - strlen(suffixes[i])] = '\0';
            break;
        }
    }
    return json_string(out);
}

struct json_t *w9_modelslocal_6756db_positive_int(const json_t *value) {
    if (!value || value->type != JSON_NUMBER) return json_null();
    double v = value->num_val;
    if (v > 0 && v == (double)(long long)v) return json_number(v);
    return json_null();
}
