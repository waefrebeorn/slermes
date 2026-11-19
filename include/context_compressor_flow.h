/* context_compressor_flow.h — FLOW-core port of agent/context_compressor.py.
 *
 * Mission: match the program's FLOW (decision sequences, call order, branch
 * conditions, state transitions, error paths) of the compaction orchestration.
 * I/O is INJECTED through seams (a generic callback dispatcher + a json_t state
 * object), never performed inside the ported decision core.
 *
 * Seam model
 * ----------
 * Every live `self.<method>(...)` / module-level I/O call that the ported flow
 * makes goes through `cc_flow_io_t.call(io, name, args)`. The dispatcher:
 *   - appends `name` to `io->trace` (an ordered JSON array of strings) so the
 *     decision SEQUENCE can be compared against live Python under scripted I/O;
 *   - returns the scripted result (a NEW json_t* the caller frees);
 *   - applies any "__state__" object in the scripted result onto `io->state`
 *     (mirrors a live method's attribute mutations);
 *   - models a raised exception with {"__raise__": "<Type>", "message": "..."}.
 * `io->state` is a json_t OBJECT mirroring the compressor's attribute dict; the
 * flow reads/writes attributes through the cc_state_* accessors below (Python
 * getattr/setattr semantics, including the falsy set None/0/""/[]/{}/False).
 *
 * Ownership: seam results returned by `call` are owned by the caller (json_free).
 * `io->trace` and `io->state` are owned by the harness/caller. Argument json_t*
 * passed to `call` are borrowed (the dispatcher copies what it records).
 */
#ifndef CONTEXT_COMPRESSOR_FLOW_H
#define CONTEXT_COMPRESSOR_FLOW_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Generic seam dispatcher + trace + attribute state ─────────────────── */
typedef struct cc_flow_io_t {
    /* Dispatch a live call by name. Returns a NEW json_t* (caller frees).
     * A result object carrying "__raise__" models a raised exception. */
    json_t *(*call)(struct cc_flow_io_t *io, const char *name, json_t *args);
    json_t *trace;   /* JSON array of strings: ordered decision/action sequence */
    json_t *state;   /* JSON object: compressor attribute dict */
    void   *ctx;     /* harness backend (scripted results) */
} cc_flow_io_t;

/* Append a decision/action token to the trace (no-op when trace is NULL). */
void cc_flow_trace(cc_flow_io_t *io, const char *token);
/* Dispatch a seam call (NULL io->call returns json_null()). */
json_t *cc_flow_call(cc_flow_io_t *io, const char *name, json_t *args);
/* True when a seam result models a raised exception. */
bool cc_flow_raised(const json_t *result);

/* ── Python-faithful attribute accessors on the json_t state object ────── */
bool        cc_state_truthy(const json_t *v);          /* Python bool(v) */
const char *cc_state_str(const json_t *state, const char *key, const char *def);
double      cc_state_num(const json_t *state, const char *key, double def);
bool        cc_state_bool(const json_t *state, const char *key, bool def);
json_t     *cc_state_get(const json_t *state, const char *key);  /* borrowed, NULL=absent */
void        cc_state_set(json_t *state, const char *key, json_t *val); /* takes ownership */
void        cc_state_set_str(json_t *state, const char *key, const char *val);
void        cc_state_set_num(json_t *state, const char *key, double val);
void        cc_state_set_bool(json_t *state, const char *key, bool val);
void        cc_state_set_null(json_t *state, const char *key);
/* getattr(self, key, default) where default may be a string/bool/number. */
bool        cc_state_has(const json_t *state, const char *key);

/* ── Pinned summary route (ContextVar single-use pin) ──────────────────── */
/* The pin lives in a one-field state object {"route": <dict|null>}. */
/* PoP prototypes are tagged at their definitions in the .c file. */
json_t *cc_flow_take_pinned_summary_route(json_t *pin);          /* NEW dict|null */
bool    cc_flow_take_deterministic_summary_pin(json_t *pin);
json_t *cc_flow_pinned_summary_call_kwargs(json_t *pin);         /* NEW dict */
void    cc_flow_pin_summary_route_set(json_t *pin, json_t *route); /* copies route; NULL=no-op pin */

/* ── Failure classification ────────────────────────────────────────────── */
/* Exception descriptor (injected shape of a live Exception). */
typedef struct {
    const char *type;        /* "RuntimeError","TimeoutError","JSONDecodeError", else other */
    const char *message;     /* str(e) */
    int         status_code; /* _exc_status_code(e); -1 == None */
    bool        has_status;  /* whether status_code is present (None vs value) */
    const char *code;        /* getattr(e,"code",None); NULL == None */
    bool        unscoped_secret; /* isinstance(e, UnscopedSecretError) */
} cc_flow_exc_t;

/* Injected classify_api_error verdict. reason is the FailoverReason member
 * NAME (e.g. "rate_limit","auth","auth_permanent","overloaded",
 * "ssl_cert_verification") or NULL; transport mirrors error_context["transport"]. */
typedef struct {
    const char *reason;
    bool        transport;
} cc_flow_verdict_t;

/* _SummaryFailureKind: 7 flags + ordered fallback_reason(). Returns NEW object
 * {model_not_found,timeout,json_decode,streaming_closed,empty_content,truncated,
 *  overloaded, fallback_reason}. */
json_t *cc_flow_classify_summary_failure(const cc_flow_exc_t *e, const cc_flow_verdict_t *verdict);
/* _is_summary_access_or_quota_error(e, verdict). */
bool cc_flow_is_summary_access_or_quota_error(const cc_flow_exc_t *e, const cc_flow_verdict_t *verdict);
/* _short_error_text(e, limit=220). Caller frees. */
char *cc_flow_short_error_text(const char *str_e, const char *class_name, int limit);
/* _next_timeout_cooldown(compressor, counter): bump state[counter], return rung. */
int cc_flow_next_timeout_cooldown(json_t *state, const char *counter);

/* ── Trigger / gate decisions ──────────────────────────────────────────── */
/* _compression_block_reason(ignore_cooldown=False). now = injected time.monotonic().
 * Returns NEW string or json_null(). */
json_t *cc_flow_compression_block_reason(const json_t *state, double now, bool ignore_cooldown);
/* should_compress_info(prompt_tokens=None). Uses io for the blocked-gate seam.
 * now/wall are injected clocks (time.monotonic()/time.time()).
 * Returns NEW {should: bool, reason: string|null}. */
json_t *cc_flow_should_compress_info(cc_flow_io_t *io, const json_t *state,
                                     bool has_prompt_tokens, long prompt_tokens,
                                     double now, double wall);
/* _automatic_compression_blocked_locally(ignore_cooldown=False). now=monotonic,
 * wall=time.time(). Mutates state via seams; returns bool. */
bool cc_flow_automatic_compression_blocked_locally(cc_flow_io_t *io, json_t *state,
                                                   double now, double wall, bool ignore_cooldown);
/* _automatic_compression_blocked(ignore_cooldown=False). */
bool cc_flow_automatic_compression_blocked(cc_flow_io_t *io, json_t *state,
                                           double now, double wall, bool ignore_cooldown);

/* ── Tail token-budget walk ────────────────────────────────────────────── */
/* _walk_tail_budget(messages, head_end, ceiling, min_tail, cut_at_break, price).
 * price is a seam: io->call(io,"price",{"i":idx}) -> number tokens. When
 * use_price_seam is false, prices[] (len n) is used directly (injected pricer).
 * Returns NEW {cut: int, accumulated: int}. */
json_t *cc_flow_walk_tail_budget(cc_flow_io_t *io, const json_t *messages,
                                 long head_end, long ceiling, long min_tail,
                                 bool cut_at_break, const long *prices, bool use_price_seam);

/* ── Summary placement (template-visible alternation) ──────────────────── */
/* _summary_placement(compressed, tail_messages, compress_start).
 * Returns NEW {summary_role, merge_into_tail, force_user_leading,
 *              first_tail_visible_idx (int|null)}. */
json_t *cc_flow_summary_placement(const json_t *compressed, const json_t *tail_messages,
                                  long compress_start);

/* ── Summary failure / retry / fallback flows (seam-driven) ────────────── */
/* _fallback_to_main_for_compression(e, reason, failed_model=None). Mutates state. */
void cc_flow_fallback_to_main_for_compression(cc_flow_io_t *io, json_t *state,
                                              const cc_flow_exc_t *e, const char *reason,
                                              const char *failed_model);
/* _on_summary_failure(e, turns, focus_topic, memory_context). Returns NEW
 * string (retry summary) or json_null(). */
json_t *cc_flow_on_summary_failure(cc_flow_io_t *io, json_t *state,
                                   const cc_flow_exc_t *e, const cc_flow_verdict_t *verdict,
                                   const json_t *turns, const char *focus_topic,
                                   const char *memory_context);
/* _generate_summary(turns, focus_topic=None, memory_context="", bypass_cooldown=False).
 * Returns NEW string, json_null(), or a raise object. */
json_t *cc_flow_generate_summary(cc_flow_io_t *io, json_t *state,
                                 const json_t *turns, const char *focus_topic,
                                 const char *memory_context, bool bypass_cooldown,
                                 double now);
/* compress(messages, current_tokens=None, focus_topic=None, force=False,
 * memory_context="", bypass_cooldown=False). Returns NEW messages array or a
 * raise object. */
json_t *cc_flow_compress(cc_flow_io_t *io, json_t *state,
                         const json_t *messages, bool has_current_tokens, long current_tokens,
                         const char *focus_topic, bool force, const char *memory_context,
                         bool bypass_cooldown);

#ifdef __cplusplus
}
#endif
#endif /* CONTEXT_COMPRESSOR_FLOW_H */