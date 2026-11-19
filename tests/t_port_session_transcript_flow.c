/*
 * t_port_session_transcript_flow.c — oracle harness for the UNCLAIMED
 * gateway/session_transcript.py FLOW cores + leaves ported in
 * src/gateway/session_transcript_flow.c.
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * "in" carries the injected state mirror ("state"), the scripted seam results
 * ("io") and the call arguments. The universal seam stub records one
 * {ev, args, ret} trace event per call in call order; the harness emits
 * {"func", "in", "out": {"ret", "trace", "state"}} per case.
 * tests/oracle_session_transcript_flow.py drives the LIVE Python with the
 * same scripts.
 *
 * Script semantics (shared with the oracle): a bare JSON array is ONE RESULT
 * PER CALL (last value repeats); an empty array is a constant None;
 * {"__script__": true, "ret": v} is a constant; anything else is a constant;
 * absent == None. A list-valued seam result must be scripted one level deeper.
 * {"__raise__": kind, "message": m} scripts a raised exception.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "gateway/session_transcript_flow.c"

typedef struct {
    const json_t *in;
    const json_t *io;    /* borrowed in["io"] */
    json_t *state;       /* owned mirror */
    json_t *trace;       /* owned array */
    json_t *pos;         /* owned per-name queue positions */
} stx_script_ctx;

/* Script lookup: bare array = per-call queue (last repeats); empty array =
 * constant None; {"__script__":true,"ret":v} = constant; else constant. */
static const json_t *stx_next_script(stx_script_ctx *c, const char *name) {
    const json_t *v = c->io ? json_obj_get(c->io, name) : NULL;
    if (!v) return NULL;
    if (v->type == JSON_ARRAY) {
        if (v->c.count == 0) return NULL;
        double idx = json_get_num(c->pos, name, 0);
        size_t i = (size_t)idx;
        if (i >= v->c.count) i = v->c.count - 1;
        else json_set(c->pos, name, json_number((double)(i + 1)));
        return json_get(v, i);
    }
    if (v->type == JSON_OBJECT && json_get_bool(v, "__script__", false))
        return json_obj_get(v, "ret");
    return v;
}

static json_t *stx_stub_call(void *ud, const char *name, const json_t *args) {
    stx_script_ctx *c = ud;
    if (strcmp(name, "log") == 0) {
        json_t *ev = json_object();
        json_set(ev, "ev", json_string("log"));
        json_set(ev, "level", json_copy(json_obj_get(args, "level")));
        json_set(ev, "fmt", json_copy(json_obj_get(args, "fmt")));
        json_set(ev, "args", json_copy(json_obj_get(args, "args")));
        if (json_has(args, "exc_info"))
            json_set(ev, "exc_info", json_copy(json_obj_get(args, "exc_info")));
        json_append(c->trace, ev);
        return NULL;
    }
    const json_t *script = stx_next_script(c, name);
    json_t *ev = json_object();
    json_set(ev, "ev", json_string(name));
    json_set(ev, "args", args ? json_copy(args) : json_null());
    json_set(ev, "ret", script ? json_copy(script) : json_null());
    json_append(c->trace, ev);
    if (!script || script->type == JSON_NULL) return NULL;   /* None == NULL */
    return json_copy(script);
}

/* ── set-section dump ordering (Python sorted() on strs) ─────────────────── */
static int stx_cmp_str_item(const void *pa, const void *pb) {
    const json_t *a = *(const json_t *const *)pa;
    const json_t *b = *(const json_t *const *)pb;
    const char *sa = (a && a->type == JSON_STRING && a->str_val) ? a->str_val : "";
    const char *sb = (b && b->type == JSON_STRING && b->str_val) ? b->str_val : "";
    return strcmp(sa, sb);
}
static json_t *stx_sorted_copy(const json_t *arr) {
    json_t *copy = arr ? json_copy(arr) : json_array();
    if (copy && copy->type == JSON_ARRAY && copy->c.count > 1)
        qsort(copy->c.items, copy->c.count, sizeof(json_t *), stx_cmp_str_item);
    return copy;
}

typedef json_t *(*stx_entry_fn)(stx_env *, const json_t *);
typedef struct { const char *name; stx_entry_fn fn; } stx_dispatch;

static const stx_dispatch stx_table[] = {
    { "TranscriptReadError", stx_transcript_read_error_init },
    { "_spool_dropped", stx_spool_dropped },
    { "_bindable_text", stx_bindable_text },
    { "transcript_append_kwargs", stx_transcript_append_kwargs },
    { "_follow_reroutes", stx_follow_reroutes },
    { "_enqueue_transcript_message", stx_enqueue_transcript_message },
    { "_divert_transcript_after_db_replaced", stx_divert_transcript_after_db_replaced },
    { "_live_compression_child", stx_live_compression_child },
    { "_migrate_transcript_queue_to_child", stx_migrate_transcript_queue_to_child },
    { "_publish_transcript_reroute", stx_publish_transcript_reroute },
    { "_spool_stalled_backlog", stx_spool_stalled_backlog },
    { "mark_spooled_drop_sessions", stx_mark_spooled_drop_sessions },
    { "has_platform_message_id", stx_has_platform_message_id },
    { "transcript_tail_role", stx_transcript_tail_role },
    { "is_published_compression_child", stx_is_published_compression_child },
    { "persist_rotated_compression_child", stx_persist_rotated_compression_child },
    { "has_input_owner", stx_has_input_owner },
    { "load_transcript", stx_load_transcript },
    { "retain_refused_turn_for_display", stx_retain_refused_turn_for_display },
};

static void stx_ensure_obj(json_t *state, const char *key) {
    const json_t *v = json_obj_get(state, key);
    if (!v || v->type != JSON_OBJECT) json_set(state, key, json_object());
}
static void stx_ensure_nullable(json_t *state, const char *key) {
    if (!json_obj_get(state, key)) json_set(state, key, json_null());
}

static void stx_run_case(const char *func, const json_t *in, json_t *out) {
    json_t *state = json_copy(json_obj_get(in, "state"));
    if (!state || state->type != JSON_OBJECT) {
        json_free(state);
        state = json_object();
    }
    stx_ensure_obj(state, "dirty_transcripts");
    stx_ensure_obj(state, "append_failures");
    stx_ensure_obj(state, "entries");
    stx_ensure_obj(state, "stores");
    stx_ensure_nullable(state, "reroutes");
    stx_ensure_nullable(state, "spooled_drop_sessions");
    stx_ensure_nullable(state, "session_owner_hints");
    /* _migrate: the caller's pending list IS dirty_transcripts[queue] */
    if (strcmp(func, "_migrate_transcript_queue_to_child") == 0) {
        const char *qsid = json_get_str(in, "queue_session_id", "");
        const json_t *pending = json_obj_get(in, "pending");
        json_set(json_obj_get(state, "dirty_transcripts"), qsid,
                 pending ? json_copy(pending) : json_array());
    }
    stx_script_ctx ctx = {
        .in = in, .io = json_obj_get(in, "io"), .state = state,
        .trace = json_array(), .pos = json_object(),
    };
    stx_env env = { .state = state, .call = stx_stub_call, .ud = &ctx,
                    .exc = NULL, .cur = NULL };
    json_t *ret = NULL;
    bool found = false;
    for (size_t i = 0; i < sizeof(stx_table) / sizeof(stx_table[0]); i++) {
        if (strcmp(func, stx_table[i].name) == 0) {
            ret = stx_table[i].fn(&env, in);
            found = true;
            break;
        }
    }
    if (!found) json_set(out, "error", json_string("unknown func"));
    if (env.exc) {   /* propagated BaseException / unhandled raise */
        json_free(ret);
        ret = env.exc;
    }
    if (env.cur) json_free(env.cur);
    json_set(out, "ret", ret ? ret : json_null());
    json_set(out, "trace", ctx.trace);   /* adopts */
    /* Out state: mutable sections only (stores is an injected input).
     * null == attribute still ABSENT; the spooled set dumps sorted. */
    json_t *st = json_object();
    json_set(st, "dirty_transcripts",
             json_copy(json_obj_get(state, "dirty_transcripts")));
    json_set(st, "append_failures",
             json_copy(json_obj_get(state, "append_failures")));
    const json_t *rr = json_obj_get(state, "reroutes");
    json_set(st, "reroutes",
             (rr && rr->type != JSON_NULL) ? json_copy(rr) : json_null());
    const json_t *sp = json_obj_get(state, "spooled_drop_sessions");
    json_set(st, "spooled_drop_sessions",
             (sp && sp->type != JSON_NULL) ? stx_sorted_copy(sp) : json_null());
    const json_t *hints = json_obj_get(state, "session_owner_hints");
    json_set(st, "session_owner_hints",
             (hints && hints->type != JSON_NULL) ? json_copy(hints) : json_null());
    json_set(st, "entries", json_copy(json_obj_get(state, "entries")));
    json_set(out, "state", st);   /* adopts */
    json_free(state);
    json_free(ctx.pos);
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
        stx_run_case(func, in ? in : empty, out);
        json_free(empty);
        char *in_s = json_serialize(in);
        char *out_s = json_serialize(out);
        /* Serialize a string node for func (bounds-safe); never call
         * json_escape_string (off-by-one heap overflow in libjson). */
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