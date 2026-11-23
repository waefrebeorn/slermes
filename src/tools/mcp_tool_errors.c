/*
 * mcp_tool_errors.c — C home for upstream Hermes tools/mcp_tool_errors.py.
 *
 * PoP: w8_mcptoolerr_c679c5_resolve_identity_header @ tools/mcp_tool_errors.py:_resolve_identity_header
 * PoP: w8_mcptoolerr_c679c5_apply_identity_header @ tools/mcp_tool_errors.py:_apply_identity_header
 * PoP: w8_mcptoolerr_c679c5_validate_remote_mcp_url @ tools/mcp_tool_errors.py:_validate_remote_mcp_url
 * PoP: w8_mcptoolerr_c679c5_shown_url @ tools/mcp_tool_errors.py:_shown_url
 */
#include "noc_w8_mcptoolerr_c679c5.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}
static char *strip_ws(const char *s) {
    if (!s) return NULL;
    const unsigned char *a = (const unsigned char *)s;
    while (*a && is_ws(*a)) a++;
    const unsigned char *b = a + strlen((const char *)a);
    while (b > a && is_ws(b[-1])) b--;
    size_t n = (size_t)(b - a);
    char *out = (char *)malloc(n + 1);
    if (out) { memcpy(out, a, n); out[n] = '\0'; }
    return out;
}
static char *lower_copy(const char *s) {
    if (!s) return NULL;
    char *out = (char *)malloc(strlen(s) + 1);
    if (out) { size_t i; for (i = 0; s[i]; ++i) out[i] = (char)tolower((unsigned char)s[i]); out[i] = '\0'; }
    return out;
}

/* Resolve the static identity_header into [name, value]; NULL when invalid. */
static struct json_t *resolve_static(const struct json_t *config) {
    if (!config || config->type != JSON_OBJECT) return NULL;
    const struct json_t *raw = json_obj_get(config, "identity_header");
    if (!raw || raw->type != JSON_OBJECT) return NULL;
    const struct json_t *nv = json_obj_get(raw, "name");
    if (!nv || nv->type != JSON_STRING) return NULL;
    char *nm = strip_ws(nv->str_val);
    if (!nm || !*nm) { free(nm); return NULL; }
    const struct json_t *vf = json_obj_get(raw, "value_from");
    const char *vfs = (vf && vf->type == JSON_STRING) ? vf->str_val : "static";
    char *vfl = lower_copy(strip_ws(vfs));
    int ok = vfl && !strcmp(vfl, "static");
    free(vfl);
    if (!ok) { free(nm); return NULL; }
    const struct json_t *vv = json_obj_get(raw, "value");
    if (!vv || vv->type != JSON_STRING) { free(nm); return NULL; }
    char *vt = strip_ws(vv->str_val);
    if (!vt || !*vt) { free(nm); free(vt); return NULL; }
    free(vt);
    struct json_t *arr = json_array();
    json_append(arr, json_string(nm));   /* name.strip() */
    json_append(arr, json_string(vv->str_val));  /* value verbatim */
    free(nm);
    return arr;
}

struct json_t *w8_mcptoolerr_c679c5_resolve_identity_header(const char *server_name,
        const struct json_t *config) {
    (void)server_name;  /* only used in warning text */
    return resolve_static(config);
}

struct json_t *w8_mcptoolerr_c679c5_apply_identity_header(const char *server_name,
        const struct json_t *config, const struct json_t *headers) {
    (void)server_name;
    struct json_t *out = headers && headers->type == JSON_OBJECT ? json_copy(headers) : json_object();
    struct json_t *resolved = resolve_static(config);
    if (!resolved) return out;
    const char *name = json_get(resolved, 0)->str_val;
    const char *value = json_get(resolved, 1)->str_val;
    char *nl = lower_copy(name);
    int present = 0;
    for (size_t i = 0; i < json_object_size(out); ++i) {
        const char *k = json_object_get_key_at(out, i);
        char *kl = lower_copy(k ? k : "");
        if (kl && nl && !strcmp(kl, nl)) { present = 1; free(kl); break; }
        free(kl);
    }
    if (!present) json_set(out, name, json_string(value));
    free(nl);
    json_free(resolved);
    return out;
}

static const char *py_type_name(const struct json_t *v) {
    if (!v) return "NoneType";
    switch (v->type) {
        case JSON_NULL: return "NoneType";
        case JSON_BOOL: return "bool";
        case JSON_NUMBER:
            return v->num_val == (double)(long long)v->num_val ? "int" : "float";
        case JSON_STRING: return "str";
        case JSON_ARRAY: return "list";
        case JSON_OBJECT: return "dict";
    }
    return "object";
}

static char *err_msg(const char *server_name, const char *detail) {
    char *m = (char *)malloc(strlen(server_name) + strlen(detail) + 64);
    if (m) sprintf(m, "Invalid MCP URL for '%s': %s", server_name, detail);
    return m;
}

struct json_t *w8_mcptoolerr_c679c5_validate_remote_mcp_url(const char *server_name,
        const struct json_t *url) {
    struct json_t *res = json_object();
    if (!url || url->type != JSON_STRING) {
        char d[128];
        sprintf(d, "expected a string, got %s", py_type_name(url));
        char *m = err_msg(server_name, d);
        json_set(res, "error", json_string(m ? m : d)); free(m); return res;
    }
    char *stripped = strip_ws(url->str_val);
    if (!stripped || !*stripped) {
        char *m = err_msg(server_name, "empty url");
        json_set(res, "error", json_string(m)); free(m); free(stripped); return res;
    }
    /* urlparse: scheme is [A-Za-z][A-Za-z0-9+.-]* before ':' */
    char scheme[64] = {0};
    const char *colon = strchr(stripped, ':');
    int scheme_ok = 0;
    if (colon) {
        size_t n = (size_t)(colon - stripped);
        if (n && n < sizeof(scheme) && isalpha((unsigned char)stripped[0])) {
            int all = 1;
            for (size_t i = 0; i < n; ++i) {
                unsigned char c = (unsigned char)stripped[i];
                if (!(isalnum(c) || c == '+' || c == '-' || c == '.')) { all = 0; break; }
            }
            if (all) { memcpy(scheme, stripped, n); scheme[n] = '\0'; scheme_ok = 1; }
        }
    }
    char *sl = lower_copy(scheme_ok ? scheme : "");
    if (!sl || (strcmp(sl, "http") && strcmp(sl, "https"))) {
        char d[512];
        sprintf(d, "scheme must be http or https, got '%s' ('%s')", scheme_ok ? scheme : "", stripped);
        char *m = err_msg(server_name, d);
        json_set(res, "error", json_string(m ? m : d)); free(m); free(sl); free(stripped); return res;
    }
    free(sl);
    /* netloc: after '://' up to first '/', '?' or '#' */
    const char *after = colon + 1;
    const char *net = NULL;
    size_t netlen = 0;
    if (*after == '/' && *(after + 1) == '/') {
        net = after + 2;
        const char *q = net;
        while (*q && *q != '/' && *q != '?' && *q != '#') q++;
        netlen = (size_t)(q - net);
    }
    if (!net || netlen == 0) {
        char d[512]; sprintf(d, "missing host ('%s')", stripped);
        char *m = err_msg(server_name, d);
        json_set(res, "error", json_string(m)); free(m); free(stripped); return res;
    }
    /* hostname: netloc after optional userinfo, strip port; empty for ":8080" */
    const char *host = net;
    const char *at = memchr(net, '@', netlen);
    if (at) { host = at + 1; netlen -= (size_t)(at + 1 - net); }
    int host_empty = 1;
    if (netlen && *host != ':') host_empty = 0;
    if (host_empty) {
        char d[512]; sprintf(d, "missing hostname ('%s')", stripped);
        char *m = err_msg(server_name, d);
        json_set(res, "error", json_string(m)); free(m); free(stripped); return res;
    }
    json_set(res, "ok", json_string(stripped));
    free(stripped);
    return res;
}

static int sensitive_param(const char *name) {
    if (!name || !*name) return 0;
    static const char *const names[] = {
        "access_token","api_key","apikey","auth","client_secret","code","id_token",
        "jwt","key","password","refresh_token","secret","session","sig","signature",
        "token","usertoken","x_amz_signature","x_google_signature"
    };
    char *c = lower_copy(name);
    for (size_t i = 0; c && i < sizeof(names) / sizeof(names[0]); ++i)
        if (!strcmp(c, names[i])) { free(c); return 1; }
    if (c) {
        /* Affixed names: pageToken, x-api-key, ... */
        const char *hits[] = {"token","key","secret","password","passwd","signature","sig","auth","session","jwt"};
        for (size_t i = 0; i < sizeof(hits) / sizeof(hits[0]); ++i)
            if (strstr(c, hits[i])) { free(c); return 1; }
    }
    free(c);
    return 0;
}

char *w8_mcptoolerr_c679c5_shown_url(const char *url) {
    if (!url) return NULL;
    /* Build result incrementally into a buffer. */
    size_t cap = strlen(url) * 4 + 64;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    size_t o = 0;
    const char *p = url;
    const char *scheme = strstr(url, "://");
    int did_userinfo = 0;
    while (*p) {
        /* userinfo: scheme://...@ -> scheme://***:***@ (before first '/' after authority). */
        if (!did_userinfo && scheme && p == scheme + 3) {
            const char *at = strchr(p, '@');
            const char *slash = strchr(p, '/');
            const char *q = url;
            if (at && (!slash || at < slash)) {
                o += (size_t)sprintf(out + o, "***:***@");
                p = at + 1;
                did_userinfo = 1;
                (void)q;
                continue;
            }
            did_userinfo = 1;
        }
        /* query param: [?&]name=value -> mask value when name is sensitive. */
        if (*p == '?' || *p == '&') {
            char sep = *p;
            const char *ns = p + 1;
            const char *eq = strchr(ns, '=');
            const char *end = eq ? eq + 1 : ns;
            const char *stop = end;
            while (*stop && *stop != '&' && *stop != '#') stop++;
            if (eq) {
                size_t nlen = (size_t)(eq - ns);
                char *nm = (char *)malloc(nlen + 1);
                if (nm) {
                    memcpy(nm, ns, nlen); nm[nlen] = '\0';
                    if (sensitive_param(nm)) {
                        out[o++] = sep;
                        o += (size_t)sprintf(out + o, "%s=***", nm);
                        p = stop;
                        free(nm);
                        continue;
                    }
                    free(nm);
                }
            }
        }
        out[o++] = *p++;
    }
    out[o] = '\0';
    return out;
}
