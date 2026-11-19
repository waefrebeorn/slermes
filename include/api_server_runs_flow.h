/*
 * api_server_runs_flow.h — run-lifecycle FLOW cores of gateway/platforms/api_server_runs.py.
 *
 * Ports the DECISION sequences of the durable /v1/runs surface: terminal-status
 * mapping (terminal_run_status), input admission (_run_user_message), the SSE
 * event envelope (_run_event) and tool-preview redaction (_tool_completed_preview),
 * room-grant auth fences (_room_permission_for / room_run_scope /
 * _remember_room_retention / _run_idempotency_scope / _check_run_auth), owner
 * liveness (_owner_alive), pollable status writes (_set_run_status /
 * _mark_shutdown_requested), ownership teardown (_forget_run /
 * _release_run_owner_if_forgotten), durable hydration (_durable_run_status /
 * _replay_or_conflict), history resolution (_resolve_conversation_history) and
 * the 202 admission shape (_accepted_response). All live I/O (HTTP response
 * emission, clocks, logging, redaction, room-grant decode, process probes,
 * idempotency-store reads/writes, response-store reads, request attribute
 * mechanics) enters through the asrun_seams_t callbacks below; the decision
 * cores perform NO I/O. Each seam documents its live counterpart.
 *
 * Sequences are observable through an asrun_trace_t: seam implementations
 * record one JSON event per call (arguments + scripted result) in call order;
 * tests/oracle_api_server_runs_flow.py compares that ordered event list against
 * recording monkeypatch stubs driving the LIVE Python with the same scripts.
 *
 * web.Response is modeled as a JSON descriptor {"status", "body", "headers"}
 * produced by the json_response seam (live: web.json_response). The adapter
 * state is a JSON object (asrun_state_new / asrun_state_out); sets are string
 * arrays sorted on output (Python set order is not observable). The request
 * double is {"items": {...}, "attr": null} — attr holds the fallback
 * _hermes_room_run_retention_until attribute.
 *
 * Already-ported siblings are NOT re-ported here: _room_retention_until enters
 * as a seam (claimed by api_server_runs_gaps.c); _handle_get_run /
 * _handle_run_events / _handle_run_approval / _handle_stop_run stay with
 * api_server_adapter_runs.c.
 *
 * JSON values are libjson json_t. Ownership is documented per function.
 */

#ifndef API_SERVER_RUNS_FLOW_H
#define API_SERVER_RUNS_FLOW_H

#include <stdbool.h>
#include <stddef.h>

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Trace recorder (harness infrastructure, no Python counterpart) ─────────
 * events is a JSON_ARRAY of event objects. asrun_trace_add takes ownership of
 * ev (frees it when t is NULL). */
typedef struct asrun_trace {
    json_t *events;   /* owned JSON_ARRAY; NULL until first add */
} asrun_trace_t;

void asrun_trace_add(asrun_trace_t *t, json_t *ev);
void asrun_trace_free(asrun_trace_t *t);

/* ── Seams ──────────────────────────────────────────────────────────────────
 * One shared seam table; each flow calls exactly the members the live code
 * path reaches. NULL members are never called on a scripted path that does
 * not reach them. ctx is the harness/oracle script context.
 *
 * Member -> live counterpart:
 *   json_response          -> aiohttp web.json_response(body, status=, headers=)
 *                             returns a NEW response descriptor (caller frees);
 *                             body/headers borrowed, headers NULL == not passed
 *   openai_error           -> _api_server._openai_error(message, **kwargs):
 *                             deterministic envelope {"message": message, **kwargs}
 *   log                    -> module logger.debug/warning/exception (pre-formatted)
 *   clock                  -> time.time()
 *   redact                 -> redact_sensitive_text(text, force=True) (malloc'd)
 *   room_grant_token       -> adapter._room_grant_token(request) (borrowed or NULL)
 *   room_run_claims        -> api_server_room_grants.room_run_claims(self, request,
 *                             permission=) (NEW claims; *raised + *exc_type +
 *                             *exc_msg on exception)
 *   expected_api_key       -> adapter._expected_api_key() (borrowed or NULL)
 *   request_profile        -> _api_server._api_request_profile.get() (borrowed/NULL)
 *   request_set            -> request[key] = value on the request double; returns
 *                             false + *exc_type on the scripted AttributeError /
 *                             TypeError, else stores into req_state["items"]
 *   room_retention_until   -> api_server_runs._room_retention_until(request)
 *                             (ported elsewhere; scripted float here)
 *   check_auth             -> adapter._check_auth(request) (descriptor or NULL)
 *   pid_exists             -> gateway.status._pid_exists(pid) (*raised)
 *   process_start_time     -> gateway.status.get_process_start_time(pid)
 *                             (*raised; *is_none when the live call returns None)
 *   fingerprints_match     -> gateway.status.start_time_fingerprints_match(
 *                             recorded, current) (*raised)
 *   run_projection         -> api_server_authority_runs.run_projection(self, run_id)
 *                             (NEW canonical object or NULL)
 *   store_extend_retention -> _run_idempotency_store.extend_retention(scope,
 *                             run_id, retention_until)
 *   store_status_for_run   -> _run_idempotency_store.status_for_run(scope, run_id,
 *                             retention_until=) (NEW record or NULL)
 *   store_update_status    -> _run_idempotency_store.update_status(run_id, status)
 *                             (*raised + *exc_type + *exc_msg on exception)
 *   response_store_get     -> self._current_response_store().get(response_id)
 *                             (NEW stored record or NULL; the two live steps
 *                             collapse to one scripted read)
 */
typedef struct asrun_seams {
    void *ctx;
    json_t *(*json_response)(void *ctx, const json_t *body, int status, const json_t *headers);
    json_t *(*openai_error)(void *ctx, const char *message, const json_t *kwargs);
    void (*log)(void *ctx, const char *level, const char *msg);
    double (*clock)(void *ctx);
    char *(*redact)(void *ctx, const char *text);
    const char *(*room_grant_token)(void *ctx, const json_t *request);
    json_t *(*room_run_claims)(void *ctx, const json_t *request, const char *permission,
                               bool *raised, const char **exc_type, const char **exc_msg);
    const char *(*expected_api_key)(void *ctx);
    const char *(*request_profile)(void *ctx);
    bool (*request_set)(void *ctx, json_t *req_state, const char *key, double value,
                        const char **exc_type);
    double (*room_retention_until)(void *ctx, const json_t *request);
    json_t *(*check_auth)(void *ctx, const json_t *request);
    bool (*pid_exists)(void *ctx, long pid, bool *raised);
    double (*process_start_time)(void *ctx, long pid, bool *raised, bool *is_none);
    bool (*fingerprints_match)(void *ctx, long recorded, double current, bool *raised);
    json_t *(*run_projection)(void *ctx, const char *run_id);
    void (*store_extend_retention)(void *ctx, const char *scope, const char *run_id,
                                   double retention_until);
    json_t *(*store_status_for_run)(void *ctx, const char *scope, const char *run_id,
                                    double retention_until);
    void (*store_update_status)(void *ctx, const char *run_id, const json_t *status,
                                bool *raised, const char **exc_type, const char **exc_msg);
    json_t *(*response_store_get)(void *ctx, const char *response_id);
} asrun_seams_t;

/* ── State helpers ──────────────────────────────────────────────────────── */

/* NEW working adapter state from in["state"] (canonical keys, defaults filled).
 * Dict tables are objects; the three set tables are string arrays. */
json_t *asrun_state_new(const json_t *in_state);

/* NEW normalized output snapshot: dicts copied, set arrays sorted, missing
 * tables defaulted. Mirrors the oracle's state_out(). */
json_t *asrun_state_out(const json_t *state);

/* NEW request double {"items": {...}, "attr": null} from in["request_state"]. */
json_t *asrun_req_state_new(const json_t *in);

/* ── Terminal status / admission leaves ─────────────────────────────────── */

/* NEW fields object; *status_out = malloc'd "completed"/"failed"/"cancelled".
 * `completed is not False` is identity, not truthiness: JSON 0 stays finished. */
/* PoP: asrun_terminal_run_status @ gateway/platforms/api_server_runs.py:terminal_run_status */
json_t *asrun_terminal_run_status(const json_t *result, char **status_out);

/* NEW user-message node (string, or the raw last-content value) or NULL;
 * *err_out = NEW 400 descriptor on the two refusal branches; *exc_out =
 * malloc'd "AttributeError: '<type>' object has no attribute 'get'" when a
 * non-empty list's last element is not a dict (live raw_input[-1].get raises).
 * The .get-AttributeError type-name domain excludes integral-float literals
 * (a JSON "5.0" is a float in live json.loads but reads as int here). */
/* PoP: asrun_run_user_message @ gateway/platforms/api_server_runs.py:_run_user_message */
json_t *asrun_run_user_message(const json_t *raw_input, const asrun_seams_t *s,
                               json_t **err_out, char **exc_out);

/* malloc'd bounded redacted preview ("" for None; redact BEFORE truncation;
 * 500 code points, cut at 497 + "..."). Non-string results json.dumps with
 * default separators, ensure_ascii=False; integral JSON numbers dump as ints
 * (json_t carries no int/float identity — a JSON 1.0 literal reads back as 1). */
/* PoP: asrun_tool_completed_preview @ gateway/platforms/api_server_runs.py:_tool_completed_preview */
char *asrun_tool_completed_preview(const json_t *result, const asrun_seams_t *s);

/* NEW SSE envelope {"event", "run_id", "timestamp", **fields} — key order is
 * wire format; fields may override the envelope keys. *exc_out = malloc'd
 * "TypeError: _run_event() got multiple values for argument '<key>'" when a
 * fields key collides with a positional parameter (first colliding key in
 * fields order); the clock seam is NOT reached on that branch. */
/* PoP: asrun_run_event @ gateway/platforms/api_server_runs.py:_run_event */
json_t *asrun_run_event(const char *run_id, const char *name, const json_t *fields,
                        const asrun_seams_t *s, char **exc_out);

/* NEW {"tool_call_id": kw["tool_call_id"]} when kw.get("tool_call_id") is
 * truthy, else {} (pure leaf; no seams, no reachable KeyError). */
/* PoP: asrun_call_id @ gateway/platforms/api_server_runs.py:_call_id */
json_t *asrun_call_id(const json_t *kw);

/* ── Room-grant auth fences ─────────────────────────────────────────────── */

/* Static permission string: control-suffix map in insertion order, then
 * GET -> "status" else "dispatch". request = {"path", "method"}. */
/* PoP: asrun_room_permission_for @ gateway/platforms/api_server_runs.py:_room_permission_for */
const char *asrun_room_permission_for(const json_t *request);

/* malloc'd sha256 hex of "\0".join(str(claims[k]) for the 7 scope keys);
 * *err_out = malloc'd "KeyError: '<key>'" when a key is missing. */
/* PoP: asrun_room_scope @ gateway/platforms/api_server_runs.py:room_run_scope */
char *asrun_room_scope(const json_t *claims, char **err_out);

/* float(claims status_expires_at or expires_at or 0) -> request[key] via the
 * request_set seam; AttributeError/TypeError fall back to req_state["attr"].
 * *err_out on any other scripted exception. */
/* PoP: asrun_remember_room_retention @ gateway/platforms/api_server_runs.py:_remember_room_retention */
void asrun_remember_room_retention(json_t *req_state, const json_t *claims,
                                   const asrun_seams_t *s, char **err_out);

/* malloc'd scope: room token -> claims + retention + room_run_scope; else
 * sha256("\0".join(profile or default, key or unauthenticated-test-listener)).
 * *err_out propagates claims/scope failures. */
/* PoP: asrun_run_idempotency_scope @ gateway/platforms/api_server_runs.py:_run_idempotency_scope */
char *asrun_run_idempotency_scope(const json_t *request, json_t *req_state,
                                  const asrun_seams_t *s, char **err_out);

/* NULL when authorized, else a NEW descriptor: no token -> check_auth seam;
 * token -> claims seam, exception -> the live _room_grant_error_response
 * decision (403 reauthorization vs 401 invalid) built through openai_error +
 * json_response. */
/* PoP: asrun_check_run_auth @ gateway/platforms/api_server_runs.py:_check_run_auth */
json_t *asrun_check_run_auth(const json_t *request, const char *permission, const asrun_seams_t *s);

/* ── Owner liveness / status writes ─────────────────────────────────────── */

/* pid > 0 and pid_exists and (not started or fingerprints_match(started,
 * start_time or 0)); any seam exception fails closed to false. */
/* PoP: asrun_owner_alive @ gateway/platforms/api_server_runs.py:_owner_alive */
bool asrun_owner_alive(long owner_pid, long owner_started, const asrun_seams_t *s);

/* NEW copy of the updated pollable status; mutates state["run_statuses"].
 * created_at setdefault (the fields pop is eager), shutdown stamp setdefault
 * on non-terminal, approval popped off waiting_for_approval, persist decision
 * (status change / terminal / special field) gated on the idempotency set. */
/* PoP: asrun_set_run_status @ gateway/platforms/api_server_runs.py:_set_run_status */
json_t *asrun_set_run_status(json_t *state, const char *run_id, const char *status,
                             const json_t *fields, const asrun_seams_t *s);

/* Stamp shutdown once (clock), then re-publish every non-terminal unstamped
 * status with shutdown_requested_at; returns the marked count. */
/* PoP: asrun_mark_shutdown_requested @ gateway/platforms/api_server_runs.py:_mark_shutdown_requested */
long asrun_mark_shutdown_requested(json_t *state, const asrun_seams_t *s);

/* ── Ownership teardown ─────────────────────────────────────────────────── */

/* Drop run_id from the named tables (sets discard, dicts pop), then release
 * the owner stamp. tables is a JSON array of state key names. */
/* PoP: asrun_forget_run @ gateway/platforms/api_server_runs.py:_forget_run */
void asrun_forget_run(json_t *state, const char *run_id, const json_t *tables);

/* Pop the owner only once none of the five live tables still holds run_id. */
/* PoP: asrun_release_run_owner_if_forgotten @ gateway/platforms/api_server_runs.py:_release_run_owner_if_forgotten */
void asrun_release_run_owner_if_forgotten(json_t *state, const char *run_id);

/* ── Durable hydration / replay ─────────────────────────────────────────── */

/* NEW status object or NULL; mutates state on the hydrate path. Canonical
 * projection wins (retained model/runtime/created_at ride along); in-memory
 * status extends retention when idempotent; a durable record hydrates and a
 * stale non-terminal owner is rewritten interrupted + persisted. *err_out
 * propagates scope/store/KeyError failures. */
/* PoP: asrun_durable_run_status @ gateway/platforms/api_server_runs.py:_durable_run_status */
json_t *asrun_durable_run_status(json_t *state, const json_t *request, json_t *req_state,
                                 const char *run_id, const asrun_seams_t *s, char **err_out);

/* NEW 202 descriptor {"run_id", "status", "replayed"}; Idempotency-Replayed
 * header when replayed, X-Hermes-Session-Key when the key is truthy. */
/* PoP: asrun_accepted_response @ gateway/platforms/api_server_runs.py:_accepted_response */
json_t *asrun_accepted_response(const char *run_id, const json_t *status,
                                const char *gateway_session_key, bool replayed,
                                const asrun_seams_t *s);

/* NEW descriptor: 409 on fingerprint conflict, else a replayed 202 of the
 * durable status (or record["status"] when durable is None/empty). */
/* PoP: asrun_replay_or_conflict @ gateway/platforms/api_server_runs.py:_replay_or_conflict */
json_t *asrun_replay_or_conflict(json_t *state, const json_t *request, json_t *req_state,
                                 const char *outcome, const json_t *record,
                                 const char *gateway_session_key, const asrun_seams_t *s,
                                 char **err_out);

/* ── History resolution ─────────────────────────────────────────────────── */

/* NEW history array; *instructions_out / *stored_out = NEW nodes or NULL;
 * *err_out = NEW 400 descriptor on validation failure. Precedence:
 * conversation_history > previous_response_id chain > all-but-last input. */
/* PoP: asrun_resolve_conversation_history @ gateway/platforms/api_server_runs.py:_resolve_conversation_history */
json_t *asrun_resolve_conversation_history(const json_t *body, const json_t *raw_input,
                                           const asrun_seams_t *s, json_t **instructions_out,
                                           json_t **stored_out, json_t **err_out);

#ifdef __cplusplus
}
#endif

#endif /* API_SERVER_RUNS_FLOW_H */