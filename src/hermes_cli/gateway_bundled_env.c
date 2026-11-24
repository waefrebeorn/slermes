/*
 * gateway_bundled_env.c - C home for upstream Hermes hermes_cli/gateway_bundled_env.py.
 *
 * PoP: w18_gatewaybun_c3a152_env_pairs @ hermes_cli/gateway_bundled_env.py:bundled_resource_env_pairs
 * PoP: w18_gatewaybun_c3a152_systemd_env_line @ hermes_cli/gateway_bundled_env.py:systemd_env_line
 * PoP: w18_gatewaybun_c3a152_systemd_bundled_env_block @ hermes_cli/gateway_bundled_env.py:systemd_bundled_env_block
 * PoP: w18_gatewaybun_c3a152_launchd_bundled_env_xml @ hermes_cli/gateway_bundled_env.py:launchd_bundled_env_xml
 *
 * The live wrapper env and the on-disk unit/plist values are injected as
 * explicit seams (the Windows CRT cannot hold Python's empty-string env).
 * Escaping order matches Python's chained str.replace calls exactly.
 */
#include "noc_w18_gatewaybun_c3a152.h"

#include "hermes_json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const GB_ENV_VARS[] = {
    "HERMES_BUNDLED_PLUGINS",
    "HERMES_BUNDLED_SKILLS",
    "HERMES_BUNDLED_LOCALES",
    "HERMES_OPTIONAL_SKILLS",
    "HERMES_TUI_DIR",
};
static const size_t GB_N = sizeof(GB_ENV_VARS) / sizeof(GB_ENV_VARS[0]);

/* str.strip() -> owned copy (ASCII whitespace, as used in the fixture values). */
static char *gb_strip(const char *s) {
    if (!s) {
        char *e = (char *)malloc(1);
        if (e) e[0] = '\0';
        return e;
    }
    const char *a = s;
    while (*a && isspace((unsigned char)*a)) a++;
    const char *b = a + strlen(a);
    while (b > a && isspace((unsigned char)b[-1])) b--;
    size_t n = (size_t)(b - a);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, a, n);
    out[n] = '\0';
    return out;
}

struct json_t *w18_gatewaybun_c3a152_env_pairs(const struct json_t *prior,
                                                const struct json_t *env) {
    json_t *out = json_array();
    for (size_t i = 0; i < GB_N; ++i) {
        const char *nm = GB_ENV_VARS[i];
        const json_t *ev = (env && env->type == JSON_OBJECT) ? json_obj_get(env, nm) : NULL;
        char *v = (ev && ev->type == JSON_STRING) ? gb_strip(ev->str_val) : NULL;
        if (!v || !v[0]) {
            free(v);
            v = NULL;
            const json_t *pv = (prior && prior->type == JSON_OBJECT) ? json_obj_get(prior, nm) : NULL;
            if (pv && pv->type == JSON_STRING) v = gb_strip(pv->str_val);
        }
        if (v && v[0]) {
            json_t *pair = json_array();
            json_append(pair, json_string(nm));
            json_append(pair, json_string(v));
            json_append(out, pair);
        }
        free(v);
    }
    return out;
}

struct json_t *w18_gatewaybun_c3a152_systemd_env_line(const char *name, const char *value) {
    if (!name) name = "";
    if (!value) value = "";
    size_t cap = strlen(name) + strlen(value) * 2 + 32;
    char *buf = (char *)malloc(cap);
    if (!buf) return json_string("");
    size_t n = (size_t)snprintf(buf, cap, "Environment=\"%s=", name);
    if (n > cap - 1) n = cap - 1;
    for (const char *p = value; *p; ++p) {
        char c = *p;
        if (n + 2 >= cap) break;
        if (c == '\\') { buf[n++] = '\\'; buf[n++] = '\\'; }
        else if (c == '"') { buf[n++] = '\\'; buf[n++] = '"'; }
        else if (c == '%') { buf[n++] = '%'; buf[n++] = '%'; }
        else buf[n++] = c;
    }
    if (n + 2 < cap) { buf[n++] = '"'; buf[n++] = '\n'; }
    buf[n] = '\0';
    json_t *res = json_string(buf);
    free(buf);
    return res;
}

typedef struct { char *p; size_t cap, len; } gb_buf;

static void gb_app(gb_buf *b, const char *s, size_t n) {
    if (!n) return;
    if (b->len + n + 1 > b->cap) {
        size_t nc = b->cap ? b->cap : 256;
        while (nc < b->len + n + 1) nc *= 2;
        char *np = (char *)realloc(b->p, nc);
        if (!np) return;
        b->p = np;
        b->cap = nc;
    }
    memcpy(b->p + b->len, s, n);
    b->len += n;
    b->p[b->len] = '\0';
}

static void gb_xml_escape(gb_buf *b, const char *s) {
    for (; s && *s; ++s) {
        unsigned char c = (unsigned char)*s;
        if (c == '&') gb_app(b, "&amp;", 5);
        else if (c == '<') gb_app(b, "&lt;", 4);
        else if (c == '>') gb_app(b, "&gt;", 4);
        else gb_app(b, (const char *)&c, 1);
    }
}

static json_t *gb_finish(gb_buf *b) {
    if (!b->p) {
        b->p = (char *)malloc(1);
        if (b->p) b->p[0] = '\0';
    }
    json_t *res = json_string(b->p ? b->p : "");
    free(b->p);
    b->p = NULL;
    return res;
}

struct json_t *w18_gatewaybun_c3a152_systemd_bundled_env_block(const struct json_t *prior,
                                                                const struct json_t *env) {
    json_t *pairs = w18_gatewaybun_c3a152_env_pairs(prior, env);
    gb_buf b = {0};
    for (size_t i = 0; i < json_len(pairs); ++i) {
        json_t *pair = json_get(pairs, i);
        const json_t *nv = json_get(pair, 0);
        const json_t *vv = json_get(pair, 1);
        const char *nm = (nv && nv->type == JSON_STRING) ? nv->str_val : "";
        const char *val = (vv && vv->type == JSON_STRING) ? vv->str_val : "";
        json_t *line = w18_gatewaybun_c3a152_systemd_env_line(nm, val);
        if (line && line->type == JSON_STRING && line->str_val)
            gb_app(&b, line->str_val, strlen(line->str_val));
        json_free(line);
    }
    json_free(pairs);
    return gb_finish(&b);
}

struct json_t *w18_gatewaybun_c3a152_launchd_bundled_env_xml(const struct json_t *prior,
                                                              const struct json_t *env) {
    json_t *pairs = w18_gatewaybun_c3a152_env_pairs(prior, env);
    gb_buf b = {0};
    for (size_t i = 0; i < json_len(pairs); ++i) {
        json_t *pair = json_get(pairs, i);
        const json_t *nv = json_get(pair, 0);
        const json_t *vv = json_get(pair, 1);
        const char *nm = (nv && nv->type == JSON_STRING) ? nv->str_val : "";
        const char *val = (vv && vv->type == JSON_STRING) ? vv->str_val : "";
        static const char K1[] = "\n        <key>";
        static const char K2[] = "</key>\n        <string>";
        static const char K3[] = "</string>";
        gb_app(&b, K1, sizeof(K1) - 1);
        gb_app(&b, nm, strlen(nm));
        gb_app(&b, K2, sizeof(K2) - 1);
        gb_xml_escape(&b, val);
        gb_app(&b, K3, sizeof(K3) - 1);
    }
    json_free(pairs);
    return gb_finish(&b);
}
