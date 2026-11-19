/*
 * t_port_bedrock_adapter_flow.c — oracle harness for the agent/bedrock_adapter.py
 * FLOW + leaf port in src/agent/bedrock_adapter_flow.c (self-contained include).
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * Wires the port's seams to the SCRIPTED results in "in.io" (recording stubs
 * appending one {"ev","args","ret"} trace event per seam call, in call order)
 * and emits {"func": ..., "in": ..., "out": {"ret": ..., "trace": [...]}}.
 * strip_redacted_reasoning also carries "same" (live same-object identity).
 * tests/oracle_bedrock_adapter_flow.py drives the LIVE Python with the same
 * scripts. Script values: plain JSON reused verbatim; {"__raise__": cls,
 * "msg": ...} raises; absent key -> None. resolve_profile is a map
 * arn -> resolved (missing arn -> the arn itself, the unresolved fallback).
 */
#include "agent/bedrock_adapter_flow.c"

typedef struct {
    const json_t *in;
    brd_flow_trace_t *trace;
} script_ctx_t;

static json_t *ev(const char *name) {
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", json_object());
    return e;
}
static void evj(json_t *e, const char *k, const json_t *v) {
    json_set(e, k, v ? json_copy(v) : json_null());
}
static void rec(script_ctx_t *c, json_t *e) { brd_flow_trace_add(c->trace, e); }

static const json_t *io_get(const script_ctx_t *c, const char *name) {
    const json_t *io = json_obj_get(c->in, "io");
    return json_obj_get(io, name);
}
static bool is_raise_marker(const json_t *v) {
    const json_t *r = json_obj_get(v, "__raise__");
    return v && v->type == JSON_OBJECT && r && r->type == JSON_STRING;
}
static json_t *raise_marker_copy(const json_t *v) {
    json_t *m = json_object();
    json_set(m, "__raise__", json_string(json_get_str(v, "__raise__", "Exception")));
    json_set(m, "msg", json_string(json_get_str(v, "msg", "")));
    return m;
}

/* ── seam wrappers (record + return the scripted outcome) ── */
static const char *sm_home_override(void *ctx) {
    script_ctx_t *c = ctx;
    const json_t *v = io_get(c, "home_override");
    json_t *e = ev("home_override");
    evj(e, "ret", v);
    rec(c, e);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static const json_t *sm_secret_scope(void *ctx) {
    script_ctx_t *c = ctx;
    const json_t *v = io_get(c, "secret_scope");
    json_t *e = ev("secret_scope");
    evj(e, "ret", v);
    rec(c, e);
    return (v && v->type == JSON_OBJECT) ? v : NULL;
}
static const json_t *sm_multiplex_active(void *ctx) {
    script_ctx_t *c = ctx;
    const json_t *v = io_get(c, "multiplex_active");
    json_t *e = ev("multiplex_active");
    evj(e, "ret", v);
    rec(c, e);
    return v;
}
static const json_t *sm_load_config(void *ctx, bool *raised) {
    script_ctx_t *c = ctx;
    const json_t *v = io_get(c, "load_config");
    json_t *e = ev("load_config");
    *raised = false;
    if (is_raise_marker(v)) {
        json_set(e, "ret", raise_marker_copy(v));
        rec(c, e);
        *raised = true;
        return NULL;
    }
    evj(e, "ret", v);
    rec(c, e);
    return (v && v->type == JSON_OBJECT) ? v : NULL;
}
static const char *sm_botocore_region(void *ctx, bool *raised) {
    script_ctx_t *c = ctx;
    const json_t *v = io_get(c, "botocore_region");
    json_t *e = ev("botocore_region");
    json_set(json_obj_get(e, "args"), "variable", json_string("region"));
    *raised = false;
    if (is_raise_marker(v)) {
        json_set(e, "ret", raise_marker_copy(v));
        rec(c, e);
        *raised = true;
        return NULL;
    }
    evj(e, "ret", v);
    rec(c, e);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static const char *sm_resolve_profile(void *ctx, const char *arn) {
    script_ctx_t *c = ctx;
    const json_t *map = io_get(c, "resolve_profile");
    const json_t *r = json_obj_get(map, arn);
    const char *ret = (r && r->type == JSON_STRING) ? r->str_val : arn;
    json_t *e = ev("resolve_profile");
    json_set(json_obj_get(e, "args"), "arn", json_string(arn));
    json_set(e, "ret", json_string(ret));
    rec(c, e);
    return ret;
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    brd_flow_trace_t trace = {0};
    script_ctx_t ctx = {in, &trace};
    brd_flow_seams_t s = {
        .ctx = &ctx,
        .home_override = sm_home_override,
        .secret_scope = sm_secret_scope,
        .multiplex_active = sm_multiplex_active,
        .load_config = sm_load_config,
        .botocore_region = sm_botocore_region,
        .resolve_profile = sm_resolve_profile,
    };
    const json_t *cfg = json_obj_get(in, "config");
    if (cfg && cfg->type == JSON_NULL) cfg = NULL;

    if (strcmp(func, "scoped_aws_session_kwargs") == 0) {
        json_set(out, "ret", brd_flow_scoped_aws_session_kwargs(&s));
    } else if (strcmp(func, "resolve_bedrock_runtime_region") == 0) {
        char *r = brd_flow_resolve_bedrock_runtime_region(
            cfg, json_obj_get(in, "env"), &s);
        json_set(out, "ret", json_string(r ? r : ""));
        free(r);
    } else if (strcmp(func, "bedrock_region_from_runtime_url") == 0) {
        char *r = brd_flow_bedrock_region_from_runtime_url(
            json_get_str(in, "base_url", NULL));
        json_set(out, "ret", json_string(r ? r : ""));
        free(r);
    } else if (strcmp(func, "parse_bedrock_openai_model_id") == 0) {
        json_t *r = brd_flow_parse_bedrock_openai_model_id(
            json_obj_get(in, "model_id"));
        json_set(out, "ret", r ? r : json_null());
    } else if (strcmp(func, "_mantle_url_parts") == 0) {
        json_set(out, "ret", brd_flow_mantle_url_parts(json_obj_get(in, "base_url")));
    } else if (strcmp(func, "bedrock_guardrail_config") == 0) {
        json_t *r = brd_flow_bedrock_guardrail_config(cfg, &s);
        json_set(out, "ret", r ? r : json_null());
    } else if (strcmp(func, "bedrock_guardrail_headers") == 0) {
        json_set(out, "ret", brd_flow_bedrock_guardrail_headers(cfg, &s));
    } else if (strcmp(func, "anthropic_response_guardrail_intervened") == 0) {
        bool r = brd_flow_anthropic_response_guardrail_intervened(
            json_obj_get(in, "response"));
        json_set(out, "ret", json_bool(r));
    } else if (strcmp(func, "_parse_tool_args") == 0) {
        json_set(out, "ret", brd_flow_parse_tool_args(json_obj_get(in, "args")));
    } else if (strcmp(func, "_decode_redacted") == 0) {
        json_t *r = brd_flow_decode_redacted(json_obj_get(in, "encoded"));
        json_set(out, "ret", r ? r : json_null());
    } else if (strcmp(func, "_image_block_from_data_url") == 0) {
        json_set(out, "ret", brd_flow_image_block_from_data_url(
            json_get_str(in, "url", "")));
    } else if (strcmp(func, "_system_blocks") == 0) {
        json_set(out, "ret", brd_flow_system_blocks(json_obj_get(in, "content")));
    } else if (strcmp(func, "_replay_ordered_blocks") == 0) {
        json_set(out, "ret", brd_flow_replay_ordered_blocks(
            json_obj_get(in, "blocks")));
    } else if (strcmp(func, "_reasoning_details_blocks") == 0) {
        json_set(out, "ret", brd_flow_reasoning_details_blocks(
            json_obj_get(in, "msg"), json_get_bool(in, "claude", false),
            json_get_bool(in, "own", false)));
    } else if (strcmp(func, "_assistant_blocks") == 0) {
        json_set(out, "ret", brd_flow_assistant_blocks(
            json_obj_get(in, "msg"), json_obj_get(in, "content"),
            json_get_bool(in, "claude", true), json_get_bool(in, "own", false)));
    } else if (strcmp(func, "_converse_replay_policy") == 0) {
        const json_t *m = json_obj_get(in, "model");
        const char *r = brd_flow_converse_replay_policy(
            (m && m->type == JSON_STRING) ? m->str_val : NULL, &s);
        json_set(out, "ret", json_string(r));
    } else if (strcmp(func, "_reasoning_filter") == 0) {
        const json_t *iss = json_obj_get(in, "issuer");
        json_set(out, "ret", brd_flow_reasoning_filter(
            json_get_str(in, "policy", ""),
            (iss && iss->type == JSON_STRING) ? iss->str_val : NULL,
            json_get_bool(in, "own", false), json_get_bool(in, "in_loop", false),
            &s));
    } else if (strcmp(func, "strip_redacted_reasoning") == 0) {
        bool same = false;
        json_t *kw = json_obj_get(in, "kwargs");
        json_t *empty = json_object();
        json_t *r = brd_flow_strip_redacted_reasoning(kw ? kw : empty, &same);
        json_free(empty);
        json_set(out, "ret", r);
        json_set(out, "same", json_bool(same));
    } else {
        json_set(out, "ret", raise_desc("ValueError", "unknown func"));
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
