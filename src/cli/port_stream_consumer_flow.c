/*
 * port_stream_consumer_flow.c — streaming-delivery FLOW cores of
 * gateway/stream_consumer.py (upstream 3371c03df6).
 *
 * Decision-sequence ports: every branch, call order, short-circuit and state
 * mutation mirrors the live method line-for-line; all I/O (sends, clock,
 * queue events, hooks) and out-of-module leaves run through scf_seams_t.
 * See include/port_stream_consumer_flow.h for the seam contract and the
 * oracle doctrine (tests/oracle_stream_consumer_flow.py).
 *
 * Python semantics carried explicitly:
 *  - str indices/len are CODEPOINTS (scf_utf8_* helpers), never bytes.
 *  - `x or y` / `if x` truthiness: None and "" are falsy, every non-empty
 *    string is truthy; `is None` / `is not None` are identity tests ("" is
 *    NOT None) — each call site keeps its own test.
 *  - `and`/`or` short-circuit: a seam on the right of a falsy left operand
 *    is never called (the trace proves it).
 *  - asyncio awaits are seam calls in await order; a scripted raising send
 *    unwinds immediately (st->raised), skipping the pending assignment just
 *    like the live exception, and contextlib.suppress sites swallow it.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "port_stream_consumer_flow.h"

/* ── small string utilities ─────────────────────────────────────────────── */

static char *scf_strdup(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

/* Replace an owned string (NULL stays NULL). */
static void scf_set_str(char **dst, const char *src) {
    char *n = scf_strdup(src);
    free(*dst);
    *dst = n;
}

static char *scf_strjoin(const char *a, const char *b) {
    size_t la = strlen(a), lb = strlen(b);
    char *p = malloc(la + lb + 1);
    if (p) {
        memcpy(p, a, la);
        memcpy(p + la, b, lb + 1);
    }
    return p;
}

/* ── string list ────────────────────────────────────────────────────────── */

void scf_strlist_append(scf_strlist_t *l, const char *s) {
    if (l->count == l->cap) {
        size_t nc = l->cap ? l->cap * 2 : 8;
        char **ni = realloc(l->items, nc * sizeof(char *));
        if (!ni) return;
        l->items = ni;
        l->cap = nc;
    }
    l->items[l->count++] = scf_strdup(s ? s : "");
}

void scf_strlist_clear(scf_strlist_t *l) {
    for (size_t i = 0; i < l->count; i++) free(l->items[i]);
    l->count = 0;
}

void scf_strlist_free(scf_strlist_t *l) {
    scf_strlist_clear(l);
    free(l->items);
    l->items = NULL;
    l->cap = 0;
}

static int scf_str_cmp(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

void scf_strlist_sort_dedup(scf_strlist_t *l) {
    if (l->count < 2) return;
    qsort(l->items, l->count, sizeof(char *), scf_str_cmp);
    size_t w = 1;
    for (size_t i = 1; i < l->count; i++) {
        if (strcmp(l->items[i], l->items[w - 1]) == 0) {
            free(l->items[i]);
        } else {
            l->items[w++] = l->items[i];
        }
    }
    l->count = w;
}

bool scf_strlist_pop(scf_strlist_t *l) {
    if (l->count == 0) return false;
    free(l->items[--l->count]);
    return true;
}

/* ── queue ──────────────────────────────────────────────────────────────── */

static void scf_qitem_free(scf_qitem_t *it) { free(it->text); it->text = NULL; }

void scf_queue_put(scf_queue_t *q, const scf_qitem_t *item) {
    if (q->count == q->cap) {
        size_t nc = q->cap ? q->cap * 2 : 8;
        scf_qitem_t *ni = realloc(q->items, nc * sizeof(scf_qitem_t));
        if (!ni) return;
        q->items = ni;
        q->cap = nc;
    }
    q->items[q->count] = *item;
    q->items[q->count].text = scf_strdup(item->text);
    q->count++;
}

bool scf_queue_get(scf_queue_t *q, scf_qitem_t *out) {
    if (q->head >= q->count) return false;   /* queue.Empty */
    *out = q->items[q->head++];
    return true;
}

void scf_queue_free(scf_queue_t *q) {
    for (size_t i = q->head; i < q->count; i++) scf_qitem_free(&q->items[i]);
    free(q->items);
    q->items = NULL;
    q->count = q->cap = q->head = 0;
}

/* ── tick ───────────────────────────────────────────────────────────────── */

void scf_tick_init(scf_tick_t *t) {
    memset(t, 0, sizeof(*t));
    t->flush_event = -1;
    t->ab_future = -1;
}

void scf_tick_free(scf_tick_t *t) {
    free(t->commentary_text);
    t->commentary_text = NULL;
}

bool scf_tick_is_interim(const scf_tick_t *t) {
    /* not got_done and not got_segment_break and commentary_text is None */
    return !t->got_done && !t->got_segment_break && t->commentary_text == NULL;
}

json_t *scf_tick_json(const scf_tick_t *t) {
    json_t *o = json_object();
    json_set(o, "got_done", json_bool(t->got_done));
    json_set(o, "got_segment_break", json_bool(t->got_segment_break));
    json_set(o, "got_flush", json_bool(t->got_flush));
    json_set(o, "flush_event", t->flush_event >= 0
             ? json_number(t->flush_event) : json_null());
    json_set(o, "got_reopen_seed", json_bool(t->got_reopen_seed));
    if (t->approval_boundary_set) {
        json_t *ab = json_array();
        json_append(ab, json_number(t->ab_future));
        json_append(ab, json_bool(t->ab_cancelled));
        json_set(o, "approval_boundary", ab);
    } else {
        json_set(o, "approval_boundary", json_null());
    }
    json_set(o, "commentary_text", t->commentary_text
             ? json_string(t->commentary_text) : json_null());
    json_set(o, "update_visible", json_bool(t->update_visible));
    json_set(o, "draft_final_fresh_send", json_bool(t->draft_final_fresh_send));
    return o;
}

/* ── UTF-8 / Python str semantics ───────────────────────────────────────── */

int scf_utf8_len(const char *s) {
    int n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xC0) != 0x80) n++;
    return n;
}

/* Byte offset of codepoint index cp (clamped to [0, len]). */
int scf_utf8_byte_offset(const char *s, int cp) {
    if (cp <= 0) return 0;
    int seen = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        if ((*p & 0xC0) != 0x80) {
            if (seen == cp) return (int)(p - (const unsigned char *)s);
            seen++;
        }
        p++;
    }
    return (int)strlen(s);
}

/* s[start:end] in codepoints; end < 0 means "to the end". Clamped like
 * Python slices (past-the-end yields "", never an error). */
char *scf_utf8_slice(const char *s, int start, int end) {
    int n = scf_utf8_len(s);
    if (start < 0) start = 0;
    if (start > n) start = n;
    if (end < 0 || end > n) end = n;
    if (end < start) end = start;
    int b0 = scf_utf8_byte_offset(s, start);
    int b1 = scf_utf8_byte_offset(s, end);
    size_t len = (size_t)(b1 - b0);
    char *p = malloc(len + 1);
    if (p) {
        memcpy(p, s + b0, len);
        p[len] = '\0';
    }
    return p;
}

bool scf_utf8_endswith(const char *s, const char *suffix) {
    size_t ls = strlen(s), lf = strlen(suffix);
    if (lf > ls) return false;
    return memcmp(s + ls - lf, suffix, lf) == 0;
}

char *scf_utf8_removesuffix(const char *s, const char *suffix) {
    if (suffix[0] && scf_utf8_endswith(s, suffix)) {
        size_t keep = strlen(s) - strlen(suffix);
        char *p = malloc(keep + 1);
        if (p) {
            memcpy(p, s, keep);
            p[keep] = '\0';
        }
        return p;
    }
    return scf_strdup(s);
}

/* Decode one codepoint at p (valid UTF-8 assumed); advances *pp. */
static unsigned int scf_cp_decode(const unsigned char **pp) {
    const unsigned char *p = *pp;
    unsigned int cp = p[0];
    int extra = 0;
    if (cp >= 0xF0) { cp &= 0x07; extra = 3; }
    else if (cp >= 0xE0) { cp &= 0x0F; extra = 2; }
    else if (cp >= 0xC0) { cp &= 0x1F; extra = 1; }
    for (int i = 0; i < extra && p[1]; i++) {
        p++;
        cp = (cp << 6) | (p[0] & 0x3F);
    }
    *pp = p + 1;
    return cp;
}

/* Python str.isspace() whitespace set (the characters str.strip() removes). */
static bool scf_cp_isspace(unsigned int cp) {
    if (cp == 0x20 || (cp >= 0x09 && cp <= 0x0D)) return true;
    if (cp >= 0x1C && cp <= 0x1F) return true;
    if (cp == 0x85 || cp == 0xA0 || cp == 0x1680) return true;
    if (cp >= 0x2000 && cp <= 0x200A) return true;
    if (cp == 0x2028 || cp == 0x2029 || cp == 0x202F || cp == 0x205F)
        return true;
    if (cp == 0x3000) return true;
    return false;
}

char *scf_utf8_strip(const char *s) {
    const unsigned char *p = (const unsigned char *)s;
    const unsigned char *first = NULL;
    const unsigned char *last_end = NULL;
    while (*p) {
        const unsigned char *start = p;
        unsigned int cp = scf_cp_decode(&p);
        if (!scf_cp_isspace(cp)) {
            if (!first) first = start;
            last_end = p;
        }
    }
    if (!first) return scf_strdup("");
    size_t len = (size_t)(last_end - first);
    char *out = malloc(len + 1);
    if (out) {
        memcpy(out, first, len);
        out[len] = '\0';
    }
    return out;
}

char *scf_utf8_lstrip_newlines(const char *s) {
    while (*s == '\n') s++;
    return scf_strdup(s);
}

/* str.rfind("\n", 0, end_cp): codepoint index of the last '\n' strictly
 * before end_cp, or -1. */
int scf_utf8_rfind_newline(const char *s, int end_cp) {
    int idx = 0, found = -1;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if ((*p & 0xC0) != 0x80) {
            if (idx >= end_cp) break;
            if (*p == '\n') found = idx;
            idx++;
        }
    }
    return found;
}

/* ── state ──────────────────────────────────────────────────────────────── */

/* _DEFAULT_BOUNDARY_PLACEHOLDER = "⏸ 等待审批中..." (byte-exact UTF-8). */
#define SCF_BOUNDARY_PLACEHOLDER \
    "\xe2\x8f\xb8 \xe7\xad\x89\xe5\xbe\x85\xe5\xae\xa1\xe6\x89\xb9\xe4\xb8\xad..."
/* DEFAULT_STREAMING_CURSOR = " ▉" */
#define SCF_DEFAULT_CURSOR " \xe2\x96\x89"

static void scf_lsd_clear(scf_state_t *st) {
    st->last_segment_delivery_set = false;
    scf_set_str(&st->lsd_id, NULL);
    st->lsd_ts = 0.0;
    st->lsd_ts_set = false;
    scf_set_str(&st->lsd_text, NULL);
    scf_strlist_clear(&st->lsd_ids);
}

void scf_state_init(scf_state_t *st) {
    memset(st, 0, sizeof(*st));
    st->edit_interval = 0.8;
    st->buffer_threshold = 24;
    st->cursor = scf_strdup(SCF_DEFAULT_CURSOR);
    st->buffer_only = false;
    st->fresh_final_after_seconds = 0.0;
    st->transport = scf_strdup("edit");
    st->chat_type = scf_strdup("");
    st->chat_id = scf_strdup("c1");
    st->initial_reply_to_id = NULL;
    st->adapter_requires_finalize = false;
    st->hook_present = false;
    st->message_id = NULL;
    st->message_created_ts = 0.0;
    st->message_created_ts_set = false;
    st->accumulated = scf_strdup("");
    st->stream_ledger = scf_strdup("");
    st->redacted_prefix = scf_strdup("");
    st->settled_for = scf_strdup("");
    st->last_sent_text = scf_strdup("");
    st->fallback_final_send = false;
    st->fallback_prefix = scf_strdup("");
    st->fallback_preserve_partial_messages = false;
    st->preview_uncertain = false;
    st->tool_progress_active = false;
    st->final_response_sent = false;
    st->final_content_delivered = false;
    st->delivered_final_text = NULL;
    st->turn_split_delivery = false;
    st->delivery_ambiguous = false;
    st->delivery_uncertain = false;
    st->already_sent = false;
    st->edit_supported = true;
    st->last_edit_time = 0.0;
    st->last_edit_overflowed = false;
    st->flood_strikes = 0;
    st->current_edit_interval = st->edit_interval;
    st->before_finalize_notified = false;
    st->use_draft_streaming = false;
    st->draft_id = 0;
    st->draft_id_set = false;
    st->use_native_streaming = false;
    st->native_stream_opened = false;
    st->native_last_pushed_len = 0;
    st->boundary_placeholder = scf_strdup(SCF_BOUNDARY_PLACEHOLDER);
    st->boundary_reason = scf_strdup("Approval");
    st->boundary_reopen = false;
    st->awaiting_reopen_after_boundary = false;
    st->reopen_seeded_eagerly = false;
    st->ceiling_hit = false;
    st->safe_limit = 4000;
    st->draft_id_counter = 0;
    st->raised = NULL;
}

void scf_state_free(scf_state_t *st) {
    free(st->cursor); free(st->transport); free(st->chat_type); free(st->chat_id);
    free(st->initial_reply_to_id); free(st->message_id);
    free(st->accumulated); free(st->stream_ledger);
    free(st->redacted_prefix); free(st->settled_for); free(st->last_sent_text);
    free(st->fallback_prefix); free(st->delivered_final_text);
    free(st->boundary_placeholder); free(st->boundary_reason);
    free(st->lsd_id); free(st->lsd_text);
    scf_strlist_free(&st->tool_progress_lines);
    scf_strlist_free(&st->delivered_commentary_texts);
    scf_strlist_free(&st->delivered_segment_texts);
    scf_strlist_free(&st->preview_message_ids);
    scf_strlist_free(&st->segment_preview_message_ids);
    scf_strlist_free(&st->superseded_reply_ids);
    scf_strlist_free(&st->lsd_ids);
    scf_queue_free(&st->queue);
}

static json_t *scf_strlist_json(const scf_strlist_t *l) {
    json_t *a = json_array();
    for (size_t i = 0; i < l->count; i++)
        json_append(a, json_string(l->items[i]));
    return a;
}

json_t *scf_state_json(const scf_state_t *st) {
    json_t *o = json_object();
    json_set(o, "accumulated", json_string(st->accumulated));
    json_set(o, "adapter_requires_finalize", json_bool(st->adapter_requires_finalize));
    json_set(o, "already_sent", json_bool(st->already_sent));
    json_set(o, "awaiting_reopen_after_boundary", json_bool(st->awaiting_reopen_after_boundary));
    json_set(o, "before_finalize_notified", json_bool(st->before_finalize_notified));
    json_set(o, "boundary_placeholder", json_string(st->boundary_placeholder));
    json_set(o, "boundary_reason", json_string(st->boundary_reason));
    json_set(o, "boundary_reopen", json_bool(st->boundary_reopen));
    json_set(o, "ceiling_hit", json_bool(st->ceiling_hit));
    json_set(o, "current_edit_interval", json_number(st->current_edit_interval));
    json_set(o, "delivered_commentary_texts", scf_strlist_json(&st->delivered_commentary_texts));
    json_set(o, "delivered_final_text", st->delivered_final_text
             ? json_string(st->delivered_final_text) : json_null());
    json_set(o, "delivered_segment_texts", scf_strlist_json(&st->delivered_segment_texts));
    json_set(o, "delivery_ambiguous", json_bool(st->delivery_ambiguous));
    json_set(o, "delivery_uncertain", json_bool(st->delivery_uncertain));
    json_set(o, "draft_id", st->draft_id_set
             ? json_number((double)st->draft_id) : json_null());
    json_set(o, "draft_id_counter", json_number((double)st->draft_id_counter));
    json_set(o, "edit_supported", json_bool(st->edit_supported));
    json_set(o, "fallback_final_send", json_bool(st->fallback_final_send));
    json_set(o, "fallback_prefix", json_string(st->fallback_prefix));
    json_set(o, "fallback_preserve_partial_messages", json_bool(st->fallback_preserve_partial_messages));
    json_set(o, "final_content_delivered", json_bool(st->final_content_delivered));
    json_set(o, "final_response_sent", json_bool(st->final_response_sent));
    json_set(o, "flood_strikes", json_number(st->flood_strikes));
    json_set(o, "last_edit_overflowed", json_bool(st->last_edit_overflowed));
    json_set(o, "last_edit_time", json_number(st->last_edit_time));
    if (st->last_segment_delivery_set) {
        json_t *t = json_array();
        json_append(t, json_string(st->lsd_id));
        json_append(t, st->lsd_ts_set ? json_number(st->lsd_ts) : json_null());
        json_append(t, json_string(st->lsd_text));
        json_append(t, scf_strlist_json(&st->lsd_ids));
        json_set(o, "last_segment_delivery", t);
    } else {
        json_set(o, "last_segment_delivery", json_null());
    }
    json_set(o, "last_sent_text", json_string(st->last_sent_text));
    json_set(o, "message_created_ts", st->message_created_ts_set
             ? json_number(st->message_created_ts) : json_null());
    json_set(o, "message_id", st->message_id
             ? json_string(st->message_id) : json_null());
    json_set(o, "native_last_pushed_len", json_number(st->native_last_pushed_len));
    json_set(o, "native_stream_opened", json_bool(st->native_stream_opened));
    json_set(o, "preview_message_ids", scf_strlist_json(&st->preview_message_ids));
    json_set(o, "preview_uncertain", json_bool(st->preview_uncertain));
    json_set(o, "redacted_prefix", json_string(st->redacted_prefix));
    json_set(o, "reopen_seeded_eagerly", json_bool(st->reopen_seeded_eagerly));
    json_set(o, "safe_limit", json_number(st->safe_limit));
    json_set(o, "segment_preview_message_ids", scf_strlist_json(&st->segment_preview_message_ids));
    json_set(o, "settled_for", json_string(st->settled_for));
    json_set(o, "stream_ledger", json_string(st->stream_ledger));
    json_set(o, "superseded_reply_ids", scf_strlist_json(&st->superseded_reply_ids));
    json_set(o, "tool_progress_active", json_bool(st->tool_progress_active));
    json_set(o, "tool_progress_lines", scf_strlist_json(&st->tool_progress_lines));
    json_set(o, "turn_split_delivery", json_bool(st->turn_split_delivery));
    json_set(o, "use_draft_streaming", json_bool(st->use_draft_streaming));
    json_set(o, "use_native_streaming", json_bool(st->use_native_streaming));
    return o;
}/* ── internal class-defined collaborators (flow-local static mirrors) ─────
 * _cumulative_transport / _reopen_seed_pending / _overflows /
 * _mark_final_delivered / _record_turn_final_payload / _reset_message_state /
 * _clear_turn_final_flags / _reset_segment_state / _bump_draft_id /
 * _notify_before_finalize / _signal_flush. The PoP-claimed ports of
 * _reset_segment_state / _notify_before_finalize / _signal_flush live in
 * src/gateway/stream_consumer.c; these static mirrors exist so the flows
 * below run their real decision sequences without exporting duplicate
 * globals. */

/* PoP-free mirror of _cumulative_transport (stream_draft or native). */
static bool scf_cumulative_transport(scf_state_t *st, const scf_seams_t *s) {
    bool sm = s->stream_is_message(s->ctx);
    bool stream_draft = sm && st->use_draft_streaming;
    return stream_draft || st->use_native_streaming;
}

static bool scf_reopen_seed_pending(const scf_state_t *st) {
    return st->use_native_streaming && st->awaiting_reopen_after_boundary
        && !st->native_stream_opened;
}

static bool scf_overflows(scf_state_t *st, const scf_seams_t *s) {
    return s->len_fn(s->ctx, st->accumulated) > st->safe_limit;
}

/* _record_turn_final_payload: split delivery records the un-truncated ledger. */
static void scf_record_turn_final_payload(scf_state_t *st, const char *text,
                                          const scf_seams_t *s) {
    if (st->turn_split_delivery && st->stream_ledger[0])
        text = st->stream_ledger;
    char *dp = s->display_payload(s->ctx, text);
    scf_set_str(&st->delivered_final_text, dp);
    free(dp);
}

/* _mark_final_delivered(record=None): both flags; record != None -> payload. */
static void scf_mark_final_delivered(scf_state_t *st, const char *record,
                                     const scf_seams_t *s) {
    st->final_response_sent = true;
    st->final_content_delivered = true;
    if (record != NULL)
        scf_record_turn_final_payload(st, record, s);
}

static void scf_clear_turn_final_flags(scf_state_t *st) {
    st->final_response_sent = false;
    st->final_content_delivered = false;
    scf_set_str(&st->delivered_final_text, NULL);
    st->turn_split_delivery = false;
    st->delivery_ambiguous = false;
    st->delivery_uncertain = false;
}

static void scf_reset_message_state(scf_state_t *st, const scf_seams_t *s) {
    scf_set_str(&st->message_id, NULL);
    st->message_created_ts = 0.0;
    st->message_created_ts_set = false;
    scf_set_str(&st->accumulated, "");
    scf_set_str(&st->stream_ledger, "");
    scf_set_str(&st->redacted_prefix, "");
    scf_set_str(&st->settled_for, "");
    scf_set_str(&st->last_sent_text, "");
    st->fallback_final_send = false;
    scf_set_str(&st->fallback_prefix, "");
    st->fallback_preserve_partial_messages = false;
    scf_strlist_clear(&st->segment_preview_message_ids);
    st->preview_uncertain = false;
    scf_strlist_clear(&st->tool_progress_lines);
    st->tool_progress_active = false;
    scf_clear_turn_final_flags(st);
    s->ceiling_reset(s->ctx);
}

static void scf_bump_draft_id(scf_state_t *st) {
    st->draft_id_counter += 1;
    st->draft_id = st->draft_id_counter;
    st->draft_id_set = true;
}

/* _reset_segment_state(preserve_no_edit=...): retain the segment's visible
 * text, then reset; draft bump only for non-stream-is-message drafts. */
static void scf_reset_segment_state(scf_state_t *st, bool preserve_no_edit,
                                    const scf_seams_t *s) {
    st->delivery_uncertain = false;
    if (preserve_no_edit && st->message_id
        && strcmp(st->message_id, "__no_edit__") == 0)
        return;
    char *cleaned = s->clean_for_display(s->ctx, st->last_sent_text);
    char *finalized = scf_utf8_strip(cleaned);
    free(cleaned);
    if (finalized[0]) {
        scf_strlist_append(&st->delivered_segment_texts, finalized);
        scf_lsd_clear(st);
        if (st->message_id && st->message_id[0]
            && strcmp(st->message_id, "__no_edit__") != 0) {
            st->last_segment_delivery_set = true;
            scf_set_str(&st->lsd_id, st->message_id);
            st->lsd_ts = st->message_created_ts;
            st->lsd_ts_set = st->message_created_ts_set;
            scf_set_str(&st->lsd_text, st->last_sent_text);
            s->stale_preview_ids(s->ctx, true, &st->lsd_ids);
            scf_strlist_sort_dedup(&st->lsd_ids);
        }
    }
    free(finalized);
    scf_reset_message_state(st, s);
    if (st->use_draft_streaming) {
        bool sm = s->stream_is_message(s->ctx);
        if (!sm)
            scf_bump_draft_id(st);
    }
}

/* _notify_before_finalize: once; hook errors swallowed; awaitable awaited. */
static void scf_notify_before_finalize(scf_state_t *st, const scf_seams_t *s) {
    if (st->before_finalize_notified)
        return;
    st->before_finalize_notified = true;
    if (st->hook_present) {
        int r = s->before_finalize_hook(s->ctx);
        /* contextlib.suppress(Exception): r == 1 (raise) is swallowed */
        if (r == 2)
            s->hook_await(s->ctx);
    }
}

/* _signal_flush(flush_event): None -> no-op; set() errors swallowed. */
static void scf_signal_flush(scf_state_t *st, int event_id,
                             const scf_seams_t *s) {
    (void)st;
    if (event_id >= 0) {
        bool raised = s->flush_event_set(s->ctx, event_id);
        (void)raised;   /* contextlib.suppress(Exception) */
    }
}

/* ── port 1: delta intake / coalescing ──────────────────────────────────── */

/* PoP: scf_drain_queue_flow @ gateway/stream_consumer.py:_drain_queue */
scf_tick_t scf_drain_queue_flow(scf_state_t *st, const scf_seams_t *s) {
    scf_tick_t tick;
    scf_tick_init(&tick);
    for (;;) {
        scf_qitem_t item;
        if (!scf_queue_get(&st->queue, &item))
            return tick;                    /* queue.Empty -> return tick */
        if (item.kind == SCF_Q_DONE) {
            tick.got_done = true;
            scf_qitem_free(&item);
            return tick;
        }
        if (item.kind == SCF_Q_NEW_SEGMENT) {
            tick.got_segment_break = true;
            scf_qitem_free(&item);
            return tick;
        }
        if (item.kind == SCF_Q_REOPEN_SEED) {
            tick.got_reopen_seed = true;
            scf_qitem_free(&item);
            return tick;
        }
        /* kind = item[0] for tuples; text deltas fall to the else branch */
        if (item.kind == SCF_Q_FINAL_TEXT) {
            scf_adopt_final_text_flow(st, item.text, s);
        } else if (item.kind == SCF_Q_TOOL_PROGRESS) {
            if (st->use_native_streaming) {
                scf_strlist_append(&st->tool_progress_lines, item.text);
                st->tool_progress_active = true;
            }
        } else if (item.kind == SCF_Q_APPROVAL_BOUNDARY) {
            tick.approval_boundary_set = true;
            tick.ab_future = item.future_id;
            tick.ab_cancelled = item.cancelled;
            scf_qitem_free(&item);
            return tick;
        } else if (item.kind == SCF_Q_COMMENTARY) {
            scf_set_str(&tick.commentary_text, item.text);
            scf_qitem_free(&item);
            return tick;
        } else if (item.kind == SCF_Q_FLUSH) {
            tick.got_flush = true;
            tick.got_segment_break = true;
            tick.flush_event = item.event_id;
            scf_qitem_free(&item);
            return tick;
        } else {
            s->filter_accumulate(s->ctx, item.text);
        }
        scf_qitem_free(&item);
        if (st->raised)
            return tick;                    /* adoption exception unwinds */
    }
}

/* ── port 2: edit-throttle / flood-backoff / silence-deferral decision ──── */

/* PoP: scf_should_edit_flow @ gateway/stream_consumer.py:_should_edit */
bool scf_should_edit_flow(scf_state_t *st, const scf_tick_t *tick,
                          const scf_seams_t *s) {
    if (!scf_tick_is_interim(tick))
        return true;
    if (st->buffer_only)
        return false;
    bool should_edit;
    if (st->use_native_streaming) {
        /* no platform edit-rate limit: push every delta immediately */
        should_edit = (st->accumulated[0] != '\0') || st->tool_progress_active;
    } else if (st->use_draft_streaming) {
        should_edit = s->should_push_draft(s->ctx);
    } else {
        double elapsed = s->clock(s->ctx) - st->last_edit_time;
        /* interval gate OR codepoint debounce; an active flood backoff
         * disables the debounce branch (only the interval may fire). */
        bool by_interval = (elapsed >= st->current_edit_interval)
                           && (st->accumulated[0] != '\0');
        bool by_threshold = (scf_utf8_len(st->accumulated) >= st->buffer_threshold)
                            && !st->flood_strikes;
        should_edit = by_interval || by_threshold;
    }
    char *cleaned = s->display_text(s->ctx, st->accumulated);
    int settled = scf_redact_settled_text(st, false, s);
    char *prefix = scf_utf8_slice(st->accumulated, 0, settled);
    char *frame = s->display_text(s->ctx, prefix);
    free(prefix);
    bool result = false;
    if (should_edit
        && !s->is_partial_silence_marker(s->ctx, cleaned)
        && !s->ends_with_partial_silence_marker(s->ctx, st->accumulated)
        && !s->has_pending_media_directive(s->ctx, cleaned)
        && !s->has_pending_media_directive(s->ctx, frame))
        result = true;
    free(cleaned);
    free(frame);
    return result;
}

/* ── port 3: does this tick end the current text? ───────────────────────── */

/* PoP: scf_tick_ends_text @ gateway/stream_consumer.py:_tick_ends_text */
bool scf_tick_ends_text(scf_state_t *st, const scf_tick_t *tick,
                        const scf_seams_t *s) {
    if (tick->got_done || tick->got_segment_break)
        return true;
    if (tick->commentary_text != NULL)
        return !scf_cumulative_transport(st, s);
    return false;
}

/* ── port 4: settled-boundary redaction (in place) ──────────────────────── */

/* PoP: scf_redact_settled_text @ gateway/stream_consumer.py:_redact_settled_text */
int scf_redact_settled_text(scf_state_t *st, bool done, const scf_seams_t *s) {
    if (!done && strcmp(st->accumulated, st->settled_for) == 0)
        return scf_utf8_len(st->redacted_prefix);   /* cached: one scan */
    char *new_text = NULL;
    int settled = 0;
    s->redact_settled(s->ctx, st->accumulated, st->redacted_prefix, done,
                      &new_text, &settled);
    scf_set_str(&st->accumulated, new_text);
    free(new_text);
    char *pref = scf_utf8_slice(st->accumulated, 0, settled);
    scf_set_str(&st->redacted_prefix, pref);
    free(pref);
    scf_set_str(&st->settled_for, done ? "" : st->accumulated);
    return settled;
}

/* ── port 5: frame push (cursor / compose / send / clock) ───────────────── */

/* PoP: scf_push_update_flow @ gateway/stream_consumer.py:_push_update */
void scf_push_update_flow(scf_state_t *st, scf_tick_t *tick,
                          const scf_seams_t *s) {
    bool ends = scf_tick_ends_text(st, tick, s);
    int settled = scf_redact_settled_text(st, ends, s);
    char *display = scf_utf8_slice(st->accumulated, 0, settled);
    if (scf_tick_is_interim(tick)) {
        if (st->use_native_streaming) {
            char *composed = s->compose_frame_content(s->ctx, display);
            free(display);
            display = composed;
            if (display[0] && st->cursor[0]) {
                char *joined = scf_strjoin(display, st->cursor);
                free(display);
                display = joined;
            }
        } else {
            char *joined = scf_strjoin(display, st->cursor);
            free(display);
            display = joined;
        }
    }
    tick->draft_final_fresh_send = tick->got_done && st->use_draft_streaming
                                   && st->message_id == NULL;
    bool raised = false;
    bool ok = s->send_or_edit(s->ctx, display,
                              tick->got_done || tick->got_segment_break,
                              tick->got_done, true, &raised);
    free(display);
    if (raised) {
        st->raised = "RuntimeError";    /* assignment skipped, unwind */
        return;
    }
    tick->update_visible = ok;
    st->last_edit_time = s->clock(s->ctx);
    st->tool_progress_active = false;
}/* ── port 6: overflow sealing loop ──────────────────────────────────────── */

/* PoP: scf_seal_overflow_heads_flow @ gateway/stream_consumer.py:_seal_overflow_heads */
bool scf_seal_overflow_heads_flow(scf_state_t *st, bool final, bool done,
                                  const scf_seams_t *s) {
    if (final && s->ceiling_final_cut(s->ctx))
        return false;   /* final edit would pass the reply's message cap */
    bool sent = false;
    for (;;) {
        if (!(scf_overflows(st, s) && st->message_id != NULL
              && st->edit_supported))
            break;
        if (sent && s->turn_reset(s->ctx))
            break;
        int settled = scf_redact_settled_text(st, done || final, s);
        int cp_budget = s->custom_unit_to_cp(s->ctx, st->accumulated,
                                             st->safe_limit);
        int split_at = scf_utf8_rfind_newline(st->accumulated, cp_budget);
        if (split_at < cp_budget / 2)
            split_at = cp_budget;
        if (settled < split_at)
            split_at = settled;     /* never seal unsettled text */
        char *head = scf_utf8_slice(st->accumulated, 0, split_at);
        char *stripped = scf_utf8_strip(head);
        bool blank = (stripped[0] == '\0');
        free(stripped);
        if (blank) {
            free(head);
            break;
        }
        if (s->ceiling_seal_cut(s->ctx, split_at, final)) {
            free(head);
            break;
        }
        /* finalize=True, is_turn_final=False: a sealed head is not the answer */
        bool raised = false;
        bool ok = s->send_or_edit(s->ctx, head, true, false, false, &raised);
        free(head);
        if (raised) {
            st->raised = "RuntimeError";
            return false;
        }
        sent = true;
        if (st->fallback_final_send || !ok)
            break;      /* keep the full text intact for the fallback final */
        char *tail = scf_utf8_slice(st->accumulated, split_at, -1);
        char *lst = scf_utf8_lstrip_newlines(tail);
        free(tail);
        scf_set_str(&st->accumulated, lst);
        free(lst);
        scf_set_str(&st->message_id, NULL);
        scf_set_str(&st->last_sent_text, "");
        st->turn_split_delivery = true;
    }
    return sent;
}

/* ── port 7: first-send overflow split ──────────────────────────────────── */

/* PoP: scf_split_first_send_flow @ gateway/stream_consumer.py:_split_first_send */
bool scf_split_first_send_flow(scf_state_t *st, scf_tick_t *tick,
                               const scf_seams_t *s) {
    int handed_off = s->ceiling_split_first_send(s->ctx, tick);
    if (handed_off != 0)
        return handed_off == 1;     /* None -> continue; True/False -> return */
    bool ends = scf_tick_ends_text(st, tick, s);
    int settled = scf_redact_settled_text(st, ends, s);
    char *head = scf_utf8_slice(st->accumulated, 0, settled);
    char *held = scf_utf8_slice(st->accumulated, settled, -1);
    char *hstrip = scf_utf8_strip(head);
    bool blank = (hstrip[0] == '\0');
    free(hstrip);
    if (blank) {
        free(head);
        free(held);
        return false;   /* nothing settled yet */
    }
    scf_strlist_t chunks = {0};
    if (s->len_fn(s->ctx, head) > st->safe_limit) {
        s->truncate_for_stream(s->ctx, head, st->safe_limit, &chunks);
        if (chunks.count <= 1) {
            /* malformed/legacy adapter result must still be splittable */
            scf_strlist_clear(&chunks);
            s->split_text_chunks(s->ctx, head, st->safe_limit, &chunks);
        }
    } else {
        scf_strlist_append(&chunks, head);
        scf_strlist_append(&chunks, "");
    }
    char *reply_to = scf_strdup(st->initial_reply_to_id);
    bool heads_delivered = chunks.count > 1;
    size_t n_send = chunks.count > 0 ? chunks.count - 1 : 0;  /* chunks[:-1] */
    for (size_t index = 0; index < n_send; index++) {
        if (index && s->turn_reset(s->ctx)) {
            free(reply_to);
            free(head);
            free(held);
            scf_strlist_free(&chunks);
            return false;   /* /stop or /new landed during a head send */
        }
        char *new_id = s->send_new_chunk(s->ctx, chunks.items[index], reply_to,
                                         tick->got_done, index == 0);
        if (new_id && strcmp(new_id, "__partial__") == 0) {
            if (tick->got_done)
                s->send_fallback_final(s->ctx, st->accumulated);
            bool ret = tick->got_done;
            free(new_id);
            free(reply_to);
            free(head);
            free(held);
            scf_strlist_free(&chunks);
            return ret;
        }
        if (new_id == NULL || (reply_to && strcmp(new_id, reply_to) == 0)) {
            heads_delivered = false;    /* keep full text for the fallback */
            free(new_id);
            break;
        }
        /* ack-lost head: no id to thread against, but it is still shown */
        free(reply_to);
        if (strcmp(new_id, "__no_edit__") == 0) {
            reply_to = NULL;
            free(new_id);
        } else {
            reply_to = new_id;      /* ownership moves to reply_to */
        }
    }
    if (heads_delivered) {
        /* drop the " (n/n)" indicator: the tail is the LIVE preview */
        const char *tail = chunks.items[chunks.count - 1];
        char indicator[64];
        snprintf(indicator, sizeof(indicator), " (%zu/%zu)",
                 chunks.count, chunks.count);
        char *tail_rem = scf_utf8_removesuffix(tail, indicator);
        char *new_acc = scf_strjoin(tail_rem, held);
        free(tail_rem);
        scf_set_str(&st->accumulated, new_acc);
        free(new_acc);
        /* flag BEFORE the tail send: fresh-final owns only the active msg */
        st->turn_split_delivery = true;
    }
    /* heads are sealed (or a later head failed): never edit a sealed msg */
    scf_set_str(&st->message_id, NULL);
    st->message_created_ts = 0.0;
    st->message_created_ts_set = false;
    scf_set_str(&st->last_sent_text, "");
    st->last_edit_time = s->clock(s->ctx);
    if (chunks.count > 1 && s->turn_reset(s->ctx)) {
        free(reply_to);
        free(head);
        free(held);
        scf_strlist_free(&chunks);
        return false;   /* run() abandons the dead turn instead of the tail */
    }
    if (tick->got_done) {
        bool tail_delivered;
        if (st->accumulated[0] == '\0') {
            tail_delivered = true;
        } else {
            bool raised = false;
            tail_delivered = s->send_or_edit(s->ctx, st->accumulated, true,
                                             true, false, &raised);
            if (raised) {
                st->raised = "RuntimeError";
                free(reply_to);
                free(head);
                free(held);
                scf_strlist_free(&chunks);
                return false;
            }
        }
        st->final_response_sent = heads_delivered && tail_delivered;
        if (st->final_response_sent) {
            st->turn_split_delivery = true;
            scf_mark_final_delivered(st, st->accumulated, s);
        }
        free(reply_to);
        free(head);
        free(held);
        scf_strlist_free(&chunks);
        return true;
    }
    if (tick->got_segment_break) {
        st->fallback_final_send = false;
        scf_set_str(&st->fallback_prefix, "");
    }
    free(reply_to);
    free(head);
    free(held);
    scf_strlist_free(&chunks);
    return false;
}

/* ── port 8: finalize edit + delivery marking ───────────────────────────── */

/* PoP: scf_finalize_edit_flow @ gateway/stream_consumer.py:_finalize_edit */
bool scf_finalize_edit_flow(scf_state_t *st, const char *text, bool record,
                            const scf_seams_t *s) {
    bool raised = false;
    bool ok = s->send_or_edit(s->ctx, text, true, true, false, &raised);
    if (raised) {
        st->raised = "RuntimeError";    /* flag assignment skipped */
        return false;
    }
    st->final_response_sent = ok;
    if (st->final_response_sent)
        scf_mark_final_delivered(st, record ? text : NULL, s);
    return st->final_response_sent;
}

/* ── port 9: edit-transport finalize ladder ─────────────────────────────── */

/* PoP: scf_finalize_edit_path_flow @ gateway/stream_consumer.py:_finalize_edit_path */
void scf_finalize_edit_path_flow(scf_state_t *st, const scf_tick_t *tick,
                                 const scf_seams_t *s) {
    if (st->fallback_final_send) {
        s->send_fallback_final(s->ctx, st->accumulated);
    } else if (st->final_response_sent) {
        /* fresh-final already delivered; a second finalize would duplicate */
        scf_mark_final_delivered(st, st->accumulated, s);
    } else if (tick->update_visible
               && (!st->adapter_requires_finalize || st->last_edit_overflowed
                   || tick->draft_final_fresh_send)) {
        scf_mark_skip_redundant_finalize(st, s);
    } else if (st->message_id && st->message_id[0]) {
        bool ok = scf_finalize_edit_flow(st, st->accumulated, true, s);
        if (st->raised)
            return;
        if (!ok && st->fallback_final_send)
            s->send_fallback_final(s->ctx, st->accumulated);
    } else if (!st->already_sent) {
        scf_finalize_edit_flow(st, st->accumulated, true, s);
    }
}/* ── port 10: got_done finalization orchestration ───────────────────────── */

/* PoP: scf_finalize_turn_flow @ gateway/stream_consumer.py:_finalize_turn */
void scf_finalize_turn_flow(scf_state_t *st, scf_tick_t *tick,
                            const scf_seams_t *s) {
    if (s->ceiling_finish(s->ctx, tick))
        return;
    if (st->accumulated[0] || st->message_id != NULL || st->already_sent)
        scf_notify_before_finalize(st, s);
    if (scf_reopen_seed_pending(st) && !st->accumulated[0]) {
        /* lazy reopen, no post-prompt content: skip lone-placeholder
         * finalize (logger.debug only — no seam) */
    } else if (st->reopen_seeded_eagerly && st->native_stream_opened
               && !st->accumulated[0] && !tick->update_visible) {
        /* eager seed, no content: close the empty typing bubble */
        s->close_empty_native_bubble(s->ctx,
                                     "Eager-seed empty finalize failed: %s");
    } else if (st->use_native_streaming) {
        if (!tick->update_visible) {
            const char *txt = st->accumulated[0] ? st->accumulated
                                                 : "\xe2\x9c\x85"; /* ✅ */
            scf_finalize_edit_flow(st, txt, false, s);
            if (st->raised)
                return;
        } else {
            scf_mark_final_delivered(st, NULL, s);
        }
    } else if (st->accumulated[0]) {
        scf_finalize_edit_path_flow(st, tick, s);
        if (st->raised)
            return;
    }
    if (st->final_content_delivered && st->superseded_reply_ids.count > 0) {
        s->delete_previews(s->ctx, &st->superseded_reply_ids, st->message_id,
                           "Recovered reply");
        scf_strlist_clear(&st->superseded_reply_ids);
    }
}

/* ── port 11: tool-boundary segment cleanup ─────────────────────────────── */

/* PoP: scf_end_segment_flow @ gateway/stream_consumer.py:_end_segment */
void scf_end_segment_flow(scf_state_t *st, scf_tick_t *tick,
                          const scf_seams_t *s) {
    if (scf_cumulative_transport(st, s))
        return;     /* cumulative transports must NOT reset */
    s->ceiling_segment_end(s->ctx, tick, true);
    if (st->accumulated[0] && !tick->update_visible
        && !(st->message_id && strcmp(st->message_id, "__no_edit__") == 0))
        s->flush_segment_tail_on_edit_failure(s->ctx);
    scf_reset_segment_state(st, true, s);
}

/* ── port 12: cancellation best-effort final edit ───────────────────────── */

/* PoP: scf_on_cancelled_flow @ gateway/stream_consumer.py:_on_cancelled */
void scf_on_cancelled_flow(scf_state_t *st, const scf_seams_t *s) {
    if (s->ceiling_cancelled(s->ctx))
        return;
    bool best_effort_ok = false;
    if (st->accumulated[0] && st->message_id && st->message_id[0]) {
        bool raised = false;
        bool ok = s->send_or_edit(s->ctx, st->accumulated, true, false, false,
                                  &raised);
        /* contextlib.suppress(Exception): a raising send leaves
         * best_effort_ok False and does NOT unwind */
        if (!raised)
            best_effort_ok = ok;
    } else if (st->message_id == NULL) {
        /* draft path keeps _message_id=None: seal in place */
        s->abandon_native_stream(s->ctx);
    }
    if (best_effort_ok && !st->final_response_sent)
        scf_mark_final_delivered(st, st->accumulated, s);
}

/* ── port 13: authoritative-final adoption ──────────────────────────────── */

/* PoP: scf_adopt_final_text_flow @ gateway/stream_consumer.py:_adopt_final_text */
void scf_adopt_final_text_flow(scf_state_t *st, const char *final_raw,
                               const scf_seams_t *s) {
    if (s->ceiling_adopt_final(s->ctx, final_raw))
        return;
    if (!(st->accumulated[0] || (st->message_id && st->message_id[0])
          || st->last_sent_text[0]))
        return;     /* no-stream turn keeps the gateway's final-send ownership */
    if (scf_adopt_recovered_reply(st, final_raw, s))
        return;
    if (st->raised)
        return;
    if (!st->turn_split_delivery) {
        char *final_payload = s->clean_for_display(s->ctx, final_raw);
        if (final_payload[0]) {
            char *acc_clean = s->clean_for_display(s->ctx, st->accumulated);
            if (strcmp(final_payload, acc_clean) != 0) {
                scf_set_str(&st->accumulated, final_raw);
                scf_set_str(&st->stream_ledger, final_raw);
            }
            free(acc_clean);
        }
        free(final_payload);
        return;
    }
    /* split delivery: append only the suffix when the final strictly
     * prefix-extends the ledger */
    const char *ledger = st->stream_ledger;
    int ledger_len = scf_utf8_len(ledger);
    if (ledger[0]
        && strncmp(final_raw, ledger, strlen(ledger)) == 0
        && scf_utf8_len(final_raw) > ledger_len) {
        char *suffix = scf_utf8_slice(final_raw, ledger_len, -1);
        char *joined = scf_strjoin(st->accumulated, suffix);
        scf_set_str(&st->accumulated, joined);
        free(joined);
        free(suffix);
        scf_set_str(&st->stream_ledger, final_raw);
    }
}

/* ── port 14: marker-followed reply reconciliation ──────────────────────── */

/* PoP: scf_adopt_recovered_reply @ gateway/stream_consumer.py:_adopt_recovered_reply */
bool scf_adopt_recovered_reply(scf_state_t *st, const char *final_raw,
                               const scf_seams_t *s) {
    /* short-circuit guard chain, left to right */
    if (st->message_id && st->message_id[0])
        return false;
    if (scf_cumulative_transport(st, s))
        return false;
    if (!st->last_segment_delivery_set)
        return false;
    char *c1 = s->clean_for_display(s->ctx, st->accumulated);
    bool sil_acc = s->is_intentional_silence_response(s->ctx, c1);
    free(c1);
    if (!sil_acc)
        return false;
    char *c2 = s->clean_for_display(s->ctx, final_raw);
    bool sil_final = s->is_intentional_silence_response(s->ctx, c2);
    free(c2);
    if (sil_final)
        return false;
    /* unpack _last_segment_delivery */
    if (st->lsd_ids.count == 1) {
        char *p1 = s->display_payload(s->ctx, st->lsd_text);
        char *p2 = s->display_payload(s->ctx, final_raw);
        bool equal = (strcmp(p1, p2) == 0);
        free(p1);
        free(p2);
        if (equal) {
            scf_set_str(&st->accumulated, "");
            scf_set_str(&st->stream_ledger, "");
            scf_mark_final_delivered(st, final_raw, s);
            return true;
        }
    }
    /* reopen its own message so the normal edit/fallback path replaces it */
    scf_set_str(&st->message_id, st->lsd_id);
    st->message_created_ts = st->lsd_ts;
    st->message_created_ts_set = st->lsd_ts_set;
    scf_set_str(&st->last_sent_text, st->lsd_text);
    st->already_sent = true;
    scf_strlist_clear(&st->segment_preview_message_ids);
    scf_strlist_append(&st->segment_preview_message_ids, st->lsd_id);
    scf_strlist_sort_dedup(&st->segment_preview_message_ids);
    scf_strlist_clear(&st->preview_message_ids);
    for (size_t i = 0; i < st->lsd_ids.count; i++)
        scf_strlist_append(&st->preview_message_ids, st->lsd_ids.items[i]);
    scf_strlist_sort_dedup(&st->preview_message_ids);
    scf_strlist_clear(&st->superseded_reply_ids);
    for (size_t i = 0; i < st->lsd_ids.count; i++) {
        if (strcmp(st->lsd_ids.items[i], st->lsd_id) != 0)
            scf_strlist_append(&st->superseded_reply_ids, st->lsd_ids.items[i]);
    }
    scf_strlist_sort_dedup(&st->superseded_reply_ids);
    if (!scf_strlist_pop(&st->delivered_segment_texts)) {
        st->raised = "IndexError";    /* live list.pop() on empty */
        return false;
    }
    scf_lsd_clear(st);
    scf_set_str(&st->accumulated, final_raw);
    scf_set_str(&st->stream_ledger, final_raw);
    return true;
}

/* ── port 15: wake still-queued flush barriers on consumer death ────────── */

/* PoP: scf_wake_flush_waiters @ gateway/stream_consumer.py:_wake_flush_waiters */
void scf_wake_flush_waiters(scf_state_t *st, const scf_seams_t *s) {
    /* contextlib.suppress(Exception) around the drain; queue.Empty ends it */
    for (;;) {
        scf_qitem_t item;
        if (!scf_queue_get(&st->queue, &item))
            break;
        if (item.kind == SCF_Q_FLUSH)
            scf_signal_flush(st, item.event_id, s);
        scf_qitem_free(&item);
    }
}

/* ── port 16: mark the turn final delivered by a prior mid-stream edit ──── */

/* PoP: scf_mark_skip_redundant_finalize @ gateway/stream_consumer.py:_mark_skip_redundant_finalize */
void scf_mark_skip_redundant_finalize(scf_state_t *st, const scf_seams_t *s) {
    /* record what was ACKED, not _accumulated; drop a trailing cursor */
    const char *acked = st->last_sent_text[0] ? st->last_sent_text
                                              : st->accumulated;
    char *record;
    if (st->cursor[0] && scf_utf8_endswith(acked, st->cursor)) {
        int end_cp = scf_utf8_len(acked) - scf_utf8_len(st->cursor);
        record = scf_utf8_slice(acked, 0, end_cp);
    } else {
        record = scf_strdup(acked);
    }
    scf_mark_final_delivered(st, record, s);
    free(record);
}
