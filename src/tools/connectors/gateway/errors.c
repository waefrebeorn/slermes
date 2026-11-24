/*
 * errors.c -- real C11 port of upstream Hermes tools/connectors/gateway/errors.py.
 *
 * PoP: w21_errors_5bcef1_parse @ tools/connectors/gateway/errors.py:parse_gateway_error
 * PoP: w21_errors_5bcef1_render @ tools/connectors/gateway/errors.py:render_connection_required
 * PoP: w21_errors_5bcef1_default_message @ tools/connectors/gateway/errors.py:_default_message
 *
 * The three exception classes carry nothing but attributes, so the port's
 * answers name the class Python would build instead of building it: the shape
 * a caller inspects (`code`, `status`, `request_id`, `retryable`,
 * `retry_after`) is what travels.  `SIDE_AGENT_HINT` and the rest of `_HINTS`
 * are the literals below.
 */
#include "hermes_json.h"
#include "noc_w21_errors_5bcef1.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* tools/connectors/turn.py: the connection surfaces. */
#define GERR_SIDE "side"
#define GERR_CARD "card"
#define GERR_LINK "link"

static const char GERR_SIDE_AGENT_HINT[] =
    "This app is not connected. Only the main agent can connect it, so report that back "
    "instead of trying to connect it here.";
static const char GERR_CARD_HINT[] =
    "This app is not connected. Call manage_connections to open a connection card for it; "
    "there is no link to show the user.";
static const char GERR_EXPIRED_SIDE_AGENT_HINT[] =
    "This app's connection expired. Only the main agent can reconnect it, so report that back "
    "instead of trying to reconnect it here.";
static const char GERR_EXPIRED_CARD_HINT[] =
    "This app's connection expired. Call manage_connections with action 'reconnect' to open a "
    "connection card for it; there is no link to show the user.";
static const char GERR_EXPIRED_LINK_HINT[] =
    "The connection expired. Show the user the connect_url to reconnect, then retry the call.";

static char *gerr_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static int gerr_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

/* Python str() of a number: integral values print without a fraction. */
static void gerr_num_str(double d, char *buf, size_t n) {
    if (isfinite(d) && d == (double)(long long)d) snprintf(buf, n, "%.0f", d);
    else snprintf(buf, n, "%.17g", d);
}

/* Python repr() of a scalar, as list str() renders it. */
static char *gerr_repr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return gerr_dup("None");
    if (v->type == JSON_BOOL) return gerr_dup(v->bool_val ? "True" : "False");
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        size_t n = 2, i;
        char *out, *p;
        for (i = 0; s[i]; i++) n += (s[i] == '\'' || s[i] == '\\' || s[i] == '\n'
                                     || s[i] == '\t' || s[i] == '\r') ? 2 : 1;
        out = (char *)malloc(n + 1);
        if (!out) return NULL;
        p = out;
        *p++ = '\'';
        for (i = 0; s[i]; i++) {
            switch (s[i]) {
            case '\'': *p++ = '\\'; *p++ = '\''; break;
            case '\\': *p++ = '\\'; *p++ = '\\'; break;
            case '\n': *p++ = '\\'; *p++ = 'n'; break;
            case '\t': *p++ = '\\'; *p++ = 't'; break;
            case '\r': *p++ = '\\'; *p++ = 'r'; break;
            default:   *p++ = s[i]; break;
            }
        }
        *p++ = '\'';
        *p = '\0';
        return out;
    }
    if (v->type == JSON_NUMBER) {
        char buf[64];
        gerr_num_str(v->num_val, buf, sizeof(buf));
        return gerr_dup(buf);
    }
    return gerr_dup("...");  /* nested containers are out of contract */
}

/* Python str() of a decoded body: text as-is, a list rendered from reprs. */
static char *gerr_str(const json_t *v) {
    if (!v || v->type == JSON_NULL) return gerr_dup("None");
    if (v->type == JSON_BOOL) return gerr_dup(v->bool_val ? "True" : "False");
    if (v->type == JSON_STRING) return gerr_dup(v->str_val ? v->str_val : "");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        gerr_num_str(v->num_val, buf, sizeof(buf));
        return gerr_dup(buf);
    }
    if (v->type == JSON_ARRAY) {
        size_t i, n = 3, off;
        char *out;
        for (i = 0; i < v->c.count; i++) {
            char *r = gerr_repr(v->c.items[i]);
            n += strlen(r ? r : "") + 2;
            free(r);
        }
        out = (char *)malloc(n + 1);
        if (!out) return NULL;
        off = 0;
        out[off++] = '[';
        for (i = 0; i < v->c.count; i++) {
            char *r = gerr_repr(v->c.items[i]);
            if (i) { out[off++] = ','; out[off++] = ' '; }
            if (r) {
                size_t rn = strlen(r);
                memcpy(out + off, r, rn);
                off += rn;
                free(r);
            }
        }
        out[off++] = ']';
        out[off] = '\0';
        return out;
    }
    return gerr_dup("...");  /* nested object body: out of contract */
}

/* `status >= 500` and f"HTTP_{status}" both need an int.  A bool counts because
 * Python's True == 1, though its f-string still prints "True"; any other type
 * makes the live function's `status >= 500` raise TypeError. */
static int gerr_to_int(const json_t *v, long long *out, const char **text) {
    *text = NULL;
    if (!v) return 0;
    if (v->type == JSON_BOOL) {
        *out = v->bool_val ? 1 : 0;
        *text = v->bool_val ? "True" : "False";
        return 1;
    }
    if (v->type == JSON_NUMBER) {
        if (!isfinite(v->num_val)) return 0;
        if (v->num_val != (double)(long long)v->num_val) return 0;
        *out = (long long)v->num_val;
        return 1;
    }
    return 0;
}

/* float(x) for the Retry-After header: ok on success, 0 when Python raises. */
static int gerr_to_float(const char *s, double *out) {
    char *end = NULL;
    double got;
    if (!s) return 0;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    got = strtod(s, &end);
    if (end == s) return 0;
    while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r') end++;
    if (*end) return 0;
    *out = got;
    return 1;
}

static int gerr_expired(const char *code) {
    return code && !strcmp(code, "CONNECTION_EXPIRED");
}

json_t *w21_errors_5bcef1_parse(const json_t *status, const json_t *body,
                                const json_t *headers) {
    json_t *out = json_object();
    char code[128], err_msg[256];
    char *message = NULL;
    char *request_id = NULL;
    long long st = 0;
    const char *st_text = NULL;
    double retry_after = 0.0, rams = 0.0;
    int has_rams = 0;
    const char *type;
    int retryable = 0;
    if (!out) return NULL;
    if (!gerr_to_int(status, &st, &st_text)) {
        json_set(out, "error_type", json_string("TypeError"));
        return out;
    }
    if (st_text) snprintf(code, sizeof(code), "HTTP_%s", st_text);
    else snprintf(code, sizeof(code), "HTTP_%lld", st);

    if (body && body->type == JSON_OBJECT) {
        json_t *envelope = json_obj_get(body, "error");
        if (envelope && envelope->type == JSON_OBJECT) {
            json_t *c = json_obj_get(envelope, "code");
            json_t *m = json_obj_get(envelope, "message");
            if (gerr_truthy(c)) {
                char *s = gerr_str(c);
                snprintf(code, sizeof(code), "%s", s ? s : "");
                free(s);
            }
            if (gerr_truthy(m)) message = gerr_str(m);
            else message = gerr_dup("");
        } else if (envelope && envelope->type == JSON_STRING && gerr_truthy(envelope)) {
            snprintf(code, sizeof(code), "%s", envelope->str_val ? envelope->str_val : "");
        } else if (gerr_truthy(json_obj_get(body, "code"))) {
            char *s = gerr_str(json_obj_get(body, "code"));
            json_t *m = json_obj_get(body, "message");
            snprintf(code, sizeof(code), "%s", s ? s : "");
            free(s);
            message = gerr_truthy(m) ? gerr_str(m) : gerr_dup("");
        }
        json_t *rid = json_obj_get(body, "requestId");
        if (rid && rid->type != JSON_NULL) request_id = gerr_str(rid);
        {
            json_t *rams_v = json_obj_get(body, "retryAfterMs");
            if (rams_v && rams_v->type == JSON_NUMBER) { rams = rams_v->num_val; has_rams = 1; }
            else if (rams_v && rams_v->type == JSON_BOOL) { rams = rams_v->bool_val ? 1.0 : 0.0; has_rams = 1; }
        }
    } else if (gerr_truthy(body)) {
        char *s = gerr_str(body);
        if (s && strlen(s) > 500) s[500] = '\0';
        message = s;
    }
    if (!message) message = gerr_dup("");
    if (!message[0]) {
        if (st_text)
            snprintf(err_msg, sizeof(err_msg), "tool gateway request failed with status %s", st_text);
        else
            snprintf(err_msg, sizeof(err_msg), "tool gateway request failed with status %lld", st);
        free(message);
        message = gerr_dup(err_msg);
    }

    if (st == 401 || st == 403) {
        type = "GatewayAuthError";
    } else if (st == 404) {
        type = "GatewayUnavailable";
    } else if (st == 409) {
        type = "IdempotencyConflict";
    } else if (st == 429 && !st_text) {
        /* `header not in (None, "")`: an empty or absent header falls through to
         * the envelope's retryAfterMs, else one second. */
        json_t *h = (headers && headers->type == JSON_OBJECT)
                        ? json_obj_get(headers, "Retry-After") : NULL;
        int use_header = 0;
        type = "RateLimited";
        if (h && h->type == JSON_NULL) use_header = 0;
        else if (h && h->type == JSON_STRING) use_header = gerr_truthy(h);
        else if (h) use_header = 1;
        if (use_header) {
            if (h->type == JSON_NUMBER) retry_after = h->num_val;
            else if (h->type == JSON_BOOL) retry_after = h->bool_val ? 1.0 : 0.0;
            else {
                const char *txt = h->str_val ? h->str_val : "";
                if (!gerr_to_float(txt, &retry_after)) {
                    json_free(out);
                    out = json_object();
                    json_set(out, "error_type", json_string("ValueError"));
                    free(message);
                    free(request_id);
                    return out;
                }
            }
        } else {
            retry_after = (has_rams && rams != 0.0 ? rams : 1000.0) / 1000.0;
        }
    } else {
        type = "ToolGatewayError";
        retryable = st >= 500;
    }

    json_set(out, "type", json_string(type));
    json_set(out, "message", json_string(message ? message : ""));
    json_set(out, "code", json_string(code));
    json_set(out, "status", json_number((double)st));
    json_set(out, "request_id", request_id ? json_string(request_id) : json_null());
    json_set(out, "retryable", json_bool(retryable));
    if (st == 429 && !st_text) json_set(out, "retry_after", json_number(retry_after));
    free(message);
    free(request_id);
    return out;
}

char *w21_errors_5bcef1_default_message(const char *code, const char *connector) {
    char buf[1024];
    const char *app;
    char *tmp;
    if (connector && connector[0]) {
        size_t n = strlen(connector) + 20;
        tmp = (char *)malloc(n);
        if (!tmp) return NULL;
        snprintf(tmp, n, "The %s connector", connector);
        app = tmp;
    } else {
        tmp = NULL;
        app = "This connector";
    }
    if (code && !strcmp(code, "CONNECTION_EXPIRED"))
        snprintf(buf, sizeof(buf), "%s connection for this account has expired and must be reconnected.", app);
    else
        snprintf(buf, sizeof(buf), "%s is not connected for this account.", app);
    free(tmp);
    return gerr_dup(buf);
}

/* The hint table as `_HINTS.get(code, _HINTS["CONNECTION_REQUIRED"])`: an
 * unknown code answers the CONNECTION_REQUIRED row. */
json_t *w21_errors_5bcef1_render(const json_t *req) {
    const char *connector = req ? json_get_str(req, "connector", "") : "";
    const char *message = req ? json_get_str(req, "message", "") : "";
    const char *connect_url = req ? json_get_str(req, "connect_url", "") : "";
    const char *hint = req ? json_get_str(req, "hint", "") : "";
    const char *surface = req ? json_get_str(req, "surface", GERR_LINK) : GERR_LINK;
    const char *code = req ? json_get_str(req, "code", "CONNECTION_REQUIRED") : "CONNECTION_REQUIRED";
    json_t *payload = json_object();
    int expired;
    if (!payload) return NULL;
    expired = gerr_expired(code);
    json_set(payload, "code", json_string(code));
    if (message && message[0]) {
        json_set(payload, "message", json_string(message));
    } else {
        char *m = w21_errors_5bcef1_default_message(code, connector);
        json_set(payload, "message", json_string(m ? m : ""));
        free(m);
    }
    if (connector && connector[0]) json_set(payload, "connector", json_string(connector));
    if (!strcmp(surface, GERR_SIDE)) {
        json_set(payload, "hint", json_string(expired ? GERR_EXPIRED_SIDE_AGENT_HINT
                                                      : GERR_SIDE_AGENT_HINT));
        return payload;
    }
    if (!strcmp(surface, GERR_CARD)) {
        json_set(payload, "connect_card_available", json_bool(1));
        json_set(payload, "hint", json_string(expired ? GERR_EXPIRED_CARD_HINT : GERR_CARD_HINT));
        return payload;
    }
    if (connect_url && connect_url[0]) {
        json_set(payload, "connect_url", json_string(connect_url));
        if (!(hint && hint[0]) && expired) hint = GERR_EXPIRED_LINK_HINT;
    }
    if (hint && hint[0]) json_set(payload, "hint", json_string(hint));
    return payload;
}
