/*
 * t_port_gateway_cli_flow.c — oracle harness for the hermes_cli/gateway.py
 * FLOW cores + leaves ported in src/cli/gateway_cli_flow.c.
 *
 * Self-contained: includes the port .c by area-qualified path. Reads JSON-line
 * cases from stdin (tests/fixtures/gateway_cli_flow.jsonl), wires the universal
 * seam to the SCRIPTED results in each case's "seams" object (a recording stub
 * appending one trace event per seam call, in call order), and emits
 * {"func":..., "in":..., "out":{"ret":..., "trace":[...]}}.
 * A scripted JSON null returns a NULL pointer (None), never a json_null node;
 * {"__raise__": cls, "msg": ...} scripts an exception. The flows are stateless;
 * the observable effects are the ordered seam trace + the return value.
 * tests/oracle_gateway_cli_flow.py drives the LIVE Python with the same scripts
 * and compares ordered trace + ret.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "cli/gateway_cli_flow.c"

typedef struct {
    const json_t *in;
    gc_trace_t *trace;
    json_t *counters;
} script_ctx_t;

static json_t *args_drop_null(const json_t *args) {
    json_t *out = json_object();
    if (args && args->type == JSON_OBJECT) {
        for (size_t i = 0; i < args->c.count; i++) {
            const json_t *v = args->c.items[i];
            if (!v || v->type == JSON_NULL) continue;
            json_set(out, args->c.keys[i], json_copy(v));
        }
    }
    return out;
}

static json_t *script_call(void *vctx, const char *name, const json_t *args) {
    script_ctx_t *ctx = vctx;
    const json_t *seams = json_obj_get(ctx->in, "seams");
    const json_t *sc = json_obj_get(seams, name);
    json_t *result = NULL;
    if (sc && sc->type == JSON_ARRAY) {
        double idx = json_get_num(ctx->counters, name, 0);
        size_t i = (size_t)idx;
        size_t n = json_len(sc);
        if (n > 0) result = json_copy(json_get(sc, i < n ? i : n - 1));
        json_set(ctx->counters, name, json_number(idx + 1));
    } else if (sc) {
        result = json_copy(sc);
    }
    /* Contract: NULL == None. A scripted null must NOT surface as a non-NULL
     * json_null node (truthiness / is-not-None guards would read it wrong). */
    if (result && result->type == JSON_NULL) {
        json_free(result);
        result = NULL;
    }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", args_drop_null(args));   /* borrowed args: copy, do not free */
    json_set(e, "ret", result ? json_copy(result) : json_null());
    gc_trace_add(ctx->trace, e);
    return result;
}

#define S(k) json_get_str(in, k, NULL)
#define J(k) json_obj_get(in, k)
#define B(k, d) json_get_bool(in, k, d)

static void ret_json(json_t *out, json_t *r) {
    json_set(out, "ret", r ? r : json_null());
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    gc_trace_t trace = {NULL};
    json_t *counters = json_object();
    script_ctx_t ctx = {in, &trace, counters};
    gc_seams_t seams = {&ctx, script_call};
    const gc_seams_t *sp = &seams;

    if (!strcmp(func, "parse_kv_pairs")) {
        ret_json(out, gc_parse_kv_pairs(J("items")));
    } else if (!strcmp(func, "systemd_cli_bits")) {
        ret_json(out, gc_systemd_cli_bits(B("system", false)));
    } else if (!strcmp(func, "runtime_state_pid")) {
        ret_json(out, gc_runtime_state_pid(J("state"), sp));
    } else if (!strcmp(func, "host_decision_exit_code")) {
        ret_json(out, gc_host_decision_exit_code(J("decision")));
    } else if (!strcmp(func, "print_info_lines")) {
        ret_json(out, gc_print_info_lines(J("lines"), sp));
    } else if (!strcmp(func, "print_indented")) {
        ret_json(out, gc_print_indented(S("text"), sp));
    } else if (!strcmp(func, "current_profile_name")) {
        ret_json(out, gc_current_profile_name(sp));
    } else if (!strcmp(func, "systemd_unit_is_active")) {
        ret_json(out, gc_systemd_unit_is_active(B("system", false), sp));
    } else if (!strcmp(func, "systemctl_show")) {
        ret_json(out, gc_systemctl_show(J("properties"), B("system", false), sp));
    } else if (!strcmp(func, "systemd_unit_installed")) {
        ret_json(out, gc_systemd_unit_installed(sp));
    } else if (!strcmp(func, "agent_timeout_setting")) {
        ret_json(out, gc_agent_timeout_setting(S("env_var"), S("key"), sp));
    } else if (!strcmp(func, "systemd_scope_preamble")) {
        ret_json(out, gc_systemd_scope_preamble(S("action"), B("system", false),
                                                B("require_installed", true),
                                                B("preflight_user", false), sp));
    } else if (!strcmp(func, "retire_hermes_replace_dropin")) {
        ret_json(out, gc_retire_hermes_replace_dropin(B("system", false), sp));
    } else if (!strcmp(func, "host_multiplexer_serving")) {
        ret_json(out, gc_host_multiplexer_serving(S("profile_name"), sp));
    } else if (!strcmp(func, "served_by_another_host_gateway")) {
        ret_json(out, gc_served_by_another_host_gateway(S("profile_name"), sp));
    } else if (!strcmp(func, "host_multiplexer_for_all_verb")) {
        ret_json(out, gc_host_multiplexer_for_all_verb(sp));
    } else if (!strcmp(func, "host_multiplexer_is_ours")) {
        ret_json(out, gc_host_multiplexer_is_ours(J("owner"), sp));
    } else if (!strcmp(func, "print_unfolded_gateway_note")) {
        ret_json(out, gc_print_unfolded_gateway_note(J("owner"), sp));
    } else if (!strcmp(func, "stop_host_multiplexer")) {
        ret_json(out, gc_stop_host_multiplexer(J("owner"), sp));
    } else if (!strcmp(func, "print_multiplex_standalone_reason")) {
        ret_json(out, gc_print_multiplex_standalone_reason(sp));
    } else if (!strcmp(func, "print_served_ingress_urls")) {
        ret_json(out, gc_print_served_ingress_urls(S("profile"), sp));
    } else if (!strcmp(func, "served_profile_needs_no_service")) {
        ret_json(out, gc_served_profile_needs_no_service(sp));
    } else if (!strcmp(func, "named_profile_refused_under_multiplexer")) {
        ret_json(out, gc_named_profile_refused_under_multiplexer(B("force", false), sp));
    } else if (!strcmp(func, "attach_to_host_gateway_or_guard")) {
        ret_json(out, gc_attach_to_host_gateway_or_guard(B("force", false), B("replace", false), sp));
    } else if (!strcmp(func, "systemd_reset_and_run")) {
        ret_json(out, gc_systemd_reset_and_run(S("action"), B("system", false), J("previous_pid"), sp));
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
        ret_json(out, NULL);
    }

    json_set(out, "trace", trace.events ? trace.events : json_array());
    json_free(counters);
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    char *line = NULL;
    size_t cap = 0;
    ssize_t len;
    while ((len = getline(&line, &cap, stdin)) > 0) {
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';
        if (line[0] != '{') continue;
        char *err = NULL;
        json_t *c = json_parse(line, &err);
        if (err || !c || c->type != JSON_OBJECT) {
            fprintf(stderr, "bad case: %s\n", err ? err : "not object");
            free(err);
            json_free(c);
            continue;
        }
        const char *func = json_get_str(c, "func", "");
        const json_t *in = json_obj_get(c, "in");
        json_t *empty = json_object();
        json_t *out = json_object();
        /* Reserve envelope key positions: the oracle's want is built {ret, trace}
         * and canon() treats insertion order as semantic. */
        json_set(out, "ret", json_null());
        json_set(out, "trace", json_array());
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
    free(line);
    return 0;
}
