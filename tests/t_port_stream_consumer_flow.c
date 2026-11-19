/*
 * t_port_stream_consumer_flow.c — oracle harness for the
 * gateway/stream_consumer.py streaming-delivery FLOW cores ported in
 * src/cli/port_stream_consumer_flow.c.
 *
 * Reads JSON-lines cases (argv[1], else stdin): {"func": ..., "in": {...}}.
 * Wires every seam to a SCRIPTED recording stub (one trace event per call,
 * in call order) and emits {"func": ..., "in": ..., "out": {"ret": ...,
 * "trace": [...], "state": {...}, ...}} per case. The oracle drives the LIVE
 * Python class under the same scripts and diffs ret + ordered trace + state.
 *
 * Script shapes (in["seams"]):
 *   sequence seams: a JSON array consumed one entry per call; past the end
 *     the LAST entry repeats; an empty/absent list uses the seam default.
 *   map seams: {"map": {input_text: result}, "default": result}; an unmatched
 *     input uses "default" (absent default = the seam default).
 *   send_or_edit entries: bool | {"ret": bool, "raise": bool,
 *     "fallback": bool, "message_id": str|null} — "fallback"/"message_id"
 *     model the transport's state effects (flood mode / first-send id).
 *   hook: {"ret": "ok"|"raise"|"awaitable"}; flush_set_raise: bool.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "libjson/json.h"
#include "port_stream_consumer_flow.h"

/* ── scripted seam context ──────────────────────────────────────────────── */

enum {
    SEQ_CLOCK = 0, SEQ_SEND_OR_EDIT, SEQ_SEND_NEW_CHUNK, SEQ_TURN_RESET,
    SEQ_SHOULD_PUSH_DRAFT, SEQ_STREAM_IS_MESSAGE, SEQ_CEILING_ADOPT_FINAL,
    SEQ_CEILING_SPLIT_FIRST_SEND, SEQ_CEILING_FINAL_CUT, SEQ_CEILING_SEAL_CUT,
    SEQ_CEILING_FINISH, SEQ_CEILING_CANCELLED, SEQ_REDACT_SETTLED,
    SEQ_CUSTOM_UNIT_TO_CP, SEQ_TRUNCATE_FOR_STREAM, SEQ_SPLIT_TEXT_CHUNKS,
    SEQ_STALE_PREVIEW_IDS, SEQ_N
};
static const char *seq_keys[SEQ_N] = {
    "clock", "send_or_edit", "send_new_chunk", "turn_reset",
    "should_push_draft", "stream_is_message", "ceiling_adopt_final",
    "ceiling_split_first_send", "ceiling_final_cut", "ceiling_seal_cut",
    "ceiling_finish", "ceiling_cancelled", "redact_settled",
    "custom_unit_to_cp", "truncate_for_stream", "split_text_chunks",
    "stale_preview_ids"
};

typedef struct {
    const json_t *in;
    const json_t *seams;
    scf_state_t *st;
    json_t *trace;
    int idx[SEQ_N];
} script_ctx_t;

static void rec(script_ctx_t *c, json_t *e) { json_append(c->trace, e); }
static json_t *ev(const char *name) {
    json_t *e = json_object();
    json_set(e, "seam", json_string(name));
    return e;
}
static void evs(json_t *e, const char *k, const char *v) {
    json_set(e, k, v ? json_string(v) : json_null());
}
static void evb(json_t *e, const char *k, bool v) { json_set(e, k, json_bool(v)); }
static void evn(json_t *e, const char *k, double v) { json_set(e, k, json_number(v)); }

/* Next scripted entry for a sequence seam (borrowed; NULL = use default). */
static const json_t *seq_next(script_ctx_t *c, int sid) {
    const json_t *arr = json_obj_get(c->seams, seq_keys[sid]);
    if (!arr || arr->type != JSON_ARRAY || arr->c.count == 0) return NULL;
    int i = c->idx[sid];
    if (i >= (int)arr->c.count) i = (int)arr->c.count - 1;  /* repeat last */
    c->idx[sid] = i + 1;
    return arr->c.items[i];
}

/* Map-seam result for an input text (borrowed; NULL = use default). */
static const json_t *map_get(script_ctx_t *c, const char *key, const char *text) {
    const json_t *m = json_obj_get(c->seams, key);
    if (!m || m->type != JSON_OBJECT) return NULL;
    const json_t *map = json_obj_get(m, "map");
    if (map && map->type == JSON_OBJECT) {
        const json_t *hit = json_obj_get(map, text);
        if (hit) return hit;
    }
    return json_obj_get(m, "default");
}

static bool node_bool(const json_t *v, bool def) {
    if (!v) return def;
    if (v->type == JSON_BOOL) return v->bool_val;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    return def;
}

/* ── seam stubs ─────────────────────────────────────────────────────────── */

static double sm_clock(void *ctx) {
    script_ctx_t *c = ctx;
    const json_t *e = seq_next(c, SEQ_CLOCK);
    double v = e ? e->num_val : 0.0;
    json_t *o = ev("clock");
    evn(o, "ret", v);
    rec(c, o);
    return v;
}

static bool sm_send_or_edit(void *ctx, const char *text, bool finalize,
                            bool is_turn_final, bool redacted, bool *raised) {
    script_ctx_t *c = ctx;
    const json_t *e = seq_next(c, SEQ_SEND_OR_EDIT);
    bool ret = true, do_raise = false;
    if (e && e->type == JSON_BOOL) {
        ret = e->bool_val;
    } else if (e && e->type == JSON_OBJECT) {
        ret = json_get_bool(e, "ret", true);
        do_raise = json_get_bool(e, "raise", false);
        /* transport state effects (identical in the oracle stub) */
        if (json_has(e, "fallback"))
            c->st->fallback_final_send = json_get_bool(e, "fallback", false);
        if (json_has(e, "message_id")) {
            const json_t *mid = json_obj_get(e, "message_id");
            const char *s = (mid && mid->type == JSON_STRING) ? mid->str_val : NULL;
            char *n = s ? strdup(s) : NULL;
            free(c->st->message_id);
            c->st->message_id = n;
        }
    }
    json_t *o = ev("send_or_edit");
    evs(o, "text", text);
    evb(o, "finalize", finalize);
    evb(o, "is_turn_final", is_turn_final);
    evb(o, "redacted", redacted);
    if (do_raise) {
        evb(o, "raised", true);
        rec(c, o);
        *raised = true;
        return false;
    }
    evb(o, "ret", ret);
    rec(c, o);
    *raised = false;
    return ret;
}

static char *sm_send_new_chunk(void *ctx, const char *text, const char *reply_to,
                               bool final, bool first) {
    script_ctx_t *c = ctx;
    const json_t *e = seq_next(c, SEQ_SEND_NEW_CHUNK);
    const char *ret = "m1";
    if (e) {
        if (e->type == JSON_STRING) ret = e->str_val;
        else if (e->type == JSON_NULL) ret = NULL;
    }
    json_t *o = ev("send_new_chunk");
    evs(o, "text", text);
    evs(o, "reply_to", reply_to);
    evb(o, "final", final);
    evb(o, "first", first);
    evs(o, "ret", ret);
    rec(c, o);
    return ret ? strdup(ret) : NULL;
}

static void sm_send_fallback_final(void *ctx, const char *text) {
    script_ctx_t *c = ctx;
    json_t *o = ev("send_fallback_final");
    evs(o, "text", text);
    rec(c, o);
}

static void sm_send_commentary(void *ctx, const char *text) {
    script_ctx_t *c = ctx;
    json_t *o = ev("send_commentary");
    evs(o, "text", text);
    rec(c, o);
}

static void sm_flush_think_buffer(void *ctx) {
    script_ctx_t *c = ctx;
    rec(c, ev("flush_think_buffer"));
}

static void sm_filter_accumulate(void *ctx, const char *text) {
    script_ctx_t *c = ctx;
    json_t *o = ev("filter_accumulate");
    evs(o, "text", text);
    rec(c, o);
    /* scripted append effect (mirrors _append_accumulated; identical in the
     * oracle stub): clear overlay, ceiling-gated accumulated, full ledger */
    scf_state_t *st = c->st;
    if (st->tool_progress_lines.count > 0) {
        scf_strlist_clear(&st->tool_progress_lines);
        st->tool_progress_active = false;
    }
    if (!st->ceiling_hit) {
        size_t la = strlen(st->accumulated), lt = strlen(text);
        char *n = malloc(la + lt + 1);
        if (n) {
            memcpy(n, st->accumulated, la);
            memcpy(n + la, text, lt + 1);
            free(st->accumulated);
            st->accumulated = n;
        }
    }
    size_t ll = strlen(st->stream_ledger), lt2 = strlen(text);
    char *n2 = malloc(ll + lt2 + 1);
    if (n2) {
        memcpy(n2, st->stream_ledger, ll);
        memcpy(n2 + ll, text, lt2 + 1);
        free(st->stream_ledger);
        st->stream_ledger = n2;
    }
}

static bool sm_ceiling_adopt_final(void *ctx, const char *final_raw) {
    script_ctx_t *c = ctx;
    bool r = node_bool(seq_next(c, SEQ_CEILING_ADOPT_FINAL), false);
    json_t *o = ev("ceiling_adopt_final");
    evs(o, "final_raw", final_raw);
    evb(o, "ret", r);
    rec(c, o);
    return r;
}

static int sm_ceiling_split_first_send(void *ctx, const scf_tick_t *tick) {
    script_ctx_t *c = ctx;
    const json_t *e = seq_next(c, SEQ_CEILING_SPLIT_FIRST_SEND);
    int r = 0;   /* None */
    if (e && e->type == JSON_BOOL) r = e->bool_val ? 1 : 2;
    json_t *o = ev("ceiling_split_first_send");
    json_set(o, "tick", scf_tick_json(tick));
    if (r == 0) json_set(o, "ret", json_null());
    else evb(o, "ret", r == 1);
    rec(c, o);
    return r;
}

static bool sm_ceiling_final_cut(void *ctx) {
    script_ctx_t *c = ctx;
    bool r = node_bool(seq_next(c, SEQ_CEILING_FINAL_CUT), false);
    json_t *o = ev("ceiling_final_cut");
    evb(o, "ret", r);
    rec(c, o);
    return r;
}

static bool sm_ceiling_seal_cut(void *ctx, int split_at, bool final) {
    script_ctx_t *c = ctx;
    bool r = node_bool(seq_next(c, SEQ_CEILING_SEAL_CUT), false);
    json_t *o = ev("ceiling_seal_cut");
    evn(o, "split_at", split_at);
    evb(o, "final", final);
    evb(o, "ret", r);
    rec(c, o);
    return r;
}

static bool sm_ceiling_finish(void *ctx, const scf_tick_t *tick) {
    script_ctx_t *c = ctx;
    bool r = node_bool(seq_next(c, SEQ_CEILING_FINISH), false);
    json_t *o = ev("ceiling_finish");
    json_set(o, "tick", scf_tick_json(tick));
    evb(o, "ret", r);
    rec(c, o);
    return r;
}

static void sm_ceiling_segment_end(void *ctx, const scf_tick_t *tick,
                                   bool has_tick) {
    script_ctx_t *c = ctx;
    json_t *o = ev("ceiling_segment_end");
    json_set(o, "tick", has_tick ? scf_tick_json(tick) : json_null());
    rec(c, o);
}

static bool sm_ceiling_cancelled(void *ctx) {
    script_ctx_t *c = ctx;
    bool r = node_bool(seq_next(c, SEQ_CEILING_CANCELLED), false);
    json_t *o = ev("ceiling_cancelled");
    evb(o, "ret", r);
    rec(c, o);
    return r;
}

static void sm_ceiling_reset(void *ctx) {
    script_ctx_t *c = ctx;
    /* the live _reset_ceiling clears _ceiling_hit (observable state) */
    c->st->ceiling_hit = false;
    rec(c, ev("ceiling_reset"));
}

static bool sm_turn_reset(void *ctx) {
    script_ctx_t *c = ctx;
    bool r = node_bool(seq_next(c, SEQ_TURN_RESET), false);
    json_t *o = ev("turn_reset");
    evb(o, "ret", r);
    rec(c, o);
    return r;
}

static int sm_len_fn(void *ctx, const char *text) {
    script_ctx_t *c = ctx;
    const json_t *e = map_get(c, "len_fn", text);
    int r = (e && e->type == JSON_NUMBER) ? (int)e->num_val : scf_utf8_len(text);
    json_t *o = ev("len_fn");
    evs(o, "text", text);
    evn(o, "ret", r);
    rec(c, o);
    return r;
}

static void sm_redact_settled(void *ctx, const char *text, const char *prefix,
                              bool done, char **new_text, int *settled) {
    script_ctx_t *c = ctx;
    const json_t *e = seq_next(c, SEQ_REDACT_SETTLED);
    const char *nt = text;
    int stl = scf_utf8_len(text);
    if (e && e->type == JSON_OBJECT) {
        nt = json_get_str(e, "text", text);
        stl = (int)json_get_num(e, "settled", stl);
    }
    json_t *o = ev("redact_settled");
    evs(o, "text", text);
    evs(o, "prefix", prefix);
    evb(o, "done", done);
    json_t *r = json_object();
    evs(r, "text", nt);
    evn(r, "settled", stl);
    json_set(o, "ret", r);
    rec(c, o);
    *new_text = strdup(nt);
    *settled = stl;
}

static int sm_custom_unit_to_cp(void *ctx, const char *text, int limit) {
    script_ctx_t *c = ctx;
    const json_t *e = seq_next(c, SEQ_CUSTOM_UNIT_TO_CP);
    int r = (e && e->type == JSON_NUMBER) ? (int)e->num_val : limit;
    json_t *o = ev("custom_unit_to_cp");
    evs(o, "text", text);
    evn(o, "limit", limit);
    evn(o, "ret", r);
    rec(c, o);
    return r;
}

static char *sm_text_map(void *ctx, const char *key, const char *text) {
    script_ctx_t *c = ctx;
    const json_t *e = map_get(c, key, text);
    const char *r = (e && e->type == JSON_STRING) ? e->str_val : text;
    json_t *o = ev(key);
    evs(o, "text", text);
    evs(o, "ret", r);
    rec(c, o);
    return strdup(r);
}
static char *sm_display_payload(void *ctx, const char *text) {
    return sm_text_map(ctx, "display_payload", text);
}
static char *sm_clean_for_display(void *ctx, const char *text) {
    return sm_text_map(ctx, "clean_for_display", text);
}
static char *sm_display_text(void *ctx, const char *text) {
    return sm_text_map(ctx, "display_text", text);
}
static char *sm_compose_frame_content(void *ctx, const char *text) {
    return sm_text_map(ctx, "compose_frame_content", text);
}

static bool sm_stream_is_message(void *ctx) {
    script_ctx_t *c = ctx;
    bool r = node_bool(seq_next(c, SEQ_STREAM_IS_MESSAGE), false);
    json_t *o = ev("stream_is_message");
    evb(o, "ret", r);
    rec(c, o);
    return r;
}

static bool sm_bool_map(void *ctx, const char *key, const char *text) {
    script_ctx_t *c = ctx;
    const json_t *e = map_get(c, key, text);
    bool r = node_bool(e, false);
    json_t *o = ev(key);
    evs(o, "text", text);
    evb(o, "ret", r);
    rec(c, o);
    return r;
}
static bool sm_is_intentional_silence(void *ctx, const char *text) {
    return sm_bool_map(ctx, "is_intentional_silence_response", text);
}
static bool sm_is_partial_silence(void *ctx, const char *text) {
    return sm_bool_map(ctx, "is_partial_silence_marker", text);
}
static bool sm_ends_with_partial_silence(void *ctx, const char *text) {
    return sm_bool_map(ctx, "ends_with_partial_silence_marker", text);
}
static bool sm_has_pending_media(void *ctx, const char *text) {
    return sm_bool_map(ctx, "has_pending_media_directive", text);
}

static void sm_stale_preview_ids(void *ctx, bool segment_only,
                                 scf_strlist_t *out) {
    script_ctx_t *c = ctx;
    const json_t *e = seq_next(c, SEQ_STALE_PREVIEW_IDS);
    if (e && e->type == JSON_ARRAY) {
        for (size_t i = 0; i < e->c.count; i++) {
            const json_t *it = e->c.items[i];
            if (it && it->type == JSON_STRING)
                scf_strlist_append(out, it->str_val);
        }
    }
    scf_strlist_sort_dedup(out);
    json_t *o = ev("stale_preview_ids");
    evb(o, "segment_only", segment_only);
    json_t *a = json_array();
    for (size_t i = 0; i < out->count; i++)
        json_append(a, json_string(out->items[i]));
    json_set(o, "ret", a);
    rec(c, o);
}

static void sm_delete_previews(void *ctx, const scf_strlist_t *ids,
                               const char *skip, const char *label) {
    script_ctx_t *c = ctx;
    json_t *o = ev("delete_previews");
    json_t *a = json_array();
    for (size_t i = 0; i < ids->count; i++)
        json_append(a, json_string(ids->items[i]));
    json_set(o, "ids", a);
    evs(o, "skip", skip);
    evs(o, "label", label);
    rec(c, o);
}

static void sm_abandon_native_stream(void *ctx) {
    script_ctx_t *c = ctx;
    rec(c, ev("abandon_native_stream"));
}

static void sm_close_empty_native_bubble(void *ctx, const char *fail_log) {
    script_ctx_t *c = ctx;
    json_t *o = ev("close_empty_native_bubble");
    evs(o, "fail_log", fail_log);
    rec(c, o);
}

static void sm_flush_segment_tail(void *ctx) {
    script_ctx_t *c = ctx;
    rec(c, ev("flush_segment_tail_on_edit_failure"));
}

static bool sm_should_push_draft(void *ctx) {
    script_ctx_t *c = ctx;
    bool r = node_bool(seq_next(c, SEQ_SHOULD_PUSH_DRAFT), false);
    json_t *o = ev("should_push_draft");
    evb(o, "ret", r);
    rec(c, o);
    return r;
}

static int sm_before_finalize_hook(void *ctx) {
    script_ctx_t *c = ctx;
    const json_t *h = json_obj_get(c->seams, "hook");
    const char *r = (h && h->type == JSON_OBJECT)
                    ? json_get_str(h, "ret", "ok") : "ok";
    json_t *o = ev("before_finalize_hook");
    evs(o, "ret", r);
    rec(c, o);
    if (strcmp(r, "raise") == 0) return 1;
    if (strcmp(r, "awaitable") == 0) return 2;
    return 0;
}

static void sm_hook_await(void *ctx) {
    script_ctx_t *c = ctx;
    rec(c, ev("hook_await"));
}

static bool sm_flush_event_set(void *ctx, int event_id) {
    script_ctx_t *c = ctx;
    bool raised = json_get_bool(c->seams, "flush_set_raise", false);
    json_t *o = ev("flush_event_set");
    evn(o, "event", event_id);
    evb(o, "raised", raised);
    rec(c, o);
    return raised;
}

static void sm_chunk_list(void *ctx, int sid, const char *key, const char *text,
                          int limit, scf_strlist_t *out) {
    script_ctx_t *c = ctx;
    const json_t *e = seq_next(c, sid);
    if (e && e->type == JSON_ARRAY) {
        for (size_t i = 0; i < e->c.count; i++) {
            const json_t *it = e->c.items[i];
            if (it && it->type == JSON_STRING)
                scf_strlist_append(out, it->str_val);
        }
    } else {
        scf_strlist_append(out, text);
        scf_strlist_append(out, "");
    }
    json_t *o = ev(key);
    evs(o, "text", text);
    evn(o, "limit", limit);
    json_t *a = json_array();
    for (size_t i = 0; i < out->count; i++)
        json_append(a, json_string(out->items[i]));
    json_set(o, "ret", a);
    rec(c, o);
}
static void sm_truncate_for_stream(void *ctx, const char *text, int limit,
                                   scf_strlist_t *out) {
    sm_chunk_list(ctx, SEQ_TRUNCATE_FOR_STREAM, "truncate_for_stream", text,
                  limit, out);
}
static void sm_split_text_chunks(void *ctx, const char *text, int limit,
                                 scf_strlist_t *out) {
    sm_chunk_list(ctx, SEQ_SPLIT_TEXT_CHUNKS, "split_text_chunks", text,
                  limit, out);
}

static void wire_seams(scf_seams_t *s, script_ctx_t *c) {
    memset(s, 0, sizeof(*s));
    s->ctx = c;
    s->clock = sm_clock;
    s->send_or_edit = sm_send_or_edit;
    s->send_new_chunk = sm_send_new_chunk;
    s->send_fallback_final = sm_send_fallback_final;
    s->send_commentary = sm_send_commentary;
    s->flush_think_buffer = sm_flush_think_buffer;
    s->filter_accumulate = sm_filter_accumulate;
    s->ceiling_adopt_final = sm_ceiling_adopt_final;
    s->ceiling_split_first_send = sm_ceiling_split_first_send;
    s->ceiling_final_cut = sm_ceiling_final_cut;
    s->ceiling_seal_cut = sm_ceiling_seal_cut;
    s->ceiling_finish = sm_ceiling_finish;
    s->ceiling_segment_end = sm_ceiling_segment_end;
    s->ceiling_cancelled = sm_ceiling_cancelled;
    s->ceiling_reset = sm_ceiling_reset;
    s->turn_reset = sm_turn_reset;
    s->len_fn = sm_len_fn;
    s->redact_settled = sm_redact_settled;
    s->custom_unit_to_cp = sm_custom_unit_to_cp;
    s->display_payload = sm_display_payload;
    s->clean_for_display = sm_clean_for_display;
    s->display_text = sm_display_text;
    s->compose_frame_content = sm_compose_frame_content;
    s->stream_is_message = sm_stream_is_message;
    s->is_intentional_silence_response = sm_is_intentional_silence;
    s->is_partial_silence_marker = sm_is_partial_silence;
    s->ends_with_partial_silence_marker = sm_ends_with_partial_silence;
    s->has_pending_media_directive = sm_has_pending_media;
    s->stale_preview_ids = sm_stale_preview_ids;
    s->delete_previews = sm_delete_previews;
    s->abandon_native_stream = sm_abandon_native_stream;
    s->close_empty_native_bubble = sm_close_empty_native_bubble;
    s->flush_segment_tail_on_edit_failure = sm_flush_segment_tail;
    s->should_push_draft = sm_should_push_draft;
    s->before_finalize_hook = sm_before_finalize_hook;
    s->hook_await = sm_hook_await;
    s->flush_event_set = sm_flush_event_set;
    s->truncate_for_stream = sm_truncate_for_stream;
    s->split_text_chunks = sm_split_text_chunks;
}/* ── case loading ───────────────────────────────────────────────────────── */

static void fail_case(const char *msg) {
    fprintf(stderr, "HARNESS FAIL: %s\n", msg);
    exit(1);
}

static char *jstrdup(const json_t *v) {
    return (v && v->type == JSON_STRING) ? strdup(v->str_val) : NULL;
}

/* non-nullable Python str fields must arrive as JSON strings (fail-closed) */
static char *jstrdup_req(const json_t *v) {
    if (!v || v->type != JSON_STRING) fail_case("non-nullable state str not a string");
    return strdup(v->str_val);
}

static void load_strlist(scf_strlist_t *l, const json_t *a) {
    if (!a || a->type != JSON_ARRAY) fail_case("state list not an array");
    for (size_t i = 0; i < a->c.count; i++) {
        const json_t *it = a->c.items[i];
        if (!it || it->type != JSON_STRING) fail_case("state list item not str");
        scf_strlist_append(l, it->str_val);
    }
}

static void load_set(scf_strlist_t *l, const json_t *a) {
    load_strlist(l, a);
    scf_strlist_sort_dedup(l);
}

static void scf_set_str_pub(char **dst, char *src);

static void apply_state(scf_state_t *st, const json_t *state) {
    if (!state) return;
    if (state->type != JSON_OBJECT) fail_case("state not an object");
    for (size_t i = 0; i < state->c.count; i++) {
        const char *k = state->c.keys[i];
        const json_t *v = state->c.items[i];
        bool bv = json_get_bool(state, k, false);
        double nv = json_get_num(state, k, 0.0);
        /* strings (non-nullable unless noted) */
        if (!strcmp(k, "_accumulated")) { scf_set_str_pub(&st->accumulated, jstrdup_req(v)); }
        else if (!strcmp(k, "_stream_ledger")) { scf_set_str_pub(&st->stream_ledger, jstrdup_req(v)); }
        else if (!strcmp(k, "_redacted_prefix")) { scf_set_str_pub(&st->redacted_prefix, jstrdup_req(v)); }
        else if (!strcmp(k, "_settled_for")) { scf_set_str_pub(&st->settled_for, jstrdup_req(v)); }
        else if (!strcmp(k, "_last_sent_text")) { scf_set_str_pub(&st->last_sent_text, jstrdup_req(v)); }
        else if (!strcmp(k, "_fallback_prefix")) { scf_set_str_pub(&st->fallback_prefix, jstrdup_req(v)); }
        else if (!strcmp(k, "_boundary_placeholder")) { scf_set_str_pub(&st->boundary_placeholder, jstrdup_req(v)); }
        else if (!strcmp(k, "_boundary_reason")) { scf_set_str_pub(&st->boundary_reason, jstrdup_req(v)); }
        else if (!strcmp(k, "_chat_id")) { scf_set_str_pub(&st->chat_id, jstrdup_req(v)); }
        /* nullable strings */
        else if (!strcmp(k, "_message_id")) { scf_set_str_pub(&st->message_id, jstrdup(v)); }
        else if (!strcmp(k, "_delivered_final_text")) { scf_set_str_pub(&st->delivered_final_text, jstrdup(v)); }
        else if (!strcmp(k, "_initial_reply_to_id")) { scf_set_str_pub(&st->initial_reply_to_id, jstrdup(v)); }
        /* bools */
        else if (!strcmp(k, "_fallback_final_send")) st->fallback_final_send = bv;
        else if (!strcmp(k, "_fallback_preserve_partial_messages")) st->fallback_preserve_partial_messages = bv;
        else if (!strcmp(k, "_preview_uncertain")) st->preview_uncertain = bv;
        else if (!strcmp(k, "_tool_progress_active")) st->tool_progress_active = bv;
        else if (!strcmp(k, "_final_response_sent")) st->final_response_sent = bv;
        else if (!strcmp(k, "_final_content_delivered")) st->final_content_delivered = bv;
        else if (!strcmp(k, "_turn_split_delivery")) st->turn_split_delivery = bv;
        else if (!strcmp(k, "_delivery_ambiguous")) st->delivery_ambiguous = bv;
        else if (!strcmp(k, "_delivery_uncertain")) st->delivery_uncertain = bv;
        else if (!strcmp(k, "_already_sent")) st->already_sent = bv;
        else if (!strcmp(k, "_edit_supported")) st->edit_supported = bv;
        else if (!strcmp(k, "_last_edit_overflowed")) st->last_edit_overflowed = bv;
        else if (!strcmp(k, "_before_finalize_notified")) st->before_finalize_notified = bv;
        else if (!strcmp(k, "_use_draft_streaming")) st->use_draft_streaming = bv;
        else if (!strcmp(k, "_use_native_streaming")) st->use_native_streaming = bv;
        else if (!strcmp(k, "_native_stream_opened")) st->native_stream_opened = bv;
        else if (!strcmp(k, "_boundary_reopen")) st->boundary_reopen = bv;
        else if (!strcmp(k, "_awaiting_reopen_after_boundary")) st->awaiting_reopen_after_boundary = bv;
        else if (!strcmp(k, "_reopen_seeded_eagerly")) st->reopen_seeded_eagerly = bv;
        else if (!strcmp(k, "_ceiling_hit")) st->ceiling_hit = bv;
        else if (!strcmp(k, "_adapter_requires_finalize")) st->adapter_requires_finalize = bv;
        /* numbers */
        else if (!strcmp(k, "_last_edit_time")) st->last_edit_time = nv;
        else if (!strcmp(k, "_current_edit_interval")) st->current_edit_interval = nv;
        else if (!strcmp(k, "_flood_strikes")) st->flood_strikes = (int)nv;
        else if (!strcmp(k, "_native_last_pushed_len")) st->native_last_pushed_len = (int)nv;
        else if (!strcmp(k, "_safe_limit")) st->safe_limit = (int)nv;
        else if (!strcmp(k, "_message_created_ts")) {
            if (v && v->type == JSON_NUMBER) { st->message_created_ts = nv; st->message_created_ts_set = true; }
            else { st->message_created_ts = 0.0; st->message_created_ts_set = false; }
        }
        else if (!strcmp(k, "_draft_id")) {
            if (v && v->type == JSON_NUMBER) { st->draft_id = (long long)nv; st->draft_id_set = true; }
            else { st->draft_id = 0; st->draft_id_set = false; }
        }
        /* lists / sets */
        else if (!strcmp(k, "_tool_progress_lines")) load_strlist(&st->tool_progress_lines, v);
        else if (!strcmp(k, "_delivered_commentary_texts")) load_strlist(&st->delivered_commentary_texts, v);
        else if (!strcmp(k, "_delivered_segment_texts")) load_strlist(&st->delivered_segment_texts, v);
        else if (!strcmp(k, "_preview_message_ids")) load_set(&st->preview_message_ids, v);
        else if (!strcmp(k, "_segment_preview_message_ids")) load_set(&st->segment_preview_message_ids, v);
        else if (!strcmp(k, "_superseded_reply_ids")) load_set(&st->superseded_reply_ids, v);
        else if (!strcmp(k, "_last_segment_delivery")) {
            if (!v || v->type == JSON_NULL) {
                st->last_segment_delivery_set = false;
            } else {
                if (v->type != JSON_ARRAY || v->c.count != 4)
                    fail_case("_last_segment_delivery not [id, ts, text, ids]");
                st->last_segment_delivery_set = true;
                scf_set_str_pub(&st->lsd_id, jstrdup_req(v->c.items[0]));
                const json_t *ts = v->c.items[1];
                if (ts && ts->type == JSON_NUMBER) { st->lsd_ts = ts->num_val; st->lsd_ts_set = true; }
                else { st->lsd_ts = 0.0; st->lsd_ts_set = false; }
                scf_set_str_pub(&st->lsd_text, jstrdup_req(v->c.items[2]));
                load_set(&st->lsd_ids, v->c.items[3]);
            }
        }
        else {
            fprintf(stderr, "HARNESS FAIL: unknown state key %s\n", k);
            exit(1);
        }
    }
}

/* scf_set_str is static in the port; the harness needs the same replace
 * semantics for owned strings (free old, adopt new). */
static void scf_set_str_pub(char **dst, char *src) {
    free(*dst);
    *dst = src;
}

static void load_cfg(scf_state_t *st, const json_t *cfg) {
    if (!cfg) return;
    if (cfg->type != JSON_OBJECT) fail_case("cfg not an object");
    st->edit_interval = json_get_num(cfg, "edit_interval", st->edit_interval);
    st->buffer_threshold = (int)json_get_num(cfg, "buffer_threshold", st->buffer_threshold);
    if (json_has(cfg, "cursor")) scf_set_str_pub(&st->cursor, jstrdup_req(json_obj_get(cfg, "cursor")));
    st->buffer_only = json_get_bool(cfg, "buffer_only", st->buffer_only);
    st->fresh_final_after_seconds = json_get_num(cfg, "fresh_final_after_seconds", st->fresh_final_after_seconds);
    if (json_has(cfg, "transport")) scf_set_str_pub(&st->transport, jstrdup_req(json_obj_get(cfg, "transport")));
    if (json_has(cfg, "chat_type")) scf_set_str_pub(&st->chat_type, jstrdup_req(json_obj_get(cfg, "chat_type")));
    st->current_edit_interval = st->edit_interval;
}

static void load_tick(scf_tick_t *t, const json_t *jt) {
    scf_tick_init(t);
    if (!jt) return;
    if (jt->type != JSON_OBJECT) fail_case("tick not an object");
    t->got_done = json_get_bool(jt, "got_done", false);
    t->got_segment_break = json_get_bool(jt, "got_segment_break", false);
    t->got_flush = json_get_bool(jt, "got_flush", false);
    const json_t *fe = json_obj_get(jt, "flush_event");
    t->flush_event = (fe && fe->type == JSON_NUMBER) ? (int)fe->num_val : -1;
    t->got_reopen_seed = json_get_bool(jt, "got_reopen_seed", false);
    const json_t *ab = json_obj_get(jt, "approval_boundary");
    if (ab && ab->type == JSON_ARRAY && ab->c.count == 2) {
        t->approval_boundary_set = true;
        t->ab_future = (int)ab->c.items[0]->num_val;
        t->ab_cancelled = ab->c.items[1]->bool_val;
    }
    t->commentary_text = jstrdup(json_obj_get(jt, "commentary_text"));
    t->update_visible = json_get_bool(jt, "update_visible", false);
    t->draft_final_fresh_send = json_get_bool(jt, "draft_final_fresh_send", false);
}

static void load_queue(scf_queue_t *q, const json_t *arr) {
    if (!arr) return;
    if (arr->type != JSON_ARRAY) fail_case("queue not an array");
    for (size_t i = 0; i < arr->c.count; i++) {
        const json_t *it = arr->c.items[i];
        if (!it || it->type != JSON_OBJECT) fail_case("queue item not object");
        const char *kind = json_get_str(it, "kind", "");
        scf_qitem_t item;
        memset(&item, 0, sizeof(item));
        item.event_id = -1;
        item.future_id = -1;
        if (!strcmp(kind, "text")) { item.kind = SCF_Q_TEXT; item.text = jstrdup(json_obj_get(it, "text")); }
        else if (!strcmp(kind, "done")) item.kind = SCF_Q_DONE;
        else if (!strcmp(kind, "new_segment")) item.kind = SCF_Q_NEW_SEGMENT;
        else if (!strcmp(kind, "reopen_seed")) item.kind = SCF_Q_REOPEN_SEED;
        else if (!strcmp(kind, "final_text")) { item.kind = SCF_Q_FINAL_TEXT; item.text = jstrdup(json_obj_get(it, "text")); }
        else if (!strcmp(kind, "tool_progress")) { item.kind = SCF_Q_TOOL_PROGRESS; item.text = jstrdup(json_obj_get(it, "line")); }
        else if (!strcmp(kind, "commentary")) { item.kind = SCF_Q_COMMENTARY; item.text = jstrdup(json_obj_get(it, "text")); }
        else if (!strcmp(kind, "flush")) { item.kind = SCF_Q_FLUSH; item.event_id = (int)json_get_num(it, "event", -1); }
        else if (!strcmp(kind, "approval_boundary")) {
            item.kind = SCF_Q_APPROVAL_BOUNDARY;
            item.future_id = (int)json_get_num(it, "future", -1);
            item.cancelled = json_get_bool(it, "cancelled", false);
        }
        else { fprintf(stderr, "HARNESS FAIL: unknown queue kind %s\n", kind); exit(1); }
        scf_queue_put(q, &item);
        free(item.text);
    }
}

static json_t *queue_remaining_json(const scf_queue_t *q) {
    json_t *a = json_array();
    for (size_t i = q->head; i < q->count; i++) {
        const scf_qitem_t *it = &q->items[i];
        json_t *o = json_object();
        switch (it->kind) {
        case SCF_Q_TEXT:
            json_set(o, "kind", json_string("text"));
            json_set(o, "text", json_string(it->text ? it->text : ""));
            break;
        case SCF_Q_DONE: json_set(o, "kind", json_string("done")); break;
        case SCF_Q_NEW_SEGMENT: json_set(o, "kind", json_string("new_segment")); break;
        case SCF_Q_REOPEN_SEED: json_set(o, "kind", json_string("reopen_seed")); break;
        case SCF_Q_FINAL_TEXT:
            json_set(o, "kind", json_string("final_text"));
            json_set(o, "text", json_string(it->text ? it->text : ""));
            break;
        case SCF_Q_TOOL_PROGRESS:
            json_set(o, "kind", json_string("tool_progress"));
            json_set(o, "line", json_string(it->text ? it->text : ""));
            break;
        case SCF_Q_COMMENTARY:
            json_set(o, "kind", json_string("commentary"));
            json_set(o, "text", json_string(it->text ? it->text : ""));
            break;
        case SCF_Q_FLUSH:
            json_set(o, "kind", json_string("flush"));
            json_set(o, "event", json_number(it->event_id));
            break;
        case SCF_Q_APPROVAL_BOUNDARY:
            json_set(o, "kind", json_string("approval_boundary"));
            json_set(o, "future", json_number(it->future_id));
            json_set(o, "cancelled", json_bool(it->cancelled));
            break;
        default: fail_case("bad queue kind in remaining"); break;
        }
        json_append(a, o);
    }
    return a;
}

/* ── dispatch ───────────────────────────────────────────────────────────── */

static void run_case(const json_t *rec_in) {
    const char *func = json_get_str(rec_in, "func", "");
    const json_t *in = json_obj_get(rec_in, "in");
    if (!in || in->type != JSON_OBJECT) fail_case("case missing in object");

    scf_state_t st;
    scf_state_init(&st);
    load_cfg(&st, json_obj_get(in, "cfg"));
    apply_state(&st, json_obj_get(in, "state"));
    st.draft_id_counter = (long long)json_get_num(in, "draft_id_counter", 0);
    st.hook_present = json_get_bool(in, "hook_present", false);
    load_queue(&st.queue, json_obj_get(in, "queue"));

    scf_tick_t tick;
    load_tick(&tick, json_obj_get(in, "tick"));
    const json_t *args = json_obj_get(in, "args");

    script_ctx_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.in = in;
    ctx.seams = json_obj_get(in, "seams");
    ctx.st = &st;
    ctx.trace = json_array();
    scf_seams_t seams;
    wire_seams(&seams, &ctx);

    json_t *ret = NULL;
    bool takes_tick = false, is_drain = false, is_wake = false;
    scf_tick_t drain_ret;
    scf_tick_init(&drain_ret);

    if (!strcmp(func, "drain_queue")) {
        is_drain = true;
        drain_ret = scf_drain_queue_flow(&st, &seams);
        ret = scf_tick_json(&drain_ret);
    } else if (!strcmp(func, "should_edit")) {
        takes_tick = true;
        ret = json_bool(scf_should_edit_flow(&st, &tick, &seams));
    } else if (!strcmp(func, "tick_ends_text")) {
        takes_tick = true;
        ret = json_bool(scf_tick_ends_text(&st, &tick, &seams));
    } else if (!strcmp(func, "redact_settled_text")) {
        bool done = json_get_bool(args, "done", false);
        ret = json_number(scf_redact_settled_text(&st, done, &seams));
    } else if (!strcmp(func, "push_update")) {
        takes_tick = true;
        scf_push_update_flow(&st, &tick, &seams);
        ret = json_null();
    } else if (!strcmp(func, "seal_overflow_heads")) {
        bool final = json_get_bool(args, "final", false);
        bool done = json_get_bool(args, "done", false);
        ret = json_bool(scf_seal_overflow_heads_flow(&st, final, done, &seams));
    } else if (!strcmp(func, "split_first_send")) {
        takes_tick = true;
        ret = json_bool(scf_split_first_send_flow(&st, &tick, &seams));
    } else if (!strcmp(func, "finalize_edit")) {
        const char *text = json_get_str(args, "text", "");
        bool record = json_get_bool(args, "record", true);
        ret = json_bool(scf_finalize_edit_flow(&st, text, record, &seams));
    } else if (!strcmp(func, "finalize_edit_path")) {
        takes_tick = true;
        scf_finalize_edit_path_flow(&st, &tick, &seams);
        ret = json_null();
    } else if (!strcmp(func, "finalize_turn")) {
        takes_tick = true;
        scf_finalize_turn_flow(&st, &tick, &seams);
        ret = json_null();
    } else if (!strcmp(func, "end_segment")) {
        takes_tick = true;
        scf_end_segment_flow(&st, &tick, &seams);
        ret = json_null();
    } else if (!strcmp(func, "on_cancelled")) {
        scf_on_cancelled_flow(&st, &seams);
        ret = json_null();
    } else if (!strcmp(func, "adopt_final_text")) {
        const char *final_raw = json_get_str(args, "final_raw", "");
        scf_adopt_final_text_flow(&st, final_raw, &seams);
        ret = json_null();
    } else if (!strcmp(func, "adopt_recovered_reply")) {
        const char *final_raw = json_get_str(args, "final_raw", "");
        ret = json_bool(scf_adopt_recovered_reply(&st, final_raw, &seams));
    } else if (!strcmp(func, "wake_flush_waiters")) {
        is_wake = true;
        scf_wake_flush_waiters(&st, &seams);
        ret = json_null();
    } else if (!strcmp(func, "mark_skip_redundant_finalize")) {
        scf_mark_skip_redundant_finalize(&st, &seams);
        ret = json_null();
    } else {
        fprintf(stderr, "HARNESS FAIL: unknown func %s\n", func);
        exit(1);
    }

    if (st.raised) {
        json_free(ret);
        ret = json_object();
        json_set(ret, "__raise__", json_string(st.raised));
    }

    json_t *out = json_object();
    json_set(out, "ret", ret);
    json_set(out, "trace", ctx.trace);          /* ownership moves */
    json_set(out, "state", scf_state_json(&st));
    if (takes_tick) json_set(out, "tick", scf_tick_json(&tick));
    if (is_drain || is_wake) json_set(out, "queue_remaining", queue_remaining_json(&st.queue));

    json_t *record = json_object();
    json_set(record, "func", json_string(func));
    json_set(record, "in", json_copy(in));
    json_set(record, "out", out);
    char *line = json_serialize(record);
    printf("%s\n", line);
    free(line);
    json_free(record);
    scf_tick_free(&tick);
    scf_tick_free(&drain_ret);
    scf_state_free(&st);
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    FILE *f = stdin;
    if (argc > 1) {
        f = fopen(argv[1], "rb");
        if (!f) { fprintf(stderr, "cannot open %s\n", argv[1]); return 1; }
    }
    static char line[65536];
    int cases = 0;
    while (fgets(line, sizeof(line), f)) {
        size_t n = strlen(line);
        while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r')) line[--n] = '\0';
        if (n == 0) continue;
        char *err = NULL;
        json_t *rec_in = json_parse(line, &err);
        if (!rec_in) {
            fprintf(stderr, "HARNESS FAIL: bad JSON line: %s\n", err ? err : "?");
            free(err);
            return 1;
        }
        run_case(rec_in);
        json_free(rec_in);
        cases++;
    }
    if (f != stdin) fclose(f);
    if (cases == 0) {
        fprintf(stderr, "HARNESS FAIL: zero cases read\n");
        return 1;
    }
    return 0;
}
