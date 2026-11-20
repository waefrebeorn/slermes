/*
 * t_port_config_env_flow2.c — oracle harness for the hermes_cli/web_routers/config_env.py
 * UNCLAIMED remainder ported in src/cli/config_env_flow2.c.
 *
 * Self-contained: includes the port .c by area-qualified path. Reads JSON-line cases
 * from stdin (tests/fixtures/config_env_flow2.jsonl), wires the universal seam to
 * the SCRIPTED results in each case's "seams" object (a recording stub appending one
 * trace event per seam call, in call order), and emits
 * {"func":..., "in":..., "out":{"ret":..., "trace":[...], "state":...}}.
 *
 * "state" is the post-hoc mutated carrier (cfg / entry) for the five mutation flows
 * (pop_legacy_custom_provider, detach_main_model_from_provider, drop_stale_addresses,
 * store_endpoint_key, apply_endpoint_key) — captured even on raise (partial mutation
 * is observable); JSON null for every other function.
 *
 * FAIL-CLOSED script semantics (identical to the oracle): a MISSING script returns a
 * RuntimeError raise spec {"__raise__":"RuntimeError","msg":"no script for seam: <name>"};
 * a scripted JSON null returns a NULL pointer (None), never a json_null node; an ARRAY
 * script is a per-call queue (last value repeats, empty array -> None);
 * {"__raise__": cls, "msg": ...} scripts an exception. The observable effects are the
 * ordered seam trace + the return value + the mutated state.
 * tests/oracle_config_env_flow2.py drives the LIVE Python with the same scripts and
 * compares ordered trace + ret + state.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "cli/config_env_flow2.c"

typedef struct {
    const json_t *in;
    json_t *trace;
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
    if (!sc) {
        /* FAIL-CLOSED: a missing script is a RuntimeError raise spec (NOT None). */
        char *m = ce3_xsf("no script for seam: %s", name);
        result = ce3_raise("RuntimeError", m);
        free(m);
    } else if (sc->type == JSON_ARRAY) {
        double idx = json_get_num(ctx->counters, name, 0);
        size_t i = (size_t)idx;
        size_t n = json_len(sc);
        if (n > 0) result = json_copy(json_get(sc, i < n ? i : n - 1));
        json_set(ctx->counters, name, json_number(idx + 1));
    } else {
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
    if (!ctx->trace) ctx->trace = json_array();
    json_append(ctx->trace, e);
    return result;
}

#define S(k) json_get_str(in, k, NULL)
#define J(k) json_obj_get(in, k)
#define B(k) json_get_bool(in, k, false)

static void ret_json(json_t *out, json_t *r) {
    json_set(out, "ret", r ? r : json_null());
}
/* mutation-flow state: the private carrier copy, moved into the envelope */
static void state_json(json_t *out, json_t *carrier) {
    json_set(out, "state", carrier ? carrier : json_null());
}

static void run_case(const char *func, const json_t *in, json_t *out) {
    script_ctx_t ctx = {in, NULL, json_object()};
    ce3_seams_t seams = {&ctx, script_call};
    const ce3_seams_t *sp = &seams;
    json_t *r = NULL;

    /* ── pure builders ── */
    if (!strcmp(func, "provider_card")) {
        r = ce3_provider_card(J("d"), S("description"), J("url"),
                              B("is_password"), B("advanced"));
    } else if (!strcmp(func, "endpoint_api_mode")) {
        r = ce3_endpoint_api_mode(sp, J("entry"));
    } else if (!strcmp(func, "endpoint_row")) {
        r = ce3_endpoint_row(sp, S("endpoint_id"), S("name"), S("base_url"), S("model"),
                             J("models"), J("context_length"), B("discover_models"),
                             J("key_entry"), B("is_current"), S("source"));
    } else if (!strcmp(func, "model_names_provider")) {
        r = ce3_model_names_provider(sp, J("model_cfg"), S("provider_key"), J("entry"));
    }
    /* ── resolution / read flows ── */
    else if (!strcmp(func, "resolve_custom_endpoint_entry")) {
        r = ce3_resolve_custom_endpoint_entry(sp, J("providers"), S("endpoint_id"));
    } else if (!strcmp(func, "raw_provider_api_key")) {
        r = ce3_raw_provider_api_key(sp, S("endpoint_id"));
    }
    /* ── mutation flows (private carrier copy -> state) ── */
    else if (!strcmp(func, "pop_legacy_custom_provider")) {
        json_t *cfg = json_copy(J("cfg"));
        r = ce3_pop_legacy_custom_provider(sp, cfg, S("provider_key"));
        state_json(out, cfg);
    } else if (!strcmp(func, "detach_main_model_from_provider")) {
        json_t *cfg = json_copy(J("cfg"));
        r = ce3_detach_main_model_from_provider(sp, cfg, S("provider_key"), J("entry"));
        state_json(out, cfg);
    } else if (!strcmp(func, "drop_stale_addresses")) {
        json_t *entry = json_copy(J("entry"));
        r = ce3_drop_stale_addresses(entry);
        state_json(out, entry);
    } else if (!strcmp(func, "store_endpoint_key")) {
        json_t *entry = json_copy(J("entry"));
        r = ce3_store_endpoint_key(sp, entry, J("env_var"), S("key"), S("base_url"),
                                   J("own_provider"));
        state_json(out, entry);
    } else if (!strcmp(func, "apply_endpoint_key")) {
        json_t *entry = json_copy(J("entry"));
        r = ce3_apply_endpoint_key(sp, entry, J("body"), S("endpoint_id"), S("base_url"),
                                   J("stored_key"));
        state_json(out, entry);
    }
    /* ── credential / route-state flows ── */
    else if (!strcmp(func, "save_env_credential")) {
        r = ce3_save_env_credential(sp, S("key"), S("value"), B("provider_setup"));
    } else if (!strcmp(func, "drop_carried_route_state")) {
        r = ce3_drop_carried_route_state(sp, J("existing"), J("merged"));
    }
    /* ── probe helpers ── */
    else if (!strcmp(func, "no_models_probe_message")) {
        r = ce3_no_models_probe_message(sp, J("resp"), S("base_url"));
    } else if (!strcmp(func, "auto_api_mode")) {
        r = ce3_auto_api_mode(sp, S("base_url"));
    } else if (!strcmp(func, "env_write_errors")) {
        r = ce3_env_write_errors(sp, S("log_msg"), J("body_raise"));
    } else if (!strcmp(func, "record_custom_endpoint_setup")) {
        r = ce3_record_custom_endpoint_setup(sp, J("home"));
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
    }

    ret_json(out, r);
    json_set(out, "trace", ctx.trace ? ctx.trace : json_array());
    json_free(ctx.counters);
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
        /* Reserve envelope key positions: the oracle's want is built {ret, trace, state}
         * and canon() treats insertion order as semantic. */
        json_set(out, "ret", json_null());
        json_set(out, "trace", json_array());
        json_set(out, "state", json_null());
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