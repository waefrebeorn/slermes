/*
 * gateway_client.c — C home for upstream Hermes hermes_cli/gateway_client.py.
 *
 * PoP: w12_gatewaycli_0af67a_gateway_ws_target @ hermes_cli/gateway_client.py:gateway_ws_target
 * PoP: w12_gatewaycli_0af67a_bounded_reason    @ hermes_cli/gateway_client.py:_bounded_reason
 * PoP: w12_gatewaycli_0af67a_bounded_error_data@ hermes_cli/gateway_client.py:_bounded_error_data
 * PoP: w12_gatewaycli_0af67a_rpc_error         @ hermes_cli/gateway_client.py:rpc_error
 *
 * Pure target / error-bounding helpers. No sockets or asyncio.
 */
#include "hermes_json.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w12_gatewaycli_0af67a.h"

#define GATEWAY_WS_PROTOCOL "hermes-gateway-v1"
#define GATEWAY_WS_TICKET_PREFIX "hermes-gateway-ticket."
#define ERROR_DATA_TEXT 200
#define ERROR_DATA_ITEMS 16

static char *replace_all(const char *s, const char *from, const char *to) {
    size_t fl = strlen(from), tl = strlen(to);
    if (!fl) return strdup(s ? s : "");
    size_t cap = 64, n = 0;
    char *out = (char *)malloc(cap);
    const char *p = s ? s : "";
    while (*p) {
        if (strncmp(p, from, fl) == 0) {
            if (n + tl + 1 > cap) { cap = (n + tl + 1) * 2; out = (char *)realloc(out, cap); }
            memcpy(out + n, to, tl); n += tl; p += fl;
        } else {
            if (n + 2 > cap) { cap *= 2; out = (char *)realloc(out, cap); }
            out[n++] = *p++;
        }
    }
    out[n] = 0;
    return out;
}

json_t *w12_gatewaycli_0af67a_gateway_ws_target(const char *api_origin, const char *ticket) {
    char *s1 = replace_all(api_origin, "https:", "wss:");
    char *s2 = replace_all(s1, "http:", "ws:");
    char url[4096];
    snprintf(url, sizeof url, "%s/api/ws", s2);
    char proto[2048];
    snprintf(proto, sizeof proto, "%s%s", GATEWAY_WS_TICKET_PREFIX, ticket ? ticket : "");
    json_t *protocols = json_array();
    json_append(protocols, json_string(GATEWAY_WS_PROTOCOL));
    json_append(protocols, json_string(proto));
    json_t *out = json_array();
    json_append(out, json_string(url));
    json_append(out, protocols);
    free(s1); free(s2);
    return out;
}

/* value.replace("_","").isalnum() and len(value) <= 80 — ASCII scalars. */
static int reason_ok(const char *s, size_t len) {
    if (len > 80) return 0;
    size_t count = 0;
    for (size_t i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c == '_') continue;
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) return 0;
        count++;
    }
    return count > 0;
}

json_t *w12_gatewaycli_0af67a_bounded_reason(const json_t *value) {
    if (!value || value->type != JSON_STRING || !value->str_val) return json_null();
    if (!reason_ok(value->str_val, strlen(value->str_val))) return json_null();
    return json_string(value->str_val);
}

static int is_scalar(const json_t *v) {
    if (!v) return 0;
    if (v->type == JSON_NULL || v->type == JSON_BOOL) return 1;
    if (v->type == JSON_NUMBER) return v->num_val == (double)(long long)v->num_val; /* int (not float) */
    if (v->type == JSON_STRING) return strlen(v->str_val ? v->str_val : "") <= ERROR_DATA_TEXT;
    return 0;
}

json_t *w12_gatewaycli_0af67a_bounded_error_data(const json_t *data) {
    json_t *out = json_object();
    if (!data || data->type != JSON_OBJECT) return out;
    for (size_t i = 0; i < json_object_size(data); ++i) {
        if (json_object_size(out) >= ERROR_DATA_ITEMS) break;
        const char *key = json_object_get_key_at(data, i);
        json_t *val = json_object_get_at(data, i);
        if (!key || strlen(key) > 64) continue;
        if (is_scalar(val)) { json_set(out, key, json_copy(val)); continue; }
        if (val && val->type == JSON_ARRAY && json_len(val) <= ERROR_DATA_ITEMS) {
            int all = 1;
            for (size_t j = 0; j < json_len(val); ++j)
                if (!is_scalar(json_get(val, j))) { all = 0; break; }
            if (all) json_set(out, key, json_copy(val));
        }
    }
    return out;
}

json_t *w12_gatewaycli_0af67a_rpc_error(const json_t *error) {
    json_t *data = w12_gatewaycli_0af67a_bounded_error_data(
        json_is_object(error) ? json_obj_get(error, "data") : NULL);

    json_t *code_in = json_is_object(error) ? json_obj_get(error, "code") : NULL;
    int has_code = 0;
    long long code = 0;
    if (code_in && code_in->type == JSON_NUMBER &&
        code_in->num_val == (double)(long long)code_in->num_val) {
        has_code = 1; code = (long long)code_in->num_val;
    }

    json_t *m = json_is_object(error) ? json_obj_get(error, "message") : NULL;
    json_t *tmpmsg = m ? NULL : json_string("request_failed");
    json_t *r1 = w12_gatewaycli_0af67a_bounded_reason(m ? m : tmpmsg);
    char reason_copy[256];
    if (r1 && r1->type == JSON_STRING) snprintf(reason_copy, sizeof reason_copy, "%s", r1->str_val);
    else {
        json_t *r2 = w12_gatewaycli_0af67a_bounded_reason(json_obj_get(data, "reason"));
        if (r2 && r2->type == JSON_STRING) snprintf(reason_copy, sizeof reason_copy, "%s", r2->str_val);
        else snprintf(reason_copy, sizeof reason_copy, "request_failed");
        json_free(r2);
    }
    json_free(r1);
    json_free(tmpmsg);

    int unknown = (has_code && code == -32601) || !strcmp(reason_copy, "unknown_method");
    json_t *out = json_object();
    json_set(out, "reason", json_string(reason_copy));
    json_set(out, "code", has_code ? json_number((double)code) : json_null());
    json_set(out, "data", data);
    json_set(out, "unknown_method", json_bool(unknown));
    return out;
}
