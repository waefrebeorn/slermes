/*
 * port_stream_consumer_flow.h — streaming-delivery FLOW cores of
 * gateway/stream_consumer.py (GatewayStreamConsumer).
 *
 * Ports the DECISION sequences of the streaming pipeline: delta intake
 * (_drain_queue), buffer/coalesce + edit-throttle timing (_should_edit),
 * settled-text redaction boundaries (_redact_settled_text), overflow sealing
 * and first-send splitting (_seal_overflow_heads / _split_first_send), frame
 * push (_push_update), finalization ladder (_finalize_turn /
 * _finalize_edit_path / _finalize_edit), segment cleanup (_end_segment),
 * cancellation (_on_cancelled), authoritative-final adoption
 * (_adopt_final_text / _adopt_recovered_reply), flush-barrier wakeup
 * (_wake_flush_waiters) and the redundant-finalize skip
 * (_mark_skip_redundant_finalize).
 *
 * All network/clock I/O and every out-of-module leaf is injected through the
 * scf_seams_t vtable: the decision cores perform NO I/O themselves. The
 * harness wires each seam to a recording stub driven by the case's scripted
 * results; tests/oracle_stream_consumer_flow.py drives the LIVE Python class
 * (AST-extracted, mixin bases stubbed) under the SAME scripts and compares
 * the ordered seam trace + return value + mutated state. Async awaits are
 * modeled as seam calls in await order; a scripted {"raise": true} send
 * result models an exception (propagated, or swallowed where the live code
 * uses contextlib.suppress).
 *
 * Text-normalization leaves that other ports own (_clean_for_display,
 * _display_text, _display_payload, _compose_frame_content, _stream_is_message)
 * are seams here too: their results are scripted per input text, so the flow
 * comparison stays about decision order, not re-ported leaf behavior.
 *
 * Strings are UTF-8; Python str indices are CODEPOINTS, so every slice /
 * len / rfind in the live code goes through the scf_utf8_* helpers.
 * Ownership: state strings/lists are owned by scf_state_t (scf_state_free);
 * seam out-strings (send_new_chunk, display_*, compose_frame_content,
 * redact_settled new_text) are malloc'd and owned by the ENGINE caller flow
 * (freed after use); tick.commentary_text is owned by the tick.
 */

#ifndef PORT_STREAM_CONSUMER_FLOW_H
#define PORT_STREAM_CONSUMER_FLOW_H

#include <stdbool.h>
#include <stddef.h>

#include "libjson/json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── string list (Python list[str]; also used as a sorted set[str]) ─────── */
typedef struct scf_strlist {
    char **items;
    size_t count;
    size_t cap;
} scf_strlist_t;

void scf_strlist_append(scf_strlist_t *l, const char *s);   /* copies s */
void scf_strlist_clear(scf_strlist_t *l);
void scf_strlist_free(scf_strlist_t *l);
void scf_strlist_sort_dedup(scf_strlist_t *l);              /* set semantics */
bool scf_strlist_pop(scf_strlist_t *l);                     /* false = empty */

/* ── queue item (Python queue.Queue entries; sentinels are bare kinds) ──── */
enum {
    SCF_Q_TEXT = 0,          /* str delta -> _filter_and_accumulate */
    SCF_Q_DONE,              /* _DONE */
    SCF_Q_NEW_SEGMENT,       /* _NEW_SEGMENT */
    SCF_Q_REOPEN_SEED,       /* _REOPEN_SEED */
    SCF_Q_FINAL_TEXT,        /* (_FINAL_TEXT, text) */
    SCF_Q_TOOL_PROGRESS,     /* (_TOOL_PROGRESS, line) */
    SCF_Q_COMMENTARY,        /* (_COMMENTARY, text) */
    SCF_Q_FLUSH,             /* (_FLUSH, event) */
    SCF_Q_APPROVAL_BOUNDARY  /* (_APPROVAL_BOUNDARY, future, cancelled_flag) */
};

typedef struct scf_qitem {
    int kind;
    char *text;       /* TEXT / FINAL_TEXT / TOOL_PROGRESS / COMMENTARY */
    int event_id;     /* FLUSH: threading.Event identity (-1 = none) */
    int future_id;    /* APPROVAL_BOUNDARY: future identity */
    bool cancelled;   /* APPROVAL_BOUNDARY: cancelled_flag["cancelled"] */
} scf_qitem_t;

typedef struct scf_queue {
    scf_qitem_t *items;
    size_t count;
    size_t cap;
    size_t head;      /* next get_nowait index */
} scf_queue_t;

void scf_queue_put(scf_queue_t *q, const scf_qitem_t *item); /* copies */
bool scf_queue_get(scf_queue_t *q, scf_qitem_t *out);        /* false = Empty */
void scf_queue_free(scf_queue_t *q);

/* ── _Tick dataclass mirror ─────────────────────────────────────────────── */
typedef struct scf_tick {
    bool got_done;
    bool got_segment_break;
    bool got_flush;
    int flush_event;            /* -1 = None, else Event identity */
    bool got_reopen_seed;
    bool approval_boundary_set; /* approval_boundary is not None */
    int ab_future;
    bool ab_cancelled;
    char *commentary_text;      /* NULL = None (owned) */
    bool update_visible;        /* set by _push_update */
    bool draft_final_fresh_send;/* set by _push_update */
} scf_tick_t;

void scf_tick_init(scf_tick_t *t);
void scf_tick_free(scf_tick_t *t);
/* _Tick.is_interim property */
bool scf_tick_is_interim(const scf_tick_t *t);
/* JSON model of the tick (caller frees); flush_event/approval_boundary use
 * the integer identities the harness assigned. */
json_t *scf_tick_json(const scf_tick_t *t);

/* ── consumer state mirror (attributes the 16 flows read/write) ─────────── */
typedef struct scf_state {
    /* StreamConsumerConfig */
    double edit_interval;
    int buffer_threshold;
    char *cursor;                 /* owned, "" = falsy */
    bool buffer_only;
    double fresh_final_after_seconds;
    char *transport;
    char *chat_type;
    char *chat_id;
    char *initial_reply_to_id;    /* NULL = None */
    bool adapter_requires_finalize; /* REQUIRES_EDIT_FINALIZE is True */
    bool hook_present;            /* _on_before_finalize is not None */
    /* per-message (segment) state */
    char *message_id;             /* NULL = None; "" is NOT None */
    double message_created_ts;
    bool message_created_ts_set;  /* false = None */
    char *accumulated;
    char *stream_ledger;
    char *redacted_prefix;
    char *settled_for;
    char *last_sent_text;
    bool fallback_final_send;
    char *fallback_prefix;
    bool fallback_preserve_partial_messages;
    bool preview_uncertain;
    scf_strlist_t tool_progress_lines;
    bool tool_progress_active;
    scf_strlist_t delivered_commentary_texts;
    scf_strlist_t delivered_segment_texts;
    /* _last_segment_delivery: (id, ts|None, text, set(ids)) */
    bool last_segment_delivery_set;
    char *lsd_id;
    double lsd_ts;
    bool lsd_ts_set;
    char *lsd_text;
    scf_strlist_t lsd_ids;
    /* turn-final flags */
    bool final_response_sent;
    bool final_content_delivered;
    char *delivered_final_text;   /* NULL = None */
    bool turn_split_delivery;
    bool delivery_ambiguous;
    bool delivery_uncertain;
    /* delivery bookkeeping */
    bool already_sent;
    bool edit_supported;
    double last_edit_time;
    bool last_edit_overflowed;
    int flood_strikes;
    double current_edit_interval;
    scf_strlist_t preview_message_ids;          /* sorted set */
    scf_strlist_t segment_preview_message_ids;  /* sorted set */
    scf_strlist_t superseded_reply_ids;         /* sorted set */
    bool before_finalize_notified;
    /* transports */
    bool use_draft_streaming;
    long long draft_id;
    bool draft_id_set;            /* false = None */
    bool use_native_streaming;
    bool native_stream_opened;
    int native_last_pushed_len;
    /* boundary state */
    char *boundary_placeholder;
    char *boundary_reason;
    bool boundary_reopen;
    bool awaiting_reopen_after_boundary;
    bool reopen_seeded_eagerly;
    /* ceiling mixin flag read by the filter_accumulate seam effect */
    bool ceiling_hit;
    /* length budget (resolved by _run_loop; injected per case) */
    int safe_limit;
    /* class-wide draft counter (GatewayStreamConsumer._draft_id_counter) */
    long long draft_id_counter;
    /* queue (intake flows) */
    scf_queue_t queue;
    /* exception propagation: NULL = no pending exception; else the Python
     * exception type name ("RuntimeError", "IndexError"). Set by a raising
     * seam or the empty-list pop; cleared by contextlib.suppress ports. */
    const char *raised;
} scf_state_t;

/* Defaults mirror __init__ + _reset_message_state + _clear_turn_final_flags
 * (cfg defaults: 0.8 / 24 / " ▉"; safe_limit injected, default 4000). */
void scf_state_init(scf_state_t *st);
void scf_state_free(scf_state_t *st);
/* JSON snapshot of the fixed state-key set (caller frees). Sets serialize
 * as sorted lists; _last_segment_delivery as [id, ts|null, text, sorted ids]. */
json_t *scf_state_json(const scf_state_t *st);

/* ── seams (one recorded event per call; scripted results) ──────────────── */
typedef struct scf_seams {
    void *ctx;
    /* time.monotonic() */
    double (*clock)(void *ctx);
    /* _send_or_edit(text, *, finalize, is_turn_final, redacted) -> bool.
     * *raised = true models a scripted exception (event recorded first). */
    bool (*send_or_edit)(void *ctx, const char *text, bool finalize,
                         bool is_turn_final, bool redacted, bool *raised);
    /* _send_new_chunk(text, reply_to_id, *, final, first) -> str|None.
     * Returns malloc'd id or NULL; caller frees. */
    char *(*send_new_chunk)(void *ctx, const char *text, const char *reply_to,
                            bool final, bool first);
    /* _send_fallback_final(text) / _send_commentary(text) */
    void (*send_fallback_final)(void *ctx, const char *text);
    void (*send_commentary)(void *ctx, const char *text);
    /* _flush_think_buffer() */
    void (*flush_think_buffer)(void *ctx);
    /* _filter_and_accumulate(text): records, then applies the scripted
     * append effect (clear overlay; accumulated += text unless ceiling_hit;
     * stream_ledger += text) — identical effect in the oracle stub. */
    void (*filter_accumulate)(void *ctx, const char *text);
    /* ceiling mixin */
    bool (*ceiling_adopt_final)(void *ctx, const char *final_raw);
    int (*ceiling_split_first_send)(void *ctx, const scf_tick_t *tick); /* 0=None 1=True 2=False */
    bool (*ceiling_final_cut)(void *ctx);
    bool (*ceiling_seal_cut)(void *ctx, int split_at, bool final);
    bool (*ceiling_finish)(void *ctx, const scf_tick_t *tick);
    void (*ceiling_segment_end)(void *ctx, const scf_tick_t *tick, bool has_tick);
    bool (*ceiling_cancelled)(void *ctx);
    void (*ceiling_reset)(void *ctx);
    /* _turn_reset() -> bool */
    bool (*turn_reset)(void *ctx);
    /* per-chat length function (default: codepoint count) */
    int (*len_fn)(void *ctx, const char *text);
    /* redact_settled(text, prefix, done=done) -> (new_text, settled).
     * *new_text is malloc'd (caller frees). */
    void (*redact_settled)(void *ctx, const char *text, const char *prefix,
                           bool done, char **new_text, int *settled);
    /* _custom_unit_to_cp(text, limit, len_fn) -> int */
    int (*custom_unit_to_cp)(void *ctx, const char *text, int limit);
    /* display-normalization leaves (scripted per input; default identity).
     * Return malloc'd strings; caller frees. */
    char *(*display_payload)(void *ctx, const char *text);
    char *(*clean_for_display)(void *ctx, const char *text);
    char *(*display_text)(void *ctx, const char *text);
    char *(*compose_frame_content)(void *ctx, const char *text);
    /* _stream_is_message() probe result */
    bool (*stream_is_message)(void *ctx);
    /* module-level predicates (scripted per input; default false) */
    bool (*is_intentional_silence_response)(void *ctx, const char *text);
    bool (*is_partial_silence_marker)(void *ctx, const char *text);
    bool (*ends_with_partial_silence_marker)(void *ctx, const char *text);
    bool (*has_pending_media_directive)(void *ctx, const char *text);
    /* _stale_preview_ids(segment_only=...) -> set (sorted list out) */
    void (*stale_preview_ids)(void *ctx, bool segment_only, scf_strlist_t *out);
    /* _delete_previews(ids, skip=..., label=...) — ids sorted */
    void (*delete_previews)(void *ctx, const scf_strlist_t *ids,
                            const char *skip, const char *label);
    /* transport lifecycle seams */
    void (*abandon_native_stream)(void *ctx);
    void (*close_empty_native_bubble)(void *ctx, const char *fail_log);
    void (*flush_segment_tail_on_edit_failure)(void *ctx);
    bool (*should_push_draft)(void *ctx);
    /* _on_before_finalize hook: 0=ok 1=raise(swallowed) 2=awaitable */
    int (*before_finalize_hook)(void *ctx);
    void (*hook_await)(void *ctx);
    /* threading.Event.set(): returns true when the scripted set raised
     * (swallowed by _signal_flush). */
    bool (*flush_event_set)(void *ctx, int event_id);
    /* chunk splitters (scripted lists; default [text, ""]) */
    void (*truncate_for_stream)(void *ctx, const char *text, int limit,
                                scf_strlist_t *out);
    void (*split_text_chunks)(void *ctx, const char *text, int limit,
                              scf_strlist_t *out);
} scf_seams_t;

/* ── UTF-8 / Python str helpers (codepoint semantics) ───────────────────── */
int scf_utf8_len(const char *s);                  /* len(str) */
int scf_utf8_byte_offset(const char *s, int cp);  /* clamp to [0, len] */
char *scf_utf8_slice(const char *s, int start, int end); /* end<0 = to end */
bool scf_utf8_endswith(const char *s, const char *suffix);
char *scf_utf8_removesuffix(const char *s, const char *suffix);
char *scf_utf8_strip(const char *s);              /* str.strip() */
char *scf_utf8_lstrip_newlines(const char *s);    /* lstrip("\n") */
int scf_utf8_rfind_newline(const char *s, int end_cp); /* str.rfind("\n",0,end) */

/* ── internal helpers (class-defined collaborators of the 16 flows; the
 * claimed PoP ports of _reset_segment_state/_notify_before_finalize/
 * _signal_flush live in stream_consumer.c — these are flow-local static
 * mirrors, not re-ports; no global symbols are exported for them) ──────── */

/* ── the 16 ports ─────────────────────────────────────────────────────────
 * Raise propagation: a scripted seam exception or the empty-list pop sets
 * st->raised; the flow returns immediately (zero value) and the harness
 * emits ret = {"__raise__": <name>}, matching the live exception unwind. */

/* PoP: scf_drain_queue_flow @ gateway/stream_consumer.py:_drain_queue */
scf_tick_t scf_drain_queue_flow(scf_state_t *st, const scf_seams_t *s);

/* PoP: scf_should_edit_flow @ gateway/stream_consumer.py:_should_edit */
bool scf_should_edit_flow(scf_state_t *st, const scf_tick_t *tick,
                          const scf_seams_t *s);

/* PoP: scf_tick_ends_text @ gateway/stream_consumer.py:_tick_ends_text */
bool scf_tick_ends_text(scf_state_t *st, const scf_tick_t *tick,
                        const scf_seams_t *s);

/* PoP: scf_redact_settled_text @ gateway/stream_consumer.py:_redact_settled_text */
int scf_redact_settled_text(scf_state_t *st, bool done, const scf_seams_t *s);

/* PoP: scf_push_update_flow @ gateway/stream_consumer.py:_push_update */
void scf_push_update_flow(scf_state_t *st, scf_tick_t *tick,
                          const scf_seams_t *s);

/* PoP: scf_seal_overflow_heads_flow @ gateway/stream_consumer.py:_seal_overflow_heads */
bool scf_seal_overflow_heads_flow(scf_state_t *st, bool final, bool done,
                                  const scf_seams_t *s);

/* PoP: scf_split_first_send_flow @ gateway/stream_consumer.py:_split_first_send */
bool scf_split_first_send_flow(scf_state_t *st, scf_tick_t *tick,
                               const scf_seams_t *s);

/* PoP: scf_finalize_edit_flow @ gateway/stream_consumer.py:_finalize_edit */
bool scf_finalize_edit_flow(scf_state_t *st, const char *text, bool record,
                            const scf_seams_t *s);

/* PoP: scf_finalize_edit_path_flow @ gateway/stream_consumer.py:_finalize_edit_path */
void scf_finalize_edit_path_flow(scf_state_t *st, const scf_tick_t *tick,
                                 const scf_seams_t *s);

/* PoP: scf_finalize_turn_flow @ gateway/stream_consumer.py:_finalize_turn */
void scf_finalize_turn_flow(scf_state_t *st, scf_tick_t *tick,
                            const scf_seams_t *s);

/* PoP: scf_end_segment_flow @ gateway/stream_consumer.py:_end_segment */
void scf_end_segment_flow(scf_state_t *st, scf_tick_t *tick,
                          const scf_seams_t *s);

/* PoP: scf_on_cancelled_flow @ gateway/stream_consumer.py:_on_cancelled */
void scf_on_cancelled_flow(scf_state_t *st, const scf_seams_t *s);

/* PoP: scf_adopt_final_text_flow @ gateway/stream_consumer.py:_adopt_final_text */
void scf_adopt_final_text_flow(scf_state_t *st, const char *final_raw,
                               const scf_seams_t *s);

/* PoP: scf_adopt_recovered_reply @ gateway/stream_consumer.py:_adopt_recovered_reply */
bool scf_adopt_recovered_reply(scf_state_t *st, const char *final_raw,
                               const scf_seams_t *s);

/* PoP: scf_wake_flush_waiters @ gateway/stream_consumer.py:_wake_flush_waiters */
void scf_wake_flush_waiters(scf_state_t *st, const scf_seams_t *s);

/* PoP: scf_mark_skip_redundant_finalize @ gateway/stream_consumer.py:_mark_skip_redundant_finalize */
void scf_mark_skip_redundant_finalize(scf_state_t *st, const scf_seams_t *s);

#ifdef __cplusplus
}
#endif

#endif /* PORT_STREAM_CONSUMER_FLOW_H */