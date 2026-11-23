/*
 * rpc_dispatch.c — C home for upstream Hermes tui_gateway/rpc_dispatch.py.
 *
 * PoP: w11_rpcdispatc_c1798d_admit @ tui_gateway/rpc_dispatch.py:_handle_admitted_request
 *
 * Faithful port of the JSON-RPC admission path: normalize -> method lookup ->
 * contract validation -> handler -> exception mapping -> result checks. Every
 * collaborator (_normalize_request, _methods, _contracts, _err,
 * _session_not_owned_err, the three exception classes) is injected through
 * the harness `io` seam, mirroring the oracle's live-module stubs.
 */
#include "noc_w11_rpcdispatc_c1798d.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static char *rd_dup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}
static const char *rd_s(const char *s) { return s ? s : ""; }
static json_t *rd_val_or_null(json_t *v) { return v ? json_copy(v) : json_null(); }

static void rd_trace(json_t *trace, const char *name, json_t *args) {
    json_t *e = json_object();
    json_set(e, "name", json_string(name));
    json_set(e, "args", args);
    json_append(trace, e);
}
static json_t *rd_args1(const char *k1, json_t *v1) {
    json_t *o = json_object();
    json_set(o, k1, v1);
    return o;
}
static json_t *rd_args2(const char *k1, json_t *v1, const char *k2, json_t *v2) {
    json_t *o = json_object();
    json_set(o, k1, v1); json_set(o, k2, v2);
    return o;
}
static json_t *rd_args3(const char *k1, json_t *v1, const char *k2, json_t *v2,
                        const char *k3, json_t *v3) {
    json_t *o = json_object();
    json_set(o, k1, v1); json_set(o, k2, v2); json_set(o, k3, v3);
    return o;
}

/* _err -> marker dict: {"__err__": {"rid","code","message"}} */
static json_t *rd_err(json_t *rid, int code, const char *msg) {
    json_t *d = json_object();
    json_set(d, "rid", rd_val_or_null(rid));
    json_set(d, "code", json_number((double)code));
    json_set(d, "message", json_string(rd_s(msg)));
    json_t *o = json_object();
    json_set(o, "__err__", d);
    return o;
}
static json_t *rd_sno(json_t *rid, const char *msg) {
    json_t *d = json_object();
    json_set(d, "rid", rd_val_or_null(rid));
    json_set(d, "message", json_string(rd_s(msg)));
    json_t *o = json_object();
    json_set(o, "__sno__", d);
    return o;
}

/* Python str(v) for the code of ConfigBackendUnavailable. */
static char *rd_pystr(json_t *v) {
    if (!v) return rd_dup("");
    if (v->type == JSON_STRING) return rd_dup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return rd_dup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) {
        char b[64];
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(b, sizeof(b), "%.0f", v->num_val);
        else snprintf(b, sizeof(b), "%.17g", v->num_val);
        return rd_dup(b);
    }
    char *s = json_dumps(v, 0);
    return s ? s : rd_dup("");
}

/* f-template with a single {method} placeholder. */
static char *rd_fmt_method(const char *tmpl, const char *method) {
    const char *ph = strstr(tmpl, "{method}");
    if (!ph) return rd_dup(tmpl);
    size_t pre = (size_t)(ph - tmpl);
    size_t n = pre + strlen(method) + strlen(ph + 8) + 1;
    char *out = (char *)malloc(n);
    if (out) snprintf(out, n, "%.*s%s%s", (int)pre, tmpl, method, ph + 8);
    return out;
}

static int rd_in_methods(json_t *methods, const char *method) {
    if (!methods || methods->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(methods); i++) {
        json_t *v = json_get(methods, i);
        if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, method)) return 1;
    }
    return 0;
}

json_t *w11_rpcdispatc_c1798d_admit(json_t *req, json_t *io) {
    json_t *trace = json_array();
    json_t *ret = NULL;

    /* normalized = _normalize_request(req) */
    rd_trace(trace, "normalize", rd_args1("req", rd_val_or_null(req)));
    json_t *norm = io ? json_obj_get(io, "normalize") : NULL;
    json_t *direct = norm ? json_obj_get(norm, "__response__") : NULL;
    if (direct) {
        ret = json_copy(direct);
        goto done;
    }
    json_t *rid = norm ? json_obj_get(norm, "rid") : NULL;
    const char *method = json_get_str(norm, "method", "");
    json_t *params = norm ? json_obj_get(norm, "params") : NULL;
    if (!params) params = json_null();

    json_t *methods = io ? json_obj_get(io, "methods") : NULL;
    json_t *handler = json_obj_get(methods, method);
    if (!handler) {
        json_t *contracts = io ? json_obj_get(io, "contracts") : NULL;
        const char *tmpl = json_get_str(contracts, "unknown_message", "Unknown method: {method}");
        rd_trace(trace, "unknown_method_message",
                 rd_args1("method", json_string(rd_s(method))));
        char *msg = rd_fmt_method(tmpl, method);
        ret = rd_err(rid, -32601, msg);
        free(msg);
        rd_trace(trace, "err", rd_args3("rid", rd_val_or_null(rid),
                                        "code", json_number(-32601.0),
                                        "message", json_string(rd_s(
                                            json_get_str(json_obj_get(ret, "__err__"),
                                                         "message", "")))));
        goto done;
    }

    json_t *contracts = io ? json_obj_get(io, "contracts") : NULL;
    int has_contract = rd_in_methods(contracts ? json_obj_get(contracts, "methods") : NULL, method);
    if (has_contract) {
        json_t *val = json_obj_get(contracts, "validate");
        rd_trace(trace, "validate_params",
                 rd_args2("method", json_string(rd_s(method)), "params", json_copy(params)));
        const char *problem = json_get_str(val, "problem", "");
        if (problem && *problem) {
            ret = rd_err(rid, 4000, problem);
            rd_trace(trace, "err", rd_args3("rid", rd_val_or_null(rid),
                                            "code", json_number(4000.0),
                                            "message", json_string(problem)));
            goto done;
        }
        json_t *np = json_obj_get(val, "params");
        if (np) params = np;
    }

    rd_trace(trace, "handler", rd_args3("rid", rd_val_or_null(rid),
                                        "method", json_string(rd_s(method)),
                                        "params", json_copy(params)));
    json_t *raise_v = json_obj_get(handler, "__raise__");
    const char *raise = (raise_v && raise_v->type == JSON_STRING) ? raise_v->str_val : NULL;
    if (raise && !strcmp(raise, "ProfileUnavailableError")) {
        const char *msg = json_get_str(handler, "message", "");
        ret = rd_err(rid, 4064, msg);
        rd_trace(trace, "err", rd_args3("rid", rd_val_or_null(rid),
                                        "code", json_number(4064.0),
                                        "message", json_string(rd_s(msg))));
    } else if (raise && !strcmp(raise, "SessionNotOwned")) {
        const char *msg = json_get_str(handler, "message", "");
        ret = rd_sno(rid, msg);
        rd_trace(trace, "session_not_owned_err",
                 rd_args2("rid", rd_val_or_null(rid), "message", json_string(rd_s(msg))));
    } else if (raise && !strcmp(raise, "ConfigBackendUnavailable")) {
        char *code = rd_pystr(json_obj_get(handler, "code"));
        ret = rd_err(rid, 4064, code);
        rd_trace(trace, "err", rd_args3("rid", rd_val_or_null(rid),
                                        "code", json_number(4064.0),
                                        "message", json_string(rd_s(code))));
        free(code);
    } else {
        json_t *hresp = json_obj_get(handler, "__response__");
        ret = hresp ? json_copy(hresp) : json_null();
    }

    if (has_contract && ret && ret->type == JSON_OBJECT) {
        json_t *result = json_obj_get(ret, "result");
        if (result && result->type == JSON_OBJECT) {
            rd_trace(trace, "check_params_accepted",
                     rd_args1("method", json_string(rd_s(method))));
            rd_trace(trace, "check_result",
                     rd_args1("method", json_string(rd_s(method))));
        }
    }

done:
    if (!ret) ret = json_null();
    json_t *out = json_object();
    json_set(out, "ret", ret);
    json_set(out, "trace", trace);
    return out;
}
