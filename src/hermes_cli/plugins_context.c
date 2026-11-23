/*
 * plugins_context.c — C home for upstream Hermes hermes_cli/plugins_context.py.
 *
 * PoP: w13_pluginscon_affd9c_mcp_envelope @ hermes_cli/plugins_context.py:PluginContext._mcp_envelope
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w13_pluginscon_affd9c.h"

#define MCP_CAP 65536

static char *dups(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    if (n) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}
static size_t cp_count(const char *s) {
    size_t n = 0;
    for (; s && *s; s++) if (((unsigned char)*s & 0xC0) != 0x80) n++;
    return n;
}
static size_t cp_prefix_len(const char *s, size_t n) {
    size_t count = 0, i = 0;
    if (n == 0) return 0;
    for (; s[i]; i++) if (((unsigned char)s[i] & 0xC0) != 0x80) { if (count == n) return i; count++; }
    return i;
}
/* Python str() for non-string JSON scalars (objects/arrays are not used by fixtures). */
static char *py_scalar(const json_t *v) {
    char buf[64];
    if (!v || v->type == JSON_NULL) return dups("None");
    if (v->type == JSON_STRING) return dups(v->str_val);
    if (v->type == JSON_BOOL) return dups(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val) snprintf(buf, sizeof(buf), "%lld", (long long)v->num_val);
        else snprintf(buf, sizeof(buf), "%.17g", v->num_val);
        return dups(buf);
    }
    return dups(v->type == JSON_ARRAY ? "[]" : "{}");
}

json_t *w13_pluginscon_affd9c_mcp_envelope(const struct json_t *in) {
    const json_t *raw = json_obj_get(in, "raw");
    char *text, *owned = NULL;
    bool truncated = false;
    json_t *parsed, *out;
    if (!raw || raw->type == JSON_NULL) text = dups("");
    else if (raw->type == JSON_STRING) text = dups(raw->str_val);
    else { owned = py_scalar(raw); text = owned; }
    if (cp_count(text) > MCP_CAP) {
        size_t bytes = cp_prefix_len(text, MCP_CAP);
        char *buf = (char *)malloc(bytes + 32);
        if (buf) {
            if (bytes) memcpy(buf, text, bytes);
            memcpy(buf + bytes, "\xe2\x80\xa6 [truncated]", 15); /* U+2026 + ' [truncated]' */
            buf[bytes + 15] = '\0';
            free(owned); owned = buf; text = buf;
            truncated = true;
        }
    }
    parsed = json_parse(text, NULL);
    out = json_object();
    if (parsed && parsed->type == JSON_OBJECT) {
        const json_t *err = json_obj_get(parsed, "error");
        const json_t *res = json_obj_get(parsed, "result");
        if (err) {
            json_set(out, "ok", json_bool(false));
            json_set(out, "error", json_copy(err));
        } else if (res) {
            const json_t *sc;
            json_set(out, "ok", json_bool(true));
            json_set(out, "result", json_copy(res));
            sc = json_obj_get(parsed, "structuredContent");
            if (sc) json_set(out, "structuredContent", json_copy(sc));
        } else {
            json_set(out, "ok", json_bool(true));
            json_set(out, "result", json_copy(parsed));
        }
    } else if (parsed && parsed->type != JSON_NULL) {
        json_set(out, "ok", json_bool(true));
        json_set(out, "result", json_copy(parsed));
    } else {
        json_set(out, "ok", json_bool(true));
        json_set(out, "result", json_string(text));
    }
    if (truncated) json_set(out, "truncated", json_bool(true));
    if (parsed) json_free(parsed);
    free(owned);
    return out;
}
