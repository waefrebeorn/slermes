/*
 * mcp_tool_config.c — C home for upstream Hermes tools/mcp_tool_config.py.
 *
 * PoP: w8_mcptoolcon_809435_pathext_suffixes @ tools/mcp_tool_config.py:_pathext_suffixes
 * PoP: w8_mcptoolcon_809435_warn_hidden_whitespace @ tools/mcp_tool_config.py:_warn_hidden_whitespace
 * PoP: w8_mcptoolcon_809435_require_rendered_remote @ tools/mcp_tool_config.py:_require_rendered_remote
 */
#include "noc_w8_mcptoolcon_809435.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const DEFAULT_PATHEXT[] = {
    ".COM", ".EXE", ".BAT", ".CMD", ".VBS", ".VBE", ".JS", ".JSE", ".WSF", ".WSH", ".MSC"
};

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

/* Case-insensitive key lookup for PATHEXT in the config env. */
static const char *find_pathext(const struct json_t *env) {
    if (!env || env->type != JSON_OBJECT) return NULL;
    for (size_t i = 0; i < json_object_size(env); ++i) {
        const char *k = json_object_get_key_at(env, i);
        struct json_t *v = json_object_get_at(env, i);
        if (k && v && v->type == JSON_STRING) {
            char *ku = (char *)malloc(strlen(k) + 1);
            if (ku) {
                size_t j;
                for (j = 0; k[j]; ++j) ku[j] = (char)toupper((unsigned char)k[j]);
                ku[j] = '\0';
                int match = !strcmp(ku, "PATHEXT");
                free(ku);
                char *t = strip_ws(v->str_val);
                int nonempty = t && *t;
                free(t);
                if (match && nonempty) return v->str_val;
            }
        }
    }
    return NULL;
}

struct json_t *w8_mcptoolcon_809435_pathext_suffixes(const struct json_t *env, int windows) {
    struct json_t *out = json_array();
    if (!windows) { json_append(out, json_string("")); return out; }
    const char *value = find_pathext(env);
    if (value) {
        const char *p = value;
        while (*p) {
            const char *q = strchr(p, ';');
            size_t len = q ? (size_t)(q - p) : strlen(p);
            if (len) {
                char *ext = (char *)malloc(len + 1);
                if (ext) { memcpy(ext, p, len); ext[len] = '\0'; json_append(out, json_string(ext)); free(ext); }
            }
            if (!q) break;
            p = q + 1;
        }
        if (json_len(out)) return out;
    }
    for (size_t i = 0; i < sizeof(DEFAULT_PATHEXT) / sizeof(DEFAULT_PATHEXT[0]); ++i)
        json_append(out, json_string(DEFAULT_PATHEXT[i]));
    return out;
}

/* Collect dotted paths of string values whose text has leading/trailing whitespace. */
static void walk_whitespace(const struct json_t *node, const char *path, struct json_t *out) {
    if (!node) return;
    if (node->type == JSON_STRING) {
        char *s = strip_ws(node->str_val);
        int flagged = s && strcmp(s, node->str_val ? node->str_val : "") != 0;
        free(s);
        if (flagged) json_append(out, json_string(path ? path : ""));
        return;
    }
    if (node->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(node); ++i) {
            const char *k = json_object_get_key_at(node, i);
            struct json_t *v = json_object_get_at(node, i);
            char *p = (char *)malloc((path ? strlen(path) : 0) + (k ? strlen(k) : 0) + 4);
            if (p) {
                if (path && *path) sprintf(p, "%s.%s", path, k ? k : "");
                else sprintf(p, "%s", k ? k : "");
                walk_whitespace(v, p, out);
                free(p);
            }
        }
        return;
    }
    if (node->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(node); ++i) {
            char *p = (char *)malloc((path ? strlen(path) : 0) + 32);
            if (p) { sprintf(p, "%s[%zu]", path ? path : "", i); walk_whitespace(json_get(node, i), p, out); free(p); }
        }
    }
}

struct json_t *w8_mcptoolcon_809435_warn_hidden_whitespace(const char *server_name,
        const struct json_t *config) {
    (void)server_name;  /* warn-once logging only; returned paths are independent */
    struct json_t *out = json_array();
    walk_whitespace(config, "", out);
    return out;
}

/* Regex \$\{([^}]+)\}: extract de-duplicated ${...} names from a string. */
static void collect_refs(const char *text, struct json_t *refs) {
    if (!text) return;
    const char *p = text;
    while ((p = strstr(p, "${")) != NULL) {
        const char *close = strchr(p + 2, '}');
        if (!close) break;
        size_t n = (size_t)(close - (p + 2));
        if (n) {
            char *ref = (char *)malloc(n + 1);
            if (ref) {
                memcpy(ref, p + 2, n); ref[n] = '\0';
                int seen = 0;
                for (size_t i = 0; i < json_len(refs); ++i) {
                    const char *rv = json_get(refs, i)->str_val;
                    if (rv && !strcmp(rv, ref)) { seen = 1; break; }
                }
                if (!seen) json_append(refs, json_string(ref));
                free(ref);
            }
        }
        p = close + 1;
    }
}

static void sort_refs(struct json_t *refs) {
    for (size_t i = 0; i < json_len(refs); ++i)
        for (size_t j = i + 1; j < json_len(refs); ++j)
            if (strcmp(json_get(refs, i)->str_val, json_get(refs, j)->str_val) > 0) {
                json_t *tmp = json_get(refs, i);
                json_array_set(refs, i, json_copy(json_get(refs, j)));
                json_array_set(refs, j, tmp);
            }
}

struct json_t *w8_mcptoolcon_809435_require_rendered_remote(const char *server_name,
        const struct json_t *config) {
    struct json_t *res = json_object();
    if (!config || config->type != JSON_OBJECT || !json_has(config, "url")) {
        json_set(res, "ok", json_copy(config ? config : json_null()));
        return res;
    }
    struct json_t *refs = json_array();
    const struct json_t *url = json_obj_get(config, "url");
    collect_refs(url && url->type == JSON_STRING ? url->str_val : "", refs);
    const struct json_t *headers = json_obj_get(config, "headers");
    if (headers && headers->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(headers); ++i) {
            struct json_t *v = json_object_get_at(headers, i);
            collect_refs(v && v->type == JSON_STRING ? v->str_val : "", refs);
        }
    }
    if (!json_len(refs)) {
        json_set(res, "ok", json_copy(config));
        json_free(refs);
        return res;
    }
    sort_refs(refs);
    char *msg = (char *)malloc(1024);
    if (msg) {
        size_t at = (size_t)sprintf(msg, "MCP server '%s': ", server_name ? server_name : "");
        for (size_t i = 0; i < json_len(refs); ++i) {
            if (i) at += (size_t)sprintf(msg + at, ", ");
            at += (size_t)sprintf(msg + at, "${%s}", json_get(refs, i)->str_val);
        }
        sprintf(msg + at, " in url/headers is not set in this profile's .env or secret source");
        json_set(res, "error", json_string(msg));
        free(msg);
    }
    json_free(refs);
    return res;
}
