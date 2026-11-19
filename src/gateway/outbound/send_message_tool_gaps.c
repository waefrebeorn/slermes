#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── tools/send_message_tool.py: 4 missing functions ── */

/* ── rest_revive1 seam infrastructure (send_message_tool_gaps) ──────────────
 * json_t *fn(sm_io_t *io, json_t *in): io->call is the injected I/O seam.
 * sm_seam records {ev, args, ret} into io->trace in call order and returns
 * the scripted result (caller frees; args are ADOPTED). A result
 * {"__raise__": Type, "message": msg} models a Python exception. NULL
 * return == Python None. Integral JSON numbers model Python ints.
 *
 * bounded_send_error: str(detail or "send failed") then codepoint-exact
 * len/slice. Fixtures restrict detail to str/int/bool/None/falsy values
 * (float/container repr is out of scope, documented in the oracle); len()
 * and slicing count Unicode codepoints, and a negative slice bound follows
 * Python semantics (text[:-3] keeps all but the last 3 codepoints).
 *
 * Production default (no seam channel installed) is fail-closed. */
typedef struct sm_io {
    json_t *(*call)(struct sm_io *io, const char *name, json_t *args);
    json_t *trace;
    void *ctx;
} sm_io_t;

static int sm_is_raise(const json_t *r) {
    return r && r->type == JSON_OBJECT && json_obj_get(r, "__raise__") != NULL;
}

static json_t *sm_raise(const char *type, const char *msg) {
    json_t *e = json_object();
    json_set_hdr(e, "__raise__", type);
    json_set_hdr(e, "message", msg);
    return e;
}

static int sm_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL:   return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY:
        case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
        default: return 0;
    }
}

static json_t *sm_seam(sm_io_t *io, const char *name, json_t *args) {
    if (!io || !io->call) {
        if (args) json_free(args);
        char msg[160];
        snprintf(msg, sizeof msg, "no seam channel installed for %s", name);
        return sm_raise("RuntimeError", msg);
    }
    json_t *ret = io->call(io, name, args);
    json_t *ev = json_object();
    json_set_hdr(ev, "ev", name);
    json_set(ev, "args", args ? args : json_object());
    json_set(ev, "ret", ret ? json_copy(ret) : json_null());
    json_append(io->trace, ev);
    return ret;
}

/* str(v) for the detail slot: None -> "None", str passthrough, integral
 * numbers -> decimal, bool -> True/False (fixtures restrict to these). */
static char *sm_py_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        snprintf(buf, sizeof buf, "%lld", (long long)v->num_val);
        return strdup(buf);
    }
    char *ser = json_serialize(v);
    char *out = strdup(ser ? ser : "");
    free(ser);
    return out;
}

/* Codepoint count of a UTF-8 string (invalid bytes count as one each). */
static size_t sm_utf8_len(const char *s) {
    size_t n = 0;
    while (*s) {
        unsigned char c = (unsigned char)*s;
        if (c < 0x80) s += 1;
        else if ((c & 0xE0) == 0xC0) s += 2;
        else if ((c & 0xF0) == 0xE0) s += 3;
        else if ((c & 0xF8) == 0xF0) s += 4;
        else s += 1;
        n++;
    }
    return n;
}

/* malloc'd prefix of n codepoints (n clamped to [0, len]). */
static char *sm_utf8_prefix(const char *s, size_t n) {
    const char *p = s;
    size_t i = 0;
    while (*p && i < n) {
        unsigned char c = (unsigned char)*p;
        if (c < 0x80) p += 1;
        else if ((c & 0xE0) == 0xC0) p += 2;
        else if ((c & 0xF0) == 0xE0) p += 3;
        else if ((c & 0xF8) == 0xF0) p += 4;
        else p += 1;
        i++;
    }
    size_t len = (size_t)(p - s);
    char *out = malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len);
    out[len] = '\0';
    return out;
}

/* PoP: prepare_send_message_platforms @ gateway/outbound/send.py:prepare_send_message_platforms */
json_t *prepare_send_message_platforms(sm_io_t *io, json_t *in) {
    (void)in;
    /* from hermes_cli.plugins import discover_plugins */
    json_t *imp = sm_seam(io, "import_discover_plugins", NULL);
    if (sm_is_raise(imp)) return imp;
    json_free(imp);
    json_t *r = sm_seam(io, "discover_plugins", NULL);
    if (sm_is_raise(r)) return r;
    json_free(r);
    return json_null();
}

/* resolve_send_target: deferred (PoP tag removed; stub left untagged). */
char *resolve_send_target(void) { return strdup("{}"); }

/* PoP: bounded_send_error @ gateway/outbound/send.py:_bounded_send_error */
json_t *bounded_send_error(sm_io_t *io, json_t *in) {
    (void)io;
    json_t *detail = json_obj_get(in, "detail");
    char *text = sm_truthy(detail) ? sm_py_str(detail) : strdup("send failed");
    json_t *mc = json_obj_get(in, "max_chars");
    long long max_chars = (mc && mc->type == JSON_NUMBER) ? (long long)mc->num_val : 900;
    size_t len = sm_utf8_len(text);
    json_t *out;
    if ((long long)len <= max_chars) {
        out = json_string(text);
    } else {
        /* text[:max_chars - 3] + "..." — Python negative-slice semantics */
        long long bound = max_chars - 3;
        if (bound < 0) bound = (long long)len + bound;
        if (bound < 0) bound = 0;
        if ((size_t)bound > len) bound = (long long)len;
        char *prefix = sm_utf8_prefix(text, (size_t)bound);
        size_t plen = prefix ? strlen(prefix) : 0;
        char *joined = malloc(plen + 4);
        if (joined) {
            if (prefix) memcpy(joined, prefix, plen);
            memcpy(joined + plen, "...", 4);
        }
        out = json_string(joined ? joined : "");
        free(joined);
        free(prefix);
    }
    free(text);
    return out;
}

/* send_live_adapter_media: deferred (PoP tag removed; stub left untagged). */
char *send_live_adapter_media(void) { return strdup("{}"); }
