/*
 * relay_adapter_flow.h — relay routing/forwarding FLOW cores of
 * gateway/relay/adapter.py (LIVE authority).
 *
 * Ports the DECISION sequences (call order, branch conditions, early returns,
 * exception->fallback paths, mutation order) of the RelayAdapter egress lanes.
 * All live I/O (transport.send_outbound, descriptor negotiation, transport
 * redial toggles, logging, the session-key namespace resolver) is injected
 * through seams: callback structs declared per flow below. The decision cores
 * perform NO I/O themselves.
 *
 * Sequences are observable through a radapter_trace_t: seam implementations
 * record one JSON event per call (arguments + scripted result) in call order;
 * the oracle compares that ordered event list against recording monkeypatch
 * stubs driving the LIVE Python with the same scripted results.
 *
 * Adapter state is a JSON model (all maps are chat_id -> string unless noted):
 *   transport            null (no transport) or object; method attributes are
 *                        modeled as {"callable": bool, ...} markers — any
 *                        other JSON value reads as a non-callable attribute,
 *                        an absent key as a missing attribute (getattr None).
 *   descriptor           {platform, supported_ops (array|null), ...}
 *   platform_by_chat     chat -> underlying platform string
 *   chat_type_by_chat    chat -> "dm" / "channel" / ...
 *   scope_by_chat        chat -> scope_id
 *   dm_user_by_chat      chat -> user_id
 *   profile_by_chat      chat -> profile
 *   last_inbound_ts_by_chat  chat -> last triggering message ts
 *   config               {"extra": ...} relay platform config extra
 *
 * JSON values are libjson json_t. Ownership is documented per function.
 */

#ifndef RELAY_ADAPTER_FLOW_H
#define RELAY_ADAPTER_FLOW_H

#include <stdbool.h>
#include <stddef.h>

#include "libjson/json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Trace recorder (harness infrastructure, no Python counterpart) ─────────
 * events is a JSON_ARRAY of event objects. radapter_flow_trace_add takes
 * ownership of ev (frees it when t is NULL). The trace never leaves the
 * caller. */
typedef struct radapter_trace {
    json_t *events;   /* owned JSON_ARRAY; NULL until first add */
} radapter_trace_t;

void radapter_flow_trace_add(radapter_trace_t *t, json_t *ev);
void radapter_flow_trace_free(radapter_trace_t *t);

/* ── 1. Telegram length unit: UTF-16 code units ─────────────────────────────
 * Pure; text is UTF-8 (Python str). Astral code points count 2. */
/* PoP: radapter_flow_utf16_len @ gateway/relay/adapter.py:_utf16_len */
int radapter_flow_utf16_len(const char *text);

/* ── 2. Connector outbound_result -> SendResult projection ──────────────────
 * extra carries the **extra kwargs (typically {"raw_response": result}).
 * Returns a NEW object with the full SendResult field set (dataclass defaults
 * included); caller frees. result/extra borrowed. */
/* PoP: radapter_flow_send_result @ gateway/relay/adapter.py:_send_result */
json_t *radapter_flow_send_result(const json_t *result, const json_t *extra);

/* ── 3. Inbound event identity: (message_id, chat_id) ───────────────────────
 * event is a JSON model {message_id?, source: {message_id?, chat_id?} | null}.
 * message_id prefers the event's own, falling back to source on FALSY (or).
 * Returns a NEW 2-element array [message_id, chat_id]; caller frees. */
/* PoP: radapter_flow_event_ids @ gateway/relay/adapter.py:_event_ids */
json_t *radapter_flow_event_ids(const json_t *event);

/* ── 4. Profile namespace encoded in an agent:<ns>:... session key ──────────
 * Seam resolve_namespace stands in for the function-local
 * `from gateway.session import profile_from_session_key_namespace` import;
 * returns the borrowed resolved profile string (or NULL). The flow maps a
 * "default" resolution to NULL (legacy single-profile wire parity).
 * Returns malloc'd profile text (caller frees) or NULL. */
typedef struct radapter_profile_seams {
    void *ctx;
    const char *(*resolve_namespace)(void *ctx, const char *ns);
} radapter_profile_seams_t;

/* PoP: radapter_flow_profile_from_session_key @ gateway/relay/adapter.py:_profile_from_session_key */
char *radapter_flow_profile_from_session_key(const char *session_key,
                                             const radapter_profile_seams_t *s);

/* ── 5. Per-platform negotiated descriptor lookup (never raises) ────────────
 * Seam resolve stands in for transport.descriptor_for_platform(platform);
 * returns the borrowed descriptor (or NULL) and sets *raised on exception.
 * Called ONLY when platform is truthy AND the transport attribute exists AND
 * is callable ({"callable": true} marker). Returns a NEW copy or NULL. */
typedef struct radapter_desc_seams {
    void *ctx;
    const json_t *(*resolve)(void *ctx, const char *platform, bool *raised);
} radapter_desc_seams_t;

/* PoP: radapter_flow_negotiated_descriptor @ gateway/relay/adapter.py:_negotiated_descriptor */
json_t *radapter_flow_negotiated_descriptor(const json_t *state,
                                            const char *platform,
                                            const radapter_desc_seams_t *s);

/* ── 6. Metrics platform label for a chat ───────────────────────────────────
 * Pure over state: the inbound's platform, else the primary's ONLY when the
 * transport fronts <= 1 distinct platform (transport._identities pairs).
 * Returns a NEW json string or NULL. chat_id borrowed (stringified). */
/* PoP: radapter_flow_metrics_platform @ gateway/relay/adapter.py:_metrics_platform */
json_t *radapter_flow_metrics_platform(const json_t *state, const char *chat_id);

/* ── 7. Slack-only unfurl hints from the relay config namespace ─────────────
 * Pure over state.config.extra. Only explicitly configured booleans survive
 * (YAML strings coerced via the _TRUTHY/_FALSY sets, junk dropped).
 * Returns a NEW object {unfurl_links?, unfurl_media?} or NULL (non-Slack or
 * no hints). platform borrowed (NULL = None). */
/* PoP: radapter_flow_slack_unfurl_hints @ gateway/relay/adapter.py:_slack_unfurl_hints */
json_t *radapter_flow_slack_unfurl_hints(const json_t *state, const char *platform);

/* ── 8. Fresh-final streaming decision (Slack unfurl forced on) ─────────────
 * Pure over state + args. Platform resolution: chat cache (when chat_id is
 * not None) -> metadata["platform"] (when metadata IS a dict) -> descriptor.
 * True only when hints carry an explicit True AND content bears a link
 * (_URL_RE: raw URL, Slack mrkdwn <https?:, markdown ](https?:).
 * content NULL = None; chat_id NULL = None (absent kwarg). */
/* PoP: radapter_flow_prefers_fresh_final_streaming @ gateway/relay/adapter.py:prefers_fresh_final_streaming */
bool radapter_flow_prefers_fresh_final_streaming(const json_t *state,
                                                 const char *content,
                                                 const json_t *metadata,
                                                 const json_t *chat_id);

/* ── 9/10. Outbound frame seams (shared by _gated_op / _outbound / typing) ──
 * send_outbound stands in for transport.send_outbound(action, platform=...);
 * returns the borrowed scripted result (ANY JSON type — the live contract
 * allows non-dict results on the _outbound lane) and sets *raised when the
 * transport raised. log_debug/log_level/log_warning receive PRE-FORMATTED
 * messages (the live logger %-formatting happens inside the flow). */
typedef struct radapter_op_seams {
    void *ctx;
    const json_t *(*send_outbound)(void *ctx, const json_t *action,
                                   const char *platform, bool *raised);
    void (*log_debug)(void *ctx, const char *msg);
    void (*log_level)(void *ctx, int level, const char *msg);
    void (*log_warning)(void *ctx, const char *msg);
} radapter_op_seams_t;

/* ── 9. Op-gated best-effort frame (_gated_op) ──────────────────────────────
 * decline_level: -1 = None (silent), else the logging level (20/30/...).
 * subject: NULL = None (falls back to chat_id in decline logs).
 * platform: NULL/"" = fall back to the chat's inbound platform (live `or`).
 * Returns a NEW copy of the connector result, or NULL when the caller must
 * fall back (op unadvertised, no transport, transport raised, decline without
 * surface_declines, non-ambiguous failure, lost ack without surface_ambiguous).
 * action borrowed; MUST carry "op". */
/* PoP: radapter_flow_gated_op @ gateway/relay/adapter.py:_gated_op */
json_t *radapter_flow_gated_op(const json_t *state, const char *chat_id,
                               const json_t *action, int decline_level,
                               const json_t *subject, const char *platform,
                               bool surface_declines, bool surface_ambiguous,
                               const radapter_op_seams_t *s);

/* ── 10. Second frame path (_outbound): bool/None-contract lanes ────────────
 * No op gate and no transport guard: a NULL transport raises AttributeError
 * live — modeled as *raised=true with a NULL return. A dict result that is a
 * definite egress decline is logged at WARNING (log_decline) but RETURNED
 * verbatim (the refusal is recorded, the contract unchanged).
 * Returns a NEW copy of the result (any JSON type). */
/* PoP: radapter_flow_outbound @ gateway/relay/adapter.py:_outbound */
json_t *radapter_flow_outbound(const json_t *state, const char *chat_id,
                               const json_t *action, bool *raised,
                               const radapter_op_seams_t *s);

/* ── 11. Reconnect-supervisor redial toggle (hold/release) ──────────────────
 * Seam call stands in for getattr(transport, method_name)(); sets *raised on
 * exception. Called only when the attribute exists and is callable.
 * Never raises into the idle path; False on any failure. */
typedef struct radapter_toggle_seams {
    void *ctx;
    void (*call)(void *ctx, const char *method_name, bool *raised);
    void (*log_debug)(void *ctx, const char *msg);
} radapter_toggle_seams_t;

/* PoP: radapter_flow_toggle_transport_redial @ gateway/relay/adapter.py:_toggle_transport_redial */
bool radapter_flow_toggle_transport_redial(const json_t *state,
                                           const char *method_name,
                                           const radapter_toggle_seams_t *s);

/* ── 12. Ack reaction vocabulary for the event's lane ───────────────────────
 * Candidate chain: event.source.platform -> platform_by_chat[chat_id] ->
 * descriptor.platform; each may be the unresolved placeholder ""/"relay".
 * Enum members modeled as {"value": ...}. Returns a NEW 3-element array
 * [in_progress, success, failure]; caller frees. */
/* PoP: radapter_flow_ack_emoji @ gateway/relay/adapter.py:_ack_emoji */
json_t *radapter_flow_ack_emoji(const json_t *state, const json_t *event,
                                const char *chat_id);

/* ── 13. Destination format-hints stamping ──────────────────────────────────
 * Pure over state.config.extra + the DESTINATION descriptor/platform.
 * Hints stamped only when the descriptor advertises supports_block_formatting
 * AND the operator enabled rich_blocks/markdown_blocks (_coerce_flag).
 * setdefault: an existing format_hints key is never overwritten.
 * Returns a NEW object (copy of metadata, possibly stamped), or NULL when
 * metadata is NULL and no hints apply (live returns metadata verbatim). */
/* PoP: radapter_flow_stamp_format_hints @ gateway/relay/adapter.py:_stamp_format_hints */
json_t *radapter_flow_stamp_format_hints(const json_t *state,
                                         const json_t *descriptor,
                                         const char *platform,
                                         const json_t *metadata);

/* ── 14. One typing/status frame (cosmetic, never raises) ───────────────────
 * Full chain: status thread anchor (Slack DM) -> scope discriminators ->
 * frame construction (content key ONLY when content is not None; "" is the
 * Slack clear) -> _outbound (decline recording) with exception->debug log.
 * content: NULL or JSON_NULL = None (omit); JSON_STRING = present.
 * Observable ONLY through the trace (live returns None). */
/* PoP: radapter_flow_typing_frame @ gateway/relay/adapter.py:_typing_frame */
void radapter_flow_typing_frame(const json_t *state, const char *chat_id,
                                const json_t *metadata, const json_t *content,
                                const char *lane, const radapter_op_seams_t *s);

#ifdef __cplusplus
}
#endif

#endif /* RELAY_ADAPTER_FLOW_H */