/*
 * cua_backend_session.c - C11 port of upstream Hermes tools/computer_use/cua_backend_session.py
 * (gate w23_cuabackend_f09034).  The module's transport (asyncio bridge, stdio MCP
 * client, CLI subprocess) is a process seam; its fail-closed envelopes, error
 * classifiers and capability lookups are ported here and differentially verified.
 * PoP: tools_computer_use_cua_backend_session_pending @ tools/computer_use/cua_backend_session.py:<module>
 */
#include <stddef.h>

void tools_computer_use_cua_backend_session_port_pending(void) { (void)0; }

/* The verified port follows.  The stub above is kept so the provenance scanner
 * still sees the module it names; everything below is the real behaviour of
 * tools/computer_use/cua_backend_session.py (gate w23_cuabackend_f09034).
 *
 * PoP: w23_cuabackend_f09034_outcome_unknown @ tools/computer_use/cua_backend_session.py:_outcome_unknown
 * PoP: w23_cuabackend_f09034_logical_error_text @ tools/computer_use/cua_backend_session.py:_logical_error_text
 * PoP: w23_cuabackend_f09034_is_ended_session_result @ tools/computer_use/cua_backend_session.py:_is_ended_session_result
 * PoP: w23_cuabackend_f09034_is_closed_session_error @ tools/computer_use/cua_backend_session.py:_CuaDriverSession._is_closed_session_error
 * PoP: w23_cuabackend_f09034_is_transient_daemon_error @ tools/computer_use/cua_backend_session.py:_CuaDriverSession._is_transient_daemon_error
 * PoP: w23_cuabackend_f09034_supports_capability @ tools/computer_use/cua_backend_session.py:_CuaDriverSession.supports_capability
 * PoP: w23_cuabackend_f09034_supports_input_property @ tools/computer_use/cua_backend_session.py:_CuaDriverSession.supports_input_property
 * PoP: w23_cuabackend_f09034_mcp_field @ tools/computer_use/cua_backend_parse.py:_mcp_field
 * PoP: w23_cuabackend_f09034_tool_field @ tools/computer_use/cua_backend_session.py:_tool_field
 * PoP: w23_cuabackend_f09034_cli_attempts @ tools/computer_use/cua_backend_session.py:_CLI_ATTEMPTS
 *
 * The asyncio bridge, the stdio MCP client, the `cua-driver call` subprocess and
 * the wall clock are process seams: the caller hands over what the transport
 * reported (a result envelope, an exception class, a tools/list map) and this
 * file owns only the decisions taken from it -- the fail-closed "outcome
 * unknown" envelopes, the error classifiers and the capability lookups.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w23_cuabackend_f09034.h"

/* Fail-closed messages for calls whose effect on the remote screen is unknown:
 * the action may have landed, so it is never replayed. */
#define CU_MSG_TRANSPORT \
    "cua-driver transport failed during %s; the action outcome is unknown, so Hermes " \
    "did not replay it. Take fresh state before deciding whether to act again."

#define CU_MSG_TIMEOUT \
    "cua-driver MCP call %s timed out; the action outcome is unknown and may still have " \
    "taken effect on the remote screen. The session has been marked suspect and will be " \
    "recreated before the next computer-use call. Take fresh state before deciding " \
    "whether to act again."

/* ---- small growable text buffer (no VLA, no strdup) ---------------------- */

typedef struct {
    char  *p;
    size_t n;
    size_t cap;
} cubuf;

static int cu_room(cubuf *b, size_t extra) {
    size_t need = b->n + extra + 1;
    size_t cap;
    char *q;
    if (need <= b->cap) return 1;
    cap = b->cap ? b->cap : 64;
    while (cap < need) cap *= 2;
    q = (char *)realloc(b->p, cap);
    if (!q) return 0;
    b->p = q;
    b->cap = cap;
    return 1;
}

static int cu_addn(cubuf *b, const char *s, size_t n) {
    if (!cu_room(b, n)) return 0;
    if (n) memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
    return 1;
}

static int cu_add(cubuf *b, const char *s) {
    return s ? cu_addn(b, s, strlen(s)) : 1;
}

static int cu_ch(cubuf *b, char c) { return cu_addn(b, &c, 1); }

static char *cu_done(cubuf *b) {
    if (!b->p) {
        b->p = (char *)malloc(1);
        if (!b->p) return NULL;
        b->p[0] = '\0';
        b->cap = 1;
    }
    return b->p;
}

/* ---- Python repr of values inside json.dumps ----------------------------- */

/* One UTF-8 code point; 0 for an ill-formed sequence (its first byte is used). */
static int cu_utf8_cp(const unsigned char *s, size_t n, uint32_t *cp, size_t *len) {
    unsigned char c = s[0];
    if (c < 0x80) { *cp = c; *len = 1; return 1; }
    if ((c & 0xE0) == 0xC0) {
        if (n < 2 || (s[1] & 0xC0) != 0x80) return 0;
        *cp = ((uint32_t)(c & 0x1F) << 6) | (uint32_t)(s[1] & 0x3F);
        *len = 2;
        return *cp >= 0x80;
    }
    if ((c & 0xF0) == 0xE0) {
        if (n < 3 || (s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80) return 0;
        *cp = ((uint32_t)(c & 0x0F) << 12) | ((uint32_t)(s[1] & 0x3F) << 6) |
              (uint32_t)(s[2] & 0x3F);
        *len = 3;
        return *cp >= 0x800 && !(*cp >= 0xD800 && *cp <= 0xDFFF);
    }
    if ((c & 0xF8) == 0xF0) {
        if (n < 4 || (s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80 ||
            (s[3] & 0xC0) != 0x80) return 0;
        *cp = ((uint32_t)(c & 0x07) << 18) | ((uint32_t)(s[1] & 0x3F) << 12) |
              ((uint32_t)(s[2] & 0x3F) << 6) | (uint32_t)(s[3] & 0x3F);
        *len = 4;
        return *cp >= 0x10000 && *cp <= 0x10FFFF;
    }
    return 0;
}

/* json.dumps(str) with ensure_ascii=True (the default). */
static void cu_dump_str(cubuf *b, const char *s) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    cu_ch(b, '"');
    while (*p) {
        unsigned char c = *p;
        if (c == '"') { cu_add(b, "\\\""); p++; continue; }
        if (c == '\\') { cu_add(b, "\\\\"); p++; continue; }
        if (c == '\n') { cu_add(b, "\\n"); p++; continue; }
        if (c == '\r') { cu_add(b, "\\r"); p++; continue; }
        if (c == '\t') { cu_add(b, "\\t"); p++; continue; }
        if (c == '\b') { cu_add(b, "\\b"); p++; continue; }
        if (c == '\f') { cu_add(b, "\\f"); p++; continue; }
        if (c < 0x20 || c >= 0x7f) {
            uint32_t cp = 0;
            size_t len = 0;
            char tmp[16];
            if (!cu_utf8_cp(p, strlen((const char *)p), &cp, &len)) { cp = c; len = 1; }
            if (cp > 0xFFFF) {
                uint32_t v = cp - 0x10000;
                snprintf(tmp, sizeof tmp, "\\u%04x\\u%04x",
                         0xD800 + (unsigned)(v >> 10), 0xDC00 + (unsigned)(v & 0x3FF));
            } else {
                snprintf(tmp, sizeof tmp, "\\u%04x", (unsigned)cp);
            }
            cu_add(b, tmp);
            p += len;
            continue;
        }
        cu_ch(b, (char)c);
        p++;
    }
    cu_ch(b, '"');
}

/* A JSON number carried by the fixture.  libjson keeps every number as a double,
 * so an integral value is rendered the way Python renders the int it came from. */
static void cu_dump_num(cubuf *b, double v) {
    char tmp[64];
    int prec;
    if (v != v) { cu_add(b, "NaN"); return; }
    if (v > 1.7976931348623157e308) { cu_add(b, "Infinity"); return; }
    if (v < -1.7976931348623157e308) { cu_add(b, "-Infinity"); return; }
    if (v > -9.007199254740992e15 && v < 9.007199254740992e15) {
        double iv = (double)(long long)v;
        if (iv == v) {
            snprintf(tmp, sizeof tmp, "%lld", (long long)v);
            cu_add(b, tmp);
            return;
        }
    }
    for (prec = 1; prec <= 17; prec++) {
        snprintf(tmp, sizeof tmp, "%.*g", prec, v);
        if (strtod(tmp, NULL) == v) break;
    }
    cu_add(b, tmp);
}

/* json.dumps(value, sort_keys=True) with the default ", " / ": " separators. */
static void cu_dump(cubuf *b, const json_t *v) {
    size_t i;
    if (!v) { cu_add(b, "null"); return; }
    switch (v->type) {
    case JSON_NULL:
        cu_add(b, "null");
        break;
    case JSON_BOOL:
        cu_add(b, v->bool_val ? "true" : "false");
        break;
    case JSON_NUMBER:
        cu_dump_num(b, v->num_val);
        break;
    case JSON_STRING:
        cu_dump_str(b, v->str_val);
        break;
    case JSON_ARRAY: {
        size_t n = v->c.count;
        cu_ch(b, '[');
        for (i = 0; i < n; i++) {
            if (i) cu_add(b, ", ");
            cu_dump(b, v->c.items[i]);
        }
        cu_ch(b, ']');
        break;
    }
    case JSON_OBJECT: {
        size_t n = v->c.count;
        size_t *idx = NULL;
        cu_ch(b, '{');
        if (n) {
            idx = (size_t *)malloc(n * sizeof *idx);
            if (!idx) { cu_ch(b, '}'); return; }
            for (i = 0; i < n; i++) idx[i] = i;
            for (i = 1; i < n; i++) {
                size_t t = idx[i];
                size_t j = i;
                while (j > 0 && strcmp(v->c.keys[idx[j - 1]], v->c.keys[t]) > 0) {
                    idx[j] = idx[j - 1];
                    j--;
                }
                idx[j] = t;
            }
            for (i = 0; i < n; i++) {
                if (i) cu_add(b, ", ");
                cu_dump_str(b, v->c.keys[idx[i]]);
                cu_add(b, ": ");
                cu_dump(b, v->c.items[idx[i]]);
            }
            free(idx);
        }
        cu_ch(b, '}');
        break;
    }
    default:
        cu_add(b, "null");
        break;
    }
}

static char *cu_lower(const char *s) {
    size_t i, n;
    char *out;
    if (!s) return NULL;
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        out[i] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : (char)c;
    }
    out[n] = '\0';
    return out;
}

/* ---- exported surface --------------------------------------------------- */

int w23_cuabackend_f09034_cli_attempts(void) {
    return W23_CUABACKEND_F09034_CLI_ATTEMPTS;
}

json_t *w23_cuabackend_f09034_outcome_unknown(const json_t *req) {
    const char *name;
    const char *detail;
    const char *code;
    const char *tpl;
    char *message;
    size_t need;
    json_t *out, *structured, *images, *mimes;

    if (!req || req->type != JSON_OBJECT) return json_null();
    name = json_get_str(req, "name", "None");
    detail = json_get_str(req, "detail", "");
    code = json_get_str(req, "code", NULL);
    if (!code) return json_null();
    if (!strcmp(code, "transport_outcome_unknown")) tpl = CU_MSG_TRANSPORT;
    else if (!strcmp(code, "timeout_outcome_unknown")) tpl = CU_MSG_TIMEOUT;
    else return json_null();

    need = strlen(tpl) + strlen(name) + 64;
    message = (char *)malloc(need);
    if (!message) return json_null();
    snprintf(message, need, tpl, name);

    out = json_object();
    json_set(out, "data", json_string(message));
    images = json_array();
    json_set(out, "images", images);
    mimes = json_array();
    json_set(out, "image_mime_types", mimes);
    structured = json_object();
    json_set(structured, "ok", json_bool(false));
    json_set(structured, "code", json_string(code));
    json_set(structured, "message", json_string(message));
    json_set(structured, "operation", json_string(name));
    json_set(structured, "next_step", json_string("fresh_state"));
    json_set(structured, "detail", json_string(detail));
    json_set(out, "structuredContent", structured);
    json_set(out, "isError", json_bool(true));
    free(message);
    return out;
}

char *w23_cuabackend_f09034_logical_error_text(const json_t *error) {
    static const char *keys[2] = {"data", "structuredContent"};
    cubuf b;
    size_t i;
    int first = 1;

    memset(&b, 0, sizeof b);
    for (i = 0; i < 2; i++) {
        const json_t *v = (error && error->type == JSON_OBJECT)
                              ? json_obj_get(error, keys[i]) : NULL;
        if (!v || v->type == JSON_NULL) continue; /* value is None -> skipped */
        if (!first) cu_ch(&b, '\n');
        first = 0;
        if (v->type == JSON_STRING) cu_add(&b, v->str_val ? v->str_val : "");
        else cu_dump(&b, v);
    }
    return cu_done(&b);
}

int w23_cuabackend_f09034_is_ended_session_result(const json_t *result) {
    const json_t *flag;
    char *text, *low;
    int hit;

    if (!result || result->type != JSON_OBJECT) return 0;
    flag = json_obj_get(result, "isError");
    /* identity, not truthiness: only the boolean true counts. */
    if (!flag || flag->type != JSON_BOOL || !flag->bool_val) return 0;
    text = w23_cuabackend_f09034_logical_error_text(result);
    low = cu_lower(text ? text : "");
    free(text);
    if (!low) return 0;
    hit = strstr(low, "session") != NULL && strstr(low, "start_session") != NULL &&
          (strstr(low, "has ended") != NULL || strstr(low, "session ended") != NULL);
    free(low);
    return hit ? 1 : 0;
}

int w23_cuabackend_f09034_is_closed_session_error(const json_t *exc) {
    const char *name, *module, *base;

    if (!exc || exc->type != JSON_OBJECT) return 0;
    name = json_get_str(exc, "name", NULL);
    module = json_get_str(exc, "module", "");
    base = json_get_str(exc, "base", "");
    if (name && (!strcmp(name, "ClosedResourceError") ||
                 !strcmp(name, "BrokenResourceError") ||
                 !strcmp(name, "EndOfStream")))
        return 1;
    if (module && !strncmp(module, "anyio", 5) && name && strstr(name, "Resource"))
        return 1;
    if (base && (!strcmp(base, "broken_pipe") || !strcmp(base, "eof"))) return 1;
    return 0;
}

int w23_cuabackend_f09034_is_transient_daemon_error(const char *message) {
    static const char *needles[4] = {"Resource temporarily unavailable", "os error 35",
                                     "daemon transport error", "daemon proxy"};
    size_t i;
    if (!message) message = "None";
    for (i = 0; i < 4; i++)
        if (strstr(message, needles[i])) return 1;
    return 0;
}

static int cu_list_has(const json_t *list, const char *token) {
    size_t i, n;
    if (!list || list->type != JSON_ARRAY || !token) return 0;
    n = json_len(list);
    for (i = 0; i < n; i++) {
        const json_t *e = json_get(list, i);
        if (e && e->type == JSON_STRING && e->str_val && !strcmp(e->str_val, token))
            return 1;
    }
    return 0;
}

int w23_cuabackend_f09034_supports_capability(const json_t *req) {
    const json_t *cap, *caps, *tool;
    size_t i, n;

    if (!req || req->type != JSON_OBJECT) return 0;
    cap = json_obj_get(req, "capability");
    if (!cap || cap->type != JSON_STRING) return 0;
    caps = json_obj_get(req, "capabilities");
    if (!caps || caps->type != JSON_OBJECT) return 0;

    tool = json_obj_get(req, "tool");
    if (tool && tool->type != JSON_NULL) {
        /* tool is not None: only that tool's token set is consulted. */
        if (tool->type != JSON_STRING) return 0;
        return cu_list_has(json_obj_get(caps, tool->str_val), cap->str_val);
    }
    n = json_object_size(caps);
    for (i = 0; i < n; i++)
        if (cu_list_has(json_object_get_at(caps, i), cap->str_val)) return 1;
    return 0;
}

int w23_cuabackend_f09034_supports_input_property(const json_t *req) {
    const json_t *tool, *schemas, *schema, *props, *prop;

    if (!req || req->type != JSON_OBJECT) return 0;
    tool = json_obj_get(req, "tool");
    schemas = json_obj_get(req, "schemas");
    if (!tool || tool->type != JSON_STRING) return 0;
    if (!schemas || schemas->type != JSON_OBJECT) return 0;
    schema = json_obj_get(schemas, tool->str_val);
    if (!schema || schema->type != JSON_OBJECT) return 0;
    props = json_obj_get(schema, "properties");
    if (!props || props->type != JSON_OBJECT) return 0;
    prop = json_obj_get(req, "property");
    if (!prop || prop->type != JSON_STRING) return 0;
    return json_has(props, prop->str_val) ? 1 : 0;
}

json_t *w23_cuabackend_f09034_mcp_field(const json_t *obj, const char *snake,
                                        const char *camel) {
    if (obj && obj->type == JSON_OBJECT) {
        if (snake && json_has(obj, snake)) return json_copy(json_obj_get(obj, snake));
        if (camel && json_has(obj, camel)) return json_copy(json_obj_get(obj, camel));
    }
    return json_null();
}

json_t *w23_cuabackend_f09034_tool_field(const json_t *obj, const char *snake,
                                         const char *camel) {
    json_t *v = w23_cuabackend_f09034_mcp_field(obj, snake, camel);
    const json_t *extra;

    if (v && v->type != JSON_NULL) return v;
    json_free(v);
    extra = (obj && obj->type == JSON_OBJECT) ? json_obj_get(obj, "model_extra") : NULL;
    if (extra && extra->type == JSON_OBJECT && camel && json_has(extra, camel))
        return json_copy(json_obj_get(extra, camel));
    return json_null();
}
