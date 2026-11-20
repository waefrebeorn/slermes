/* t_port_error_classifier_flow.c -- harness for the FLOW + leaf port of the
 * UNCLAIMED remainder of agent/error_classifier.py in
 * src/agent/error_classifier_flow.c (ec_* entries).
 *
 * Reads JSON LINES from stdin (or argv[1]): {"func": <live_name>, "in": {...}}
 * where in.io carries the universal-seam scripts (seam-name -> constant or
 * per-call queue). Emits JSON LINES:
 *   {"func", "in", "ret", "trace", "st"}
 * ret is the projected return (or {"__raise__":kind,"message":str(exc)}),
 * trace the ordered seam events {ev,args,ret}, st the final state projection
 * ({} on every path -- these classifiers mutate no caller state).
 *
 * The whole port .c is #included (area-qualified); the gate links libjson
 * alone. Script semantics (shared with the oracle Script):
 *   absent key / exhausted queue -> fail-closed RuntimeError "no script for X"
 *   non-list value               -> constant result reused every call
 *   list value                   -> per-call queue (one result per call)
 *   {"__raise__":K,"message":M}  -> raised exception of kind K
 *   {"__script__":true,"ret":X}  -> list-valued result X (queue-ambiguity wrap)
 *   null                         -> None result (delivered as NULL pointer)
 */
#include "agent/error_classifier_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- universal-seam recording dispatcher ---- */
typedef struct {
    json_t *io;        /* borrowed in.io */
    json_t *cursors;   /* owned per-name queue cursors */
    json_t *trace;     /* owned ordered events */
} ec_script_t;

/* drop None-valued object keys recursively (array elements kept) */
static json_t *ec_h_strip_nulls(const json_t *v)
{
    if (!v) return json_null();
    if (v->type != JSON_OBJECT) return json_copy(v);
    json_t *o = json_object();
    for (size_t i = 0; i < v->c.count; i++) {
        json_t *val = v->c.items[i];
        if (val && val->type == JSON_NULL) continue;
        json_set(o, v->c.keys[i], ec_h_strip_nulls(val));
    }
    return o;
}

static json_t *ec_h_dispatch(void *ctx, const char *name, const json_t *args)
{
    ec_script_t *sc = (ec_script_t *)ctx;
    json_t *entry = sc->io ? json_obj_get(sc->io, name) : NULL;
    int present = entry != NULL;
    int exhausted = 0;
    json_t *result = NULL;
    if (present) {
        if (entry->type == JSON_ARRAY) {
            double cur = json_get_num(sc->cursors, name, 0);
            size_t idx = (size_t)cur;
            if (idx >= json_len(entry)) exhausted = 1;
            else {
                result = json_get(entry, idx);
                json_set(sc->cursors, name, json_number(cur + 1));
            }
        } else {
            result = entry;
        }
    }
    int failclosed = !present || exhausted;
    json_t *actual = result;
    if (!failclosed && actual && actual->type == JSON_OBJECT) {
        json_t *tag = json_obj_get(actual, "__script__");
        if (tag && tag->type == JSON_BOOL && tag->bool_val)
            actual = json_obj_get(actual, "ret");
    }
    json_t *retnode;
    if (failclosed) {
        char *m = ec_xfmt("no script for %s", name);
        retnode = ec_mkraise("RuntimeError", m ? m : "no script");
        free(m);
    } else {
        retnode = actual ? json_copy(actual) : json_null();
    }
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", ec_h_strip_nulls(args));
    json_set(ev, "ret", json_copy(retnode));
    json_append(sc->trace, ev);

    if (failclosed) return retnode;
    if (!actual || actual->type == JSON_NULL) { json_free(retnode); return NULL; }
    return retnode;
}

/* ---- entry dispatch (full live name) ---- */
static json_t *ec_h_entry(const char *func, ec_io_t *io, const json_t *in, json_t *st)
{
#define EC_DISPATCH(nm, fn) if (strcmp(func, nm) == 0) return fn(io, in, st)
    EC_DISPATCH("_v", ec_v_entry);
    EC_DISPATCH("_first_match", ec_first_match_entry);
    EC_DISPATCH("_billing_hints", ec_billing_hints_entry);
    EC_DISPATCH("_is_anthropic_thinking_rejection", ec_is_anthropic_thinking_rejection);
    EC_DISPATCH("_gemini_prompt_block_marker", ec_gemini_prompt_block_marker);
    EC_DISPATCH("_is_gemini_prompt_block", ec_is_gemini_prompt_block_entry);
    EC_DISPATCH("_from_cause_chain", ec_from_cause_chain_entry);
    EC_DISPATCH("_status_of", ec_status_of_entry);
    EC_DISPATCH("_body_of", ec_body_of_entry);
    EC_DISPATCH("_headers_of", ec_headers_of_entry);
    EC_DISPATCH("_status_code_from_body", ec_status_code_from_body_entry);
    EC_DISPATCH("_code_from_payload", ec_code_from_payload_entry);
    EC_DISPATCH("_body_message_candidates", ec_body_message_candidates_entry);
    EC_DISPATCH("_openrouter_wrapped_message", ec_openrouter_wrapped_message_entry);
    EC_DISPATCH("_build_error_msg", ec_build_error_msg_entry);
    EC_DISPATCH("_is_server_injected_param_rejection", ec_is_server_injected_param_rejection);
    EC_DISPATCH("_is_codex_masked_replay_rejection", ec_is_codex_masked_replay_rejection_entry);
    EC_DISPATCH("_moa_special_cases", ec_moa_special_cases_entry);
    EC_DISPATCH("_nous_welcome_tier", ec_nous_welcome_tier_entry);
    EC_DISPATCH("_provider_special_cases", ec_provider_special_cases_entry);
    EC_DISPATCH("_status_403", ec_status_403_entry);
    EC_DISPATCH("_status_404", ec_status_404_entry);
    EC_DISPATCH("_status_429", ec_status_429_entry);
    EC_DISPATCH("_status_5xx", ec_status_5xx_entry);
    EC_DISPATCH("_is_nous_route", ec_is_nous_route_entry);
    EC_DISPATCH("_by_status", ec_by_status_entry);
    EC_DISPATCH("_by_error_code", ec_by_error_code_entry);
    EC_DISPATCH("_by_transport", ec_by_transport_entry);
    EC_DISPATCH("_shape_context", ec_shape_context_entry);
    EC_DISPATCH("_off_route_host", ec_off_route_host_entry);
    EC_DISPATCH("_plugin_verdict", ec_plugin_verdict_entry);
    EC_DISPATCH("_profile_verdict", ec_profile_verdict_entry);
    EC_DISPATCH("__post_init__", ec_ctx_post_init_entry);
    EC_DISPATCH("large_session", ec_large_session_entry);
#undef EC_DISPATCH
    return ec_mkraise("RuntimeError", "unknown func");
}

/* ---- line reader (\n only) ---- */
static char *ec_h_readline(FILE *f)
{
    size_t cap = 4096, n = 0;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;
    int c;
    while ((c = fgetc(f)) != EOF) {
        if (c == '\n') break;
        if (n + 1 >= cap) { cap *= 2; char *nb = (char *)realloc(buf, cap); if (!nb) { free(buf); return NULL; } buf = nb; }
        buf[n++] = (char)c;
    }
    if (c == EOF && n == 0) { free(buf); return NULL; }
    buf[n] = '\0';
    return buf;
}

static void ec_h_run(const char *func, json_t *in)
{
    ec_script_t sc;
    sc.io = ec_get(in, "io");
    sc.cursors = json_object();
    sc.trace = json_array();
    ec_io_t io;
    io.call = ec_h_dispatch;
    io.ctx = &sc;
    json_t *st = json_object();
    json_t *in_echo = json_copy(in);
    json_t *ret = ec_h_entry(func, &io, in, st);
    if (ec_is_raise(ret)) {
        json_free(st);
        st = json_object();
    }
    json_t *out = json_object();
    json_set(out, "func", json_string(func));
    json_set(out, "in", in_echo);
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", sc.trace);
    json_set(out, "st", st);
    char *s = json_serialize(out);
    printf("%s\n", s ? s : "{}");
    free(s);
    json_free(out);
    json_free(sc.cursors);
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    ec_tables_init();
    FILE *f = stdin;
    if (argc > 1) { f = fopen(argv[1], "rb"); if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 1; } }
    char *line;
    while ((line = ec_h_readline(f)) != NULL) {
        if (line[0] == '\0') { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec) continue;
        const char *func = ec_str(ec_get(rec, "func"));
        json_t *in = ec_get(rec, "in");
        if (func && in) ec_h_run(func, in);
        json_free(rec);
    }
    if (f != stdin) fclose(f);
    return 0;
}