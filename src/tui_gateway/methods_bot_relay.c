/*
 * methods_bot_relay.c — C home for upstream Hermes tui_gateway/methods_bot_relay.py.
 *
 * PoP: w18_methodsbot_bc579b_reply @ tui_gateway/methods_bot_relay.py:_ (bot_relay.reply)
 *
 * Mirrors the handler's own logic only. `_ok`/`_err`, `current_transport`,
 * `_relay_root` and `tools.bot_relay.write_reply` are server-side globals the
 * oracle injects; this module ports the id/field coercion and error routing.
 */
#include "noc_w18_methodsbot_bc579b.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int py_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
    }
    return 0;
}

/* str(value or "") for the JSON values the fixture can carry. */
static const char *py_str(const json_t *v, char *buf, size_t cap) {
    if (!py_truthy(v)) return "";
    if (v->type == JSON_STRING) return v->str_val;
    if (v->type == JSON_BOOL) return "True";
    if (v->type == JSON_NUMBER) {
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, cap, "%lld", (long long)v->num_val);
        else
            snprintf(buf, cap, "%g", v->num_val);
        return buf;
    }
    return "";
}

static char *strip_dup(const char *s) {
    size_t a = 0, b = strlen(s);
    char *d;
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\n' || s[a] == '\r' ||
                     s[a] == '\v' || s[a] == '\f')) a++;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\n' || s[b - 1] == '\r' ||
                     s[b - 1] == '\v' || s[b - 1] == '\f')) b--;
    d = (char *)malloc(b - a + 1);
    if (!d) return NULL;
    memcpy(d, s + a, b - a);
    d[b - a] = '\0';
    return d;
}

static json_t *err_payload(int code, const char *msg) {
    json_t *o = json_object();
    json_set(o, "code", json_number((double)code));
    json_set(o, "message", json_string(msg ? msg : ""));
    json_set(o, "data", json_null());
    return o;
}

json_t *w18_methodsbot_bc579b_reply(const char *rid, const json_t *params,
                                    const char *mode, const char *msg) {
    char ibuf[64], rbuf[64], ebuf[64], nbuf[64];
    const char *raw;
    char *eid;
    json_t *root = json_object(), *rpc, *written = NULL;
    (void)rid; /* injected _ok/_err stubs ignore rid; kept for signature fidelity */

    raw = py_str(params ? json_obj_get(params, "id") : NULL, ibuf, sizeof(ibuf));
    eid = strip_dup(raw);
    if (!eid || !*eid) {
        free(eid);
        json_set(root, "rpc", err_payload(4093, "id required"));
        json_set(root, "written", json_null());
        return root;
    }

    if (mode && !strcmp(mode, "valueerror")) {
        rpc = err_payload(4094, msg && *msg ? msg : "bad");
        json_set(root, "rpc", rpc);
        json_set(root, "written", json_null());
        free(eid);
        return root;
    }
    if (mode && !strcmp(mode, "error")) {
        rpc = err_payload(5095, msg && *msg ? msg : "boom");
        json_set(root, "rpc", rpc);
        json_set(root, "written", json_null());
        free(eid);
        return root;
    }

    written = json_object();
    json_set(written, "id", json_string(eid));
    json_set(written, "reply", json_string(py_str(params ? json_obj_get(params, "reply") : NULL, rbuf, sizeof(rbuf))));
    json_set(written, "error", json_string(py_str(params ? json_obj_get(params, "error") : NULL, ebuf, sizeof(ebuf))));
    json_set(written, "reason", json_string(py_str(params ? json_obj_get(params, "reason") : NULL, nbuf, sizeof(nbuf))));
    rpc = json_object();
    json_set(rpc, "ok", json_bool(true));
    json_set(root, "rpc", rpc);
    json_set(root, "written", written);
    free(eid);
    return root;
}

