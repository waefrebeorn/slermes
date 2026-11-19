/* t_port_chat_completion_helpers_flow.c -- harness for the cch2 second-wave
 * FLOW + leaf port of agent/chat_completion_helpers.py (appended to
 * src/agent/chat_completion_helpers_flow.c).
 *
 * Reads JSON LINES from stdin (or argv[1]): {"func": <live_name>, "in": {...}}
 * where in.io carries the universal-seam scripts (seam-name -> constant or
 * per-call queue). Emits JSON LINES:
 *   {"func", "in", "ret", "trace", "st"}
 * ret is the projected return (or {"__raise__":kind,"message":str(exc)}),
 * trace the ordered seam events {ev,args,ret}, st the final state projection.
 *
 * The whole port .c is #included (area-qualified). The first-wave cch_flow
 * code references five canonical helpers from port_chat_completion_helpers.o;
 * this gate links libjson alone, so those externs are stubbed below (they are
 * never reached by the cch2 dispatch).
 */
#include "agent/chat_completion_helpers_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- stubs for the first-wave cch_flow externs (never called by cch2) ---- */
int cch_status_code_from_value(int kind, long long ival, const char *sval)
{ (void)kind; (void)ival; (void)sval; return -1; }
int cch_status_code_from_payload(const json_t *payload)
{ (void)payload; return -1; }
json_t *cch_json_object_from_text(const char *text)
{ (void)text; return NULL; }
int cch_payload_has_error_shape(const json_t *payload)
{ (void)payload; return 0; }
json_t **cch_parse_provider_sse_events(const char *text, int *out_count)
{ (void)text; if (out_count) *out_count = 0; return NULL; }

/* ---- universal-seam recording dispatcher ---- */
typedef struct {
    json_t *io;        /* borrowed in.io */
    json_t *cursors;   /* owned per-name queue cursors */
    json_t *trace;     /* owned ordered events */
} cch2_script_t;

/* drop None-valued object keys recursively (array elements kept) */
static json_t *cch2_h_strip_nulls(const json_t *v)
{
    if (!v) return json_null();
    if (v->type != JSON_OBJECT) return json_copy(v);
    json_t *o = json_object();
    for (size_t i = 0; i < v->c.count; i++) {
        json_t *val = v->c.items[i];
        if (val && val->type == JSON_NULL) continue;
        json_set(o, v->c.keys[i], cch2_h_strip_nulls(val));
    }
    return o;
}

static json_t *cch2_h_dispatch(void *ctx, const char *name, const json_t *args)
{
    cch2_script_t *sc = (cch2_script_t *)ctx;
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
    /* unwrap {"__script__":true,"ret":...} list-result wrapper */
    json_t *actual = result;
    if (!failclosed && actual && actual->type == JSON_OBJECT) {
        json_t *tag = json_obj_get(actual, "__script__");
        if (tag && tag->type == JSON_BOOL && tag->bool_val)
            actual = json_obj_get(actual, "ret");
    }
    json_t *retnode;
    if (failclosed) {
        char *m = cch2_xfmt("no script for %s", name);
        retnode = cch2_mkraise("RuntimeError", m ? m : "no script");
        free(m);
    } else {
        retnode = actual ? json_copy(actual) : json_null();
    }
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", cch2_h_strip_nulls(args));
    json_set(ev, "ret", json_copy(retnode));
    json_append(sc->trace, ev);

    if (failclosed) return retnode;
    if (!actual || actual->type == JSON_NULL) { json_free(retnode); return NULL; }
    return retnode;
}

/* ---- entry dispatch ---- */
static json_t *cch2_h_entry(const char *func, cch2_io_t *io, const json_t *in, json_t *st)
{
    /* Cases carry the LIVE name (e.g. "_rejects_stream_options"); the C entry
     * suffix drops the leading underscore (cch2_rejects_stream_options).
     * Normalize before suffix dispatch or every underscored name falls
     * through to "unknown func" (fail-closed). */
    if (func[0] == '_') func++;
#define CCH2_DISPATCH(nm) if (strcmp(func, #nm) == 0) return cch2_##nm(io, in, st)
    CCH2_DISPATCH(is_provider_stream_empty_frame_error);
    CCH2_DISPATCH(rejects_stream_options);
    CCH2_DISPATCH(is_sse_connection_error);
    CCH2_DISPATCH(relay_stream_identity);
    CCH2_DISPATCH(relay_stream_metadata);
    CCH2_DISPATCH(stream_final_text);
    CCH2_DISPATCH(high_effort_silence_floor);
    CCH2_DISPATCH(consume_ephemeral_max_output);
    CCH2_DISPATCH(model_dump_safe);
    CCH2_DISPATCH(dump_if_model);
    CCH2_DISPATCH(codex_silent_hang_hint);
    CCH2_DISPATCH(stream_env_stale_base);
    CCH2_DISPATCH(local_stream_stale_timeout_default);
    CCH2_DISPATCH(cap_to_run_budget);
    CCH2_DISPATCH(cloud_stale_timeout);
    CCH2_DISPATCH(stream_stale_timeout_is_explicit);
    CCH2_DISPATCH(cloud_stale_timeout_for);
    CCH2_DISPATCH(fallback_chain_exhausted);
    CCH2_DISPATCH(log_fallback_activated);
    CCH2_DISPATCH(rebind_fallback_credential_pool);
    CCH2_DISPATCH(candidate_pool_exhausted);
    CCH2_DISPATCH(update_fallback_context_compressor);
    CCH2_DISPATCH(reresolve_fallback_reasoning_config);
    CCH2_DISPATCH(rescope_fallback_extra_body);
    CCH2_DISPATCH(fallback_api_mode_hint);
    CCH2_DISPATCH(is_anthropic_wire_url);
    CCH2_DISPATCH(managed_summary_call);
    CCH2_DISPATCH(summary_text);
    CCH2_DISPATCH(with_stream_emitters);
    CCH2_DISPATCH(finalize_bedrock_relay_events);
#undef CCH2_DISPATCH
    return cch2_mkraise("RuntimeError", "unknown func");
}

/* ---- line reader (\n only; fixtures may carry raw U+2028 inside strings) ---- */
static char *cch2_h_readline(FILE *f)
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

static void cch2_h_run(const char *func, json_t *in)
{
    cch2_script_t sc;
    sc.io = cch2_get(in, "io");
    sc.cursors = json_object();
    sc.trace = json_array();
    cch2_io_t io;
    io.call = cch2_h_dispatch;
    io.ctx = &sc;
    json_t *st = json_object();
    json_t *in_echo = json_copy(in);
    json_t *ret = cch2_h_entry(func, &io, in, st);

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
    FILE *f = stdin;
    if (argc > 1) { f = fopen(argv[1], "rb"); if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 1; } }
    char *line;
    while ((line = cch2_h_readline(f)) != NULL) {
        if (line[0] == '\0') { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec) continue;
        const char *func = cch2_str(cch2_get(rec, "func"));
        json_t *in = cch2_get(rec, "in");
        if (func && in) cch2_h_run(func, in);
        json_free(rec);
    }
    if (f != stdin) fclose(f);
    return 0;
}