/*
 * t_port_relay_adapter_flow.c — oracle harness for the gateway/relay/adapter.py
 * FLOW cores ported in src/gateway/relay/relay_adapter_flow.c.
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * Wires each flow's seams to the SCRIPTED results in "in" (recording stubs
 * that append one trace event per seam call, in call order) and emits
 * {"func": ..., "in": ..., "out": {"ret": ..., "trace": [...], ...}} per case.
 * tests/oracle_relay_adapter_flow.py drives the LIVE Python with the same
 * scripts.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "libjson/json.h"
#include "relay_adapter_flow.h"

typedef struct {
    const json_t *in;
    radapter_trace_t *trace;
} script_ctx_t;

/* ── event builders ── */
static json_t *ev(const char *name) {
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    return e;
}
static void evs(json_t *e, const char *k, const char *v) {
    json_set(e, k, v ? json_string(v) : json_null());
}
static void evb(json_t *e, const char *k, bool v) { json_set(e, k, json_bool(v)); }
static void evn(json_t *e, const char *k, double v) { json_set(e, k, json_number(v)); }
static void evj(json_t *e, const char *k, const json_t *v) {
    json_set(e, k, v ? json_copy(v) : json_null());
}
static void rec(script_ctx_t *c, json_t *e) { radapter_flow_trace_add(c->trace, e); }

static const json_t *in_obj(const script_ctx_t *c, const char *k) {
    const json_t *v = json_obj_get(c->in, k);
    return (v && v->type == JSON_OBJECT) ? v : NULL;
}
static bool in_bool(const script_ctx_t *c, const char *k) {
    return json_get_bool(c->in, k, false);
}
static const char *in_str(const script_ctx_t *c, const char *k, const char *def) {
    return json_get_str(c->in, k, def);
}
/* str-or-null kwarg: absent AND explicit null both read as None. */
static const char *in_str_null(const script_ctx_t *c, const char *k) {
    const json_t *v = json_obj_get(c->in, k);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static const json_t *in_state(const script_ctx_t *c) {
    const json_t *s = in_obj(c, "state");
    return s;
}

/* ── shared outbound seams (_gated_op / _outbound / _typing_frame) ── */
static const json_t *op_send(void *ctx, const json_t *action, const char *platform,
                             bool *raised) {
    script_ctx_t *c = ctx;
    *raised = false;
    json_t *e = ev("send_outbound");
    evj(e, "action", action);
    evs(e, "platform", platform);
    if (in_bool(c, "send_raises")) {
        evb(e, "raised", true);
        rec(c, e);
        *raised = true;
        return NULL;
    }
    const json_t *r = json_obj_get(c->in, "send_ret");
    evj(e, "ret", r);
    rec(c, e);
    return r;
}
static void op_log_debug(void *ctx, const char *msg) {
    script_ctx_t *c = ctx;
    json_t *e = ev("log");
    evs(e, "level", "debug");
    evs(e, "msg", msg);
    rec(c, e);
}
static void op_log_level(void *ctx, int level, const char *msg) {
    script_ctx_t *c = ctx;
    json_t *e = ev("log");
    evn(e, "level", level);
    evs(e, "msg", msg);
    rec(c, e);
}
static void op_log_warning(void *ctx, const char *msg) {
    script_ctx_t *c = ctx;
    json_t *e = ev("log");
    evs(e, "level", "warning");
    evs(e, "msg", msg);
    rec(c, e);
}

/* ── profile namespace seam ── */
static const char *pf_namespace(void *ctx, const char *ns) {
    script_ctx_t *c = ctx;
    const char *r = in_str_null(c, "namespace_profile");
    json_t *e = ev("resolve_namespace");
    evs(e, "ns", ns);
    evs(e, "ret", r);
    rec(c, e);
    return r;
}

/* ── negotiated descriptor seam ── */
static const json_t *nd_resolve(void *ctx, const char *platform, bool *raised) {
    script_ctx_t *c = ctx;
    *raised = false;
    json_t *e = ev("descriptor_for_platform");
    evs(e, "platform", platform);
    if (in_bool(c, "resolve_raises")) {
        evb(e, "raised", true);
        rec(c, e);
        *raised = true;
        return NULL;
    }
    const json_t *r = json_obj_get(c->in, "resolve_ret");
    evj(e, "ret", r);
    rec(c, e);
    return r;
}

/* ── redial toggle seam ── */
static void tg_call(void *ctx, const char *method_name, bool *raised) {
    script_ctx_t *c = ctx;
    *raised = false;
    json_t *e = ev("toggle");
    evs(e, "method", method_name);
    if (in_bool(c, "toggle_raises")) {
        evb(e, "raised", true);
        rec(c, e);
        *raised = true;
        return;
    }
    rec(c, e);
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    radapter_trace_t trace = {0};
    script_ctx_t ctx = {in, &trace};

    if (strcmp(func, "utf16_len") == 0) {
        int r = radapter_flow_utf16_len(in_str(&ctx, "text", ""));
        json_set(out, "ret", json_number(r));
    } else if (strcmp(func, "send_result") == 0) {
        json_t *r = radapter_flow_send_result(in_obj(&ctx, "result"),
                                              in_obj(&ctx, "extra"));
        json_set(out, "ret", r ? r : json_null());
    } else if (strcmp(func, "event_ids") == 0) {
        json_t *r = radapter_flow_event_ids(in_obj(&ctx, "event"));
        json_set(out, "ret", r ? r : json_null());
    } else if (strcmp(func, "profile_from_session_key") == 0) {
        radapter_profile_seams_t s = {.ctx = &ctx, .resolve_namespace = pf_namespace};
        char *r = radapter_flow_profile_from_session_key(in_str_null(&ctx, "session_key"), &s);
        json_set(out, "ret", r ? json_string(r) : json_null());
        free(r);
    } else if (strcmp(func, "negotiated_descriptor") == 0) {
        radapter_desc_seams_t s = {.ctx = &ctx, .resolve = nd_resolve};
        json_t *r = radapter_flow_negotiated_descriptor(
            in_state(&ctx), in_str_null(&ctx, "platform"), &s);
        json_set(out, "ret", r ? r : json_null());
    } else if (strcmp(func, "metrics_platform") == 0) {
        json_t *r = radapter_flow_metrics_platform(in_state(&ctx),
                                                   in_str(&ctx, "chat_id", ""));
        json_set(out, "ret", r ? r : json_null());
    } else if (strcmp(func, "slack_unfurl_hints") == 0) {
        json_t *r = radapter_flow_slack_unfurl_hints(in_state(&ctx),
                                                     in_str_null(&ctx, "platform"));
        json_set(out, "ret", r ? r : json_null());
    } else if (strcmp(func, "prefers_fresh_final_streaming") == 0) {
        const json_t *chat_id = json_obj_get(in, "chat_id");
        if (chat_id && chat_id->type == JSON_NULL) chat_id = NULL;
        bool r = radapter_flow_prefers_fresh_final_streaming(
            in_state(&ctx), in_str_null(&ctx, "content"),
            in_obj(&ctx, "metadata"), chat_id);
        json_set(out, "ret", json_bool(r));
    } else if (strcmp(func, "gated_op") == 0) {
        radapter_op_seams_t s = {
            .ctx = &ctx, .send_outbound = op_send,
            .log_debug = op_log_debug, .log_level = op_log_level,
            .log_warning = op_log_warning,
        };
        const json_t *dl = json_obj_get(in, "decline_level");
        int decline_level = (dl && dl->type == JSON_NUMBER) ? (int)dl->num_val : -1;
        const json_t *subject = json_obj_get(in, "subject");
        if (subject && subject->type == JSON_NULL) subject = NULL;
        json_t *r = radapter_flow_gated_op(
            in_state(&ctx), in_str(&ctx, "chat_id", ""), in_obj(&ctx, "action"),
            decline_level, subject, in_str_null(&ctx, "platform"),
            in_bool(&ctx, "surface_declines"), in_bool(&ctx, "surface_ambiguous"), &s);
        json_set(out, "ret", r ? r : json_null());
    } else if (strcmp(func, "outbound") == 0) {
        radapter_op_seams_t s = {
            .ctx = &ctx, .send_outbound = op_send,
            .log_debug = op_log_debug, .log_level = op_log_level,
            .log_warning = op_log_warning,
        };
        bool raised = false;
        json_t *r = radapter_flow_outbound(in_state(&ctx), in_str(&ctx, "chat_id", ""),
                                           in_obj(&ctx, "action"), &raised, &s);
        json_set(out, "ret", r ? r : json_null());
        json_set(out, "raised", json_bool(raised));
    } else if (strcmp(func, "toggle_transport_redial") == 0) {
        radapter_toggle_seams_t s = {.ctx = &ctx, .call = tg_call,
                                     .log_debug = op_log_debug};
        bool r = radapter_flow_toggle_transport_redial(
            in_state(&ctx), in_str(&ctx, "method_name", "hold_redial"), &s);
        json_set(out, "ret", json_bool(r));
    } else if (strcmp(func, "ack_emoji") == 0) {
        json_t *r = radapter_flow_ack_emoji(in_state(&ctx), in_obj(&ctx, "event"),
                                            in_str(&ctx, "chat_id", ""));
        json_set(out, "ret", r ? r : json_null());
    } else if (strcmp(func, "stamp_format_hints") == 0) {
        const json_t *desc = json_obj_get(in, "descriptor");
        if (desc && desc->type != JSON_OBJECT) desc = NULL;
        json_t *r = radapter_flow_stamp_format_hints(
            in_state(&ctx), desc, in_str_null(&ctx, "platform"),
            in_obj(&ctx, "metadata"));
        json_set(out, "ret", r ? r : json_null());
    } else if (strcmp(func, "typing_frame") == 0) {
        radapter_op_seams_t s = {
            .ctx = &ctx, .send_outbound = op_send,
            .log_debug = op_log_debug, .log_level = op_log_level,
            .log_warning = op_log_warning,
        };
        const json_t *content = json_obj_get(in, "content");
        radapter_flow_typing_frame(in_state(&ctx), in_str(&ctx, "chat_id", ""),
                                   in_obj(&ctx, "metadata"), content,
                                   in_str(&ctx, "lane", "send_typing"), &s);
        json_set(out, "ret", json_null());
    } else {
        json_set(out, "error", json_string("unknown func"));
    }

    json_set(out, "trace", trace.events ? trace.events : json_array());
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    FILE *f = (argc > 1) ? fopen(argv[1], "rb") : stdin;
    if (!f) { fprintf(stderr, "cannot open fixture\n"); return 1; }
    char line[1 << 20];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '\0' || line[0] == '\n' || line[0] == '#') continue;
        char *err = NULL;
        json_t *c = json_parse(line, &err);
        if (err || !c || c->type != JSON_OBJECT) {
            fprintf(stderr, "bad case line: %s\n", err ? err : "not an object");
            free(err);
            json_free(c);
            continue;
        }
        const char *func = json_get_str(c, "func", "");
        const json_t *in = json_obj_get(c, "in");
        json_t *empty = json_object();
        json_t *out = json_object();
        run_case(func, in ? in : empty, out);
        json_free(empty);
        char *in_s = json_serialize(in);
        char *out_s = json_serialize(out);
        /* Serialize a string node for func: json_escape_string has an
         * off-by-one heap overflow (see t_port_gateway_flow.c). */
        json_t *func_node = json_string(func);
        char *func_s = json_serialize(func_node);
        json_free(func_node);
        printf("{\"func\":%s,\"in\":%s,\"out\":%s}\n", func_s,
               in_s ? in_s : "null", out_s ? out_s : "null");
        free(in_s); free(out_s); free(func_s);
        json_free(out);
        json_free(c);
    }
    if (f != stdin) fclose(f);
    return 0;
}