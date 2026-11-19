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

/* ---- wave13: direct leaf dispatch for first-wave internal helpers ----
 * These static helpers were previously reached only via the cch_flow_* entries
 * dispatched by t_port_cch_flow.c. Adding direct dispatch here clears the
 * name-based DEAD-CREDIT finding and gives independent leaf coverage. Additive:
 * no existing cch2 case uses these func names. */
static void cch2_h_tc_feed(tc_acc_t *t, const json_t *in) {
    const json_t *deltas = jget(in, "deltas");
    size_t n = deltas ? json_len((json_t *)deltas) : 0;
    for (size_t i = 0; i < n; i++) tc_feed(t, json_get((json_t *)deltas, i));
}
static void cch2_h_loop_init(chunk_loop_t *s, const json_t *in) {
    memset(s, 0, sizeof(*s));
    s->log = json_array();
    s->content_parts = json_array();
    s->reasoning_parts = json_array();
    s->detail_display_parts = json_array();
    s->refusal_parts = json_array();
    s->reasoning_details = json_array();
    s->pending_text_parts = json_array();
    s->partial_tool_names = json_array();
    tc_init(&s->tc);
    s->deltas_were_sent = jtruth(jget(in, "deltas_were_sent"));
    s->hold_stream = jtruth(jget(in, "hold_stream"));
    s->stream_delta_callback = jtruth(jget(in, "stream_delta_callback"));
    s->on_first_delta_present = jhas(in, "on_first_delta") ? jtruth(jget(in, "on_first_delta")) : 1;
    s->display.ud = NULL;
    s->display.fire_stream_delta = display_noop;
    s->display.fire_reasoning_delta = display_noop;
    s->display.fire_tool_gen_started = display_noop;
    s->display.on_first_delta = display_noop0;
    const json_t *watch = jget(in, "watch");
    s->watch_content.arr = jget(watch, "content");
    s->watch_reasoning.arr = jget(watch, "reasoning");
    s->watch_detail.arr = jget(watch, "detail");
}
static void cch2_h_loop_free(chunk_loop_t *s) {
    json_free(s->content_parts); json_free(s->reasoning_parts); json_free(s->detail_display_parts);
    json_free(s->refusal_parts); json_free(s->reasoning_details); json_free(s->pending_text_parts);
    json_free(s->partial_tool_names);
    tc_free(&s->tc);
    free(s->finish_reason); free(s->model_name); free(s->response_id); free(s->upstream_provider);
    json_free(s->usage_obj);
}
static json_t *cch2_h_loop_proj(chunk_loop_t *s) {
    json_t *o = json_object();
    json_set(o, "log", json_copy(s->log));
    json_set(o, "content_parts", json_copy(s->content_parts));
    json_set(o, "reasoning_parts", json_copy(s->reasoning_parts));
    json_set(o, "pending_text_parts", json_copy(s->pending_text_parts));
    json_set(o, "partial_tool_names", json_copy(s->partial_tool_names));
    json_set(o, "first_delta_fired", json_bool(s->first_delta_fired));
    return o;
}
static json_t *cch2_l_normalize_finish_reason(const json_t *in) {
    char *r = flow_normalize_finish_reason(jget(in, "raw"));
    json_t *o = r ? json_string(r) : json_null(); free(r); return o;
}
static json_t *cch2_l_router_shim_may_follow(const json_t *in) {
    return json_bool(flow_router_shim_may_follow(jstrv(jget(in, "text"))));
}
static json_t *cch2_l_positive_completion_tokens(const json_t *in) {
    return json_bool(flow_positive_completion_tokens(jget(in, "usage")));
}
static json_t *cch2_l_is_router_timeout_shim(const json_t *in) {
    return json_bool(flow_is_router_timeout_shim(jget(in, "response")));
}
static json_t *cch2_l_detail_text(const json_t *in) {
    char *r = flow_detail_text(jget(in, "detail"));
    json_t *o = r ? json_string(r) : json_null(); free(r); return o;
}
static json_t *cch2_l_append_detail(const json_t *in) {
    json_t *acc = jget(in, "acc") ? json_copy(jget(in, "acc")) : json_array();
    flow_append_detail(acc, jget(in, "detail"));
    return acc;
}
static json_t *cch2_l_provider_error_body(const json_t *in) {
    return flow_provider_error_body(jget(in, "payload"), (int)json_get_num(in, "status_code", 0));
}
static json_t *cch2_l_text_may_be_sse(const json_t *in) {
    return json_bool(flow_text_may_be_sse(jstrv(jget(in, "text"))));
}
static json_t *cch2_l_consume_ephemeral(const json_t *in) {
    json_t *agent = jget(in, "agent") ? json_copy(jget(in, "agent")) : json_object();
    int r = flow_consume_ephemeral(agent);
    json_free(agent);
    return json_bool(r);
}
static json_t *cch2_l_prompt_progress(const json_t *in) {
    return json_bool(loop_prompt_progress(jget(in, "chunk")));
}
static json_t *cch2_l_text_from_part(const json_t *in) {
    char *r = flow_text_from_part(jget(in, "part"));
    json_t *o = r ? json_string(r) : json_null(); free(r); return o;
}
static json_t *cch2_l_flatten_message_text(const json_t *in) {
    char *r = flow_flatten_message_text(jget(in, "content"), jstrv(jget(in, "sep")));
    json_t *o = r ? json_string(r) : json_null(); free(r); return o;
}
static json_t *cch2_l_separate_glued(const json_t *in) {
    char *r = flow_separate_glued(jstrv(jget(in, "previous")), jget(in, "delta"));
    json_t *o = r ? json_string(r) : json_null(); free(r); return o;
}
static json_t *cch2_l_pse_format_message(const json_t *in) {
    char *r = flow_pse_format_message((int)json_get_num(in, "status_code", 0), jget(in, "body"));
    json_t *o = r ? json_string(r) : json_null(); free(r); return o;
}
static json_t *cch2_l_tc_arguments_closed(const json_t *in) {
    tc_acc_t t; tc_init(&t);
    cch2_h_tc_feed(&t, in);
    int r = tc_arguments_closed(&t, (int)json_get_num(in, "slot", 0));
    tc_free(&t);
    return json_bool(r);
}
static json_t *cch2_l_tc_starts_new_call(const json_t *in) {
    tc_acc_t t; tc_init(&t);
    cch2_h_tc_feed(&t, in);
    int r = tc_starts_new_call(&t, jget(in, "delta"), (int)json_get_num(in, "slot", 0));
    tc_free(&t);
    return json_bool(r);
}
static json_t *cch2_l_tc_feed(const json_t *in) {
    tc_acc_t t; tc_init(&t);
    cch2_h_tc_feed(&t, in);
    const char *name = tc_feed(&t, jget(in, "delta"));
    json_t *r = name ? json_string(name) : json_null();
    tc_free(&t);
    return r;
}
static json_t *cch2_l_tc_materialize(const json_t *in) {
    tc_acc_t t; tc_init(&t);
    cch2_h_tc_feed(&t, in);
    tc_materialize(&t);
    json_t *r = json_copy(t.acc);
    tc_free(&t);
    return r;
}
#define CCH2_LOOP_LEAF(nm, stmt) \
    static json_t *cch2_l_##nm(const json_t *in) { \
        chunk_loop_t s; cch2_h_loop_init(&s, in); \
        stmt; \
        json_t *o = cch2_h_loop_proj(&s); \
        cch2_h_loop_free(&s); json_free(s.log); \
        return o; \
    }
CCH2_LOOP_LEAF(loop_fire_first, loop_fire_first(&s))
CCH2_LOOP_LEAF(loop_emit_text, loop_emit_text(&s, jstrv(jget(in, "text"))))
CCH2_LOOP_LEAF(loop_emit_reasoning, loop_emit_reasoning(&s, jstrv(jget(in, "text"))))
CCH2_LOOP_LEAF(loop_emit_tool_started, loop_emit_tool_started(&s, jstrv(jget(in, "name"))))
CCH2_LOOP_LEAF(loop_route_suppressed, loop_route_suppressed(&s, jstrv(jget(in, "text"))))
CCH2_LOOP_LEAF(loop_flush_pending, loop_flush_pending(&s))
#undef CCH2_LOOP_LEAF

/* ---- entry dispatch ---- */
static json_t *cch2_h_entry(const char *func, cch2_io_t *io, const json_t *in, json_t *st)
{
    /* wave13 first-wave leaf dispatch (before underscore normalization). */
    if (strcmp(func, "normalize_finish_reason") == 0) return cch2_l_normalize_finish_reason(in);
    if (strcmp(func, "router_shim_may_follow") == 0) return cch2_l_router_shim_may_follow(in);
    if (strcmp(func, "positive_completion_tokens") == 0) return cch2_l_positive_completion_tokens(in);
    if (strcmp(func, "is_router_timeout_shim") == 0) return cch2_l_is_router_timeout_shim(in);
    if (strcmp(func, "detail_text") == 0) return cch2_l_detail_text(in);
    if (strcmp(func, "append_detail") == 0) return cch2_l_append_detail(in);
    if (strcmp(func, "provider_error_body") == 0) return cch2_l_provider_error_body(in);
    if (strcmp(func, "text_may_be_sse") == 0) return cch2_l_text_may_be_sse(in);
    if (strcmp(func, "consume_ephemeral") == 0) return cch2_l_consume_ephemeral(in);
    if (strcmp(func, "prompt_progress") == 0) return cch2_l_prompt_progress(in);
    if (strcmp(func, "text_from_part") == 0) return cch2_l_text_from_part(in);
    if (strcmp(func, "flatten_message_text") == 0) return cch2_l_flatten_message_text(in);
    if (strcmp(func, "separate_glued") == 0) return cch2_l_separate_glued(in);
    if (strcmp(func, "pse_format_message") == 0) return cch2_l_pse_format_message(in);
    if (strcmp(func, "tc_arguments_closed") == 0) return cch2_l_tc_arguments_closed(in);
    if (strcmp(func, "tc_starts_new_call") == 0) return cch2_l_tc_starts_new_call(in);
    if (strcmp(func, "tc_feed") == 0) return cch2_l_tc_feed(in);
    if (strcmp(func, "tc_materialize") == 0) return cch2_l_tc_materialize(in);
    if (strcmp(func, "loop_fire_first") == 0) return cch2_l_loop_fire_first(in);
    if (strcmp(func, "loop_emit_text") == 0) return cch2_l_loop_emit_text(in);
    if (strcmp(func, "loop_emit_reasoning") == 0) return cch2_l_loop_emit_reasoning(in);
    if (strcmp(func, "loop_emit_tool_started") == 0) return cch2_l_loop_emit_tool_started(in);
    if (strcmp(func, "loop_route_suppressed") == 0) return cch2_l_loop_route_suppressed(in);
    if (strcmp(func, "loop_flush_pending") == 0) return cch2_l_loop_flush_pending(in);
    (void)io; (void)st;
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