/*
 * gateway_tools.c — C port of upstream Hermes hermes_cli/gateway_tools.py.
 *
 * PoP: w10_gatewaytoo_83997c_proc_from_row @ hermes_cli/gateway_tools.py:_Proc.__init__
 * PoP: w10_gatewaytoo_83997c_filter_printable @ hermes_cli/gateway_tools.py:GatewayProcessView.subagent_tail
 * PoP: w10_gatewaytoo_83997c_interesting @ hermes_cli/gateway_tools.py:GatewayProcessView._interesting
 *
 * The gateway/RPC seams are represented as explicit fixture inputs; these are
 * the pure attribute-shaping and cache-policy helpers the view is built from.
 */
#include "noc_w10_gatewaytoo_83997c.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RETAIN_SECONDS 60.0

static int truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
        case JSON_STRING: return v->str_val && v->str_val[0] ? 1 : 0;
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0 ? 1 : 0;
    }
    return 0;
}
/* row.get(key) or "" — Python keeps the value's type. */
static json_t *or_empty(const json_t *v) {
    return truthy(v) ? json_copy(v) : json_string("");
}
/* float(row.get(key) or 0.0) */
static double float_or_zero(const json_t *v) {
    if (!truthy(v)) return 0.0;
    if (v->type == JSON_NUMBER) return v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1.0 : 0.0;
    if (v->type == JSON_STRING) return strtod(v->str_val, NULL);
    return 0.0;
}
static int status_is(const json_t *row, const char *want) {
    const json_t *s = json_obj_get(row, "status");
    return s && s->type == JSON_STRING && !strcmp(s->str_val, want);
}

json_t *w10_gatewaytoo_83997c_proc_from_row(const struct json_t *row) {
    json_t *out = json_object();
    json_set(out, "id", or_empty(json_obj_get(row, "session_id")));
    json_set(out, "command", or_empty(json_obj_get(row, "command")));
    const json_t *ec = json_obj_get(row, "exit_code");
    json_set(out, "exit_code", ec ? json_copy(ec) : json_null());   /* None when absent */
    json_set(out, "started_at", json_number(float_or_zero(json_obj_get(row, "started_epoch"))));
    json_set(out, "exited_at", json_number(float_or_zero(json_obj_get(row, "exited_at"))));
    json_set(out, "output_buffer", or_empty(json_obj_get(row, "output_tail")));
    const json_t *cr = json_obj_get(row, "completion_reason");
    json_set(out, "completion_reason", truthy(cr) ? json_copy(cr) : json_string("exited"));
    json_set(out, "exited", json_bool(status_is(row, "exited") ? true : false));
    return out;
}

/* ASCII subset of str.isprintable(): printable 0x20..0x7e, plus newline/tab. */
json_t *w10_gatewaytoo_83997c_filter_printable(const char *text) {
    size_t n = text ? strlen(text) : 0;
    char *buf = (char *)malloc(n + 1);
    size_t k = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)text[i];
        if ((c >= 0x20 && c <= 0x7e) || c == '\n' || c == '\t') buf[k++] = (char)c;
    }
    buf[k] = 0;
    json_t *out = json_string(buf);
    free(buf);
    return out;
}

int w10_gatewaytoo_83997c_interesting(const struct json_t *rows, double now) {
    if (!json_is_array(rows)) return 0;
    for (size_t i = 0; i < json_len(rows); i++) {
        const json_t *row = json_get(rows, i);
        if (!json_is_object(row)) continue;
        int exited = status_is(row, "exited") ? 1 : 0;
        double exited_at = float_or_zero(json_obj_get(row, "exited_at"));
        if (!exited || (exited_at != 0.0 && now - exited_at <= RETAIN_SECONDS)) return 1;
    }
    return 0;
}
