/*
 * api_server_flow.h — request-handling FLOW cores of gateway/platforms/api_server.py.
 *
 * Ports the DECISION sequences of the API-server request pipeline: auth checks
 * (_expected_api_key / _check_auth / _require_auth), route/validation preludes
 * (_cron_request_guard / _validate_cron_prompt / _parse_session_key_header),
 * error-response construction (_error_response family), approval-event shaping
 * and the ordered SSE event queue (_SessionEventQueue), plus the pure request
 * helpers they branch on. All live I/O (HTTP response emission, secret scope
 * reads, config/profile lookups, clocks, logging, loop/queue mechanics, storage
 * purges) enters through the apisf_seams_t callbacks below; the decision cores
 * perform NO I/O. Each seam documents its live counterpart.
 *
 * Sequences are observable through an apisf_trace_t: seam implementations
 * record one JSON event per call (arguments + scripted result) in call order;
 * tests/oracle_api_server_flow.py compares that ordered event list against
 * recording monkeypatch stubs driving the LIVE Python with the same scripts.
 *
 * web.Response is modeled as a JSON descriptor {"status", "body", "headers"}
 * produced by the json_response seam (live: web.json_response). Loop identity
 * is modeled by string tokens (live: `running_loop is self.loop`); the harness
 * and oracle compare tokens, never pointers.
 *
 * JSON values are libjson json_t. Ownership is documented per function.
 */

#ifndef API_SERVER_FLOW_H
#define API_SERVER_FLOW_H

#include <stdbool.h>
#include <stddef.h>

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Trace recorder (harness infrastructure, no Python counterpart) ─────────
 * events is a JSON_ARRAY of event objects. apisf_trace_add takes ownership of
 * ev (frees it when t is NULL). */
typedef struct apisf_trace {
    json_t *events;   /* owned JSON_ARRAY; NULL until first add */
} apisf_trace_t;

void apisf_trace_add(apisf_trace_t *t, json_t *ev);
void apisf_trace_free(apisf_trace_t *t);

/* ── Seams ──────────────────────────────────────────────────────────────────
 * One shared seam table; each flow calls exactly the members the live code
 * path reaches. NULL members are never called on a scripted path that does
 * not reach them. ctx is the harness/oracle script context.
 *
 * Member -> live counterpart:
 *   json_response          -> aiohttp web.json_response(body, status=, headers=)
 *                             returns a NEW response descriptor (caller frees)
 *   redact                 -> api_server.py:_redact_api_error_text (malloc'd)
 *   log                    -> module logger.warning/debug/info (pre-formatted)
 *   clock                  -> time.time()
 *   secrets_match          -> gateway/platforms/_shared.py:secrets_match
 *   audit_log_suffix       -> APIServerAdapter._request_audit_log_suffix (malloc'd)
 *   request_profile        -> _api_request_profile ContextVar .get() (borrowed)
 *   get_secret             -> agent.secret_scope.get_secret (malloc'd; *raised +
 *                             *exc_type = type(exc).__name__ on exception)
 *   has_usable_secret      -> hermes_cli.auth.has_usable_secret (*raised/*exc_type)
 *   check_auth             -> APIServerAdapter._check_auth (descriptor or NULL)
 *   draining_response      -> APIServerAdapter._draining_response (desc or NULL)
 *   handler                -> the awaited route handler (descriptor)
 *   scan_cron_prompt       -> tools.cronjob_tools._scan_cron_prompt (malloc'd or NULL)
 *   cors_headers_for_origin-> APIServerAdapter._cors_headers_for_origin (new obj/NULL)
 *   redact_approval_command-> gateway/run.py:_redact_approval_command (malloc'd)
 *   run_event              -> api_server_runs._run_event (new envelope object)
 *   approval_choices       -> api_server.py:_approval_event_choices (new array)
 *   routing_process_home   -> hermes_constants.get_routing_process_hermes_home
 *   profile_matches_home   -> hermes_cli.profiles.profile_matches_home(profile, home=)
 *   parse_turn_author      -> agent.turn_author.parse_turn_author (new obj;
 *                             *raised + *exc_msg = str(ValueError) on failure)
 *   clean_runtime_id       -> APIServerAdapter._clean_runtime_id (malloc'd)
 *   get_running_loop       -> asyncio.get_running_loop (token; *raised on RuntimeError)
 *   put_nowait             -> asyncio.Queue.put_nowait (event borrowed)
 *   call_soon_threadsafe   -> loop.call_soon_threadsafe(cb); the harness runs cb
 *                             synchronously (single-threaded scripted replay);
 *                             *raised on RuntimeError (loop closed)
 *   sleep_zero             -> asyncio.sleep(0) (flush wait tick)
 *   set_run_status         -> APIServerAdapter._set_run_status
 *   enqueue                -> _SessionEventQueue.enqueue (name/payload borrowed)
 */
typedef struct apisf_seams {
    void *ctx;
    json_t *(*json_response)(void *ctx, const json_t *body, int status, const json_t *headers);
    char *(*redact)(void *ctx, const char *text);
    void (*log)(void *ctx, const char *level, const char *msg);
    double (*clock)(void *ctx);
    bool (*secrets_match)(void *ctx, const char *a, const char *b);
    char *(*audit_log_suffix)(void *ctx, const json_t *request);
    const char *(*request_profile)(void *ctx);
    char *(*get_secret)(void *ctx, const char *name, const char *def,
                        bool *raised, const char **exc_type);
    bool (*has_usable_secret)(void *ctx, const char *value, int min_length,
                              bool *raised, const char **exc_type);
    json_t *(*check_auth)(void *ctx, const json_t *request);
    json_t *(*draining_response)(void *ctx);
    json_t *(*handler)(void *ctx, const json_t *request);
    char *(*scan_cron_prompt)(void *ctx, const char *prompt);
    json_t *(*cors_headers_for_origin)(void *ctx, const char *origin);
    char *(*redact_approval_command)(void *ctx, const char *text);
    json_t *(*run_event)(void *ctx, const char *run_id, const char *kind, const json_t *fields);
    json_t *(*approval_choices)(void *ctx, bool smart_denied, bool allow_session, bool allow_permanent);
    char *(*routing_process_home)(void *ctx, bool *raised);
    bool (*profile_matches_home)(void *ctx, const char *profile, const char *home, bool *raised);
    json_t *(*parse_turn_author)(void *ctx, const json_t *raw, bool *raised, const char **exc_msg);
    char *(*clean_runtime_id)(void *ctx, const json_t *value, int max_len);
    const char *(*get_running_loop)(void *ctx, bool *raised);
    void (*put_nowait)(void *ctx, const json_t *event);
    void (*call_soon_threadsafe)(void *ctx, void (*cb)(void *), void *cb_ctx, bool *raised);
    void (*sleep_zero)(void *ctx);
    void (*set_run_status)(void *ctx, const char *run_id, const char *status,
                           const char *last_event, const json_t *approval);
    void (*enqueue)(void *ctx, const char *name, const json_t *payload);
} apisf_seams_t;

/* ── Pure request helpers ───────────────────────────────────────────────── */

/* malloc'd copy of text capped at MAX_NORMALIZED_TEXT_LENGTH code points. */
/* PoP: apisf_cap_text @ gateway/platforms/api_server.py:_cap_text */
char *apisf_cap_text(const char *text);

/* NEW array: first MAX_CONTENT_LIST_SIZE items (copies). items borrowed. */
/* PoP: apisf_cap_list @ gateway/platforms/api_server.py:_cap_list */
json_t *apisf_cap_list(const json_t *items);

/* PoP: apisf_coerce_request_bool @ gateway/platforms/api_server.py:_coerce_request_bool */
bool apisf_coerce_request_bool(const json_t *value, bool default_value);

/* NEW object ({} when body/metadata is not an object). body borrowed. */
/* PoP: apisf_request_relay_metadata @ gateway/platforms/api_server.py:_request_relay_metadata */
json_t *apisf_request_relay_metadata(const json_t *body);

/* Canonical vision part or NULL; on ValueError *err_out = malloc'd
 * "<code>:<message>" (caller frees). part borrowed. */
/* PoP: apisf_normalize_image_part @ gateway/platforms/api_server.py:_normalize_image_part */
json_t *apisf_normalize_image_part(const json_t *part, char **err_out);

/* NEW usage objects; values copied verbatim, missing keys -> 0. */
/* PoP: apisf_chat_usage_payload @ gateway/platforms/api_server.py:_chat_usage_payload */
json_t *apisf_chat_usage_payload(const json_t *usage);
/* PoP: apisf_responses_usage_payload @ gateway/platforms/api_server.py:_responses_usage_payload */
json_t *apisf_responses_usage_payload(const json_t *usage);

/* sha256(repr({k: body.get(k) for k in keys})) hex; keys is a JSON array of
 * strings. repr follows Python syntax (None/True/False, single-quoted strs,
 * ", " separators); integral json numbers repr as ints (json_t carries no
 * int/float identity — a JSON 1.0 literal reads back as 1). */
/* PoP: apisf_make_request_fingerprint @ gateway/platforms/api_server.py:_make_request_fingerprint */
char *apisf_make_request_fingerprint(const json_t *body, const json_t *keys);

/* NEW header object. */
/* PoP: apisf_session_headers @ gateway/platforms/api_server.py:_session_headers */
json_t *apisf_session_headers(const char *session_id, const char *gateway_session_key);

/* NEW object: result["runtime"] or usage["runtime"] or {}. */
/* PoP: apisf_result_runtime @ gateway/platforms/api_server.py:_result_runtime */
json_t *apisf_result_runtime(const json_t *result, const json_t *usage);

/* Static string: "" / "accepted" / "confirmed". */
/* PoP: apisf_model_lock_state @ gateway/platforms/api_server.py:_model_lock_state */
const char *apisf_model_lock_state(const json_t *runtime_request, const json_t *runtime);

/* NEW [model, provider] array via the clean_runtime_id seam. */
/* PoP: apisf_requested_ids @ gateway/platforms/api_server.py:_requested_ids */
json_t *apisf_requested_ids(const json_t *requested, const apisf_seams_t *s);

/* ── Profile-scoped identity ────────────────────────────────────────────── */

/* PoP: apisf_names_launch_profile @ gateway/platforms/api_server.py:_names_launch_profile */
bool apisf_names_launch_profile(const char *profile, const apisf_seams_t *s);

/* malloc'd "api-<sha256(seed)[:16]>" session id. */
/* PoP: apisf_derive_chat_session_id @ gateway/platforms/api_server.py:_derive_chat_session_id */
char *apisf_derive_chat_session_id(const char *system_prompt, const char *first_user_message,
                                   const char *profile, const apisf_seams_t *s);

/* NEW author object or NULL; *err_out = malloc'd ValueError text on failure. */
/* PoP: apisf_request_turn_author @ gateway/platforms/api_server.py:_request_turn_author */
json_t *apisf_request_turn_author(const json_t *body, const apisf_seams_t *s, char **err_out);

/* ── Error-response construction ────────────────────────────────────────── */

/* NEW response descriptor; err_type/param/code/headers may be NULL. */
/* PoP: apisf_error_response @ gateway/platforms/api_server.py:_error_response */
json_t *apisf_error_response(const char *message, int status, const char *err_type,
                             const char *param, const char *code, const json_t *headers,
                             const apisf_seams_t *s);
/* PoP: apisf_invalid_request @ gateway/platforms/api_server.py:_invalid_request */
json_t *apisf_invalid_request(const char *message, const apisf_seams_t *s);
/* PoP: apisf_auth_failed_response @ gateway/platforms/api_server.py:_auth_failed_response */
json_t *apisf_auth_failed_response(const apisf_seams_t *s);
/* PoP: apisf_session_db_unavailable @ gateway/platforms/api_server.py:_session_db_unavailable */
json_t *apisf_session_db_unavailable(const apisf_seams_t *s);
/* exc_text is str(exc) of the live exception argument. */
/* PoP: apisf_cron_error_response @ gateway/platforms/api_server.py:_cron_error_response */
json_t *apisf_cron_error_response(const char *exc_text, const apisf_seams_t *s);

/* ── Auth flow ──────────────────────────────────────────────────────────── */

/* malloc'd key text ("" when none/fail-closed). api_key = self._api_key. */
/* PoP: apisf_expected_api_key @ gateway/platforms/api_server.py:_expected_api_key */
char *apisf_expected_api_key(const char *api_key, const apisf_seams_t *s);

/* NULL when authorized, else a NEW 401 descriptor. request models
 * web.Request as {"headers": {...}, "match_info": {...}}. */
/* PoP: apisf_check_auth @ gateway/platforms/api_server.py:_check_auth */
json_t *apisf_check_auth(const json_t *request, const char *api_key, const apisf_seams_t *s);

/* The _require_auth decorator pipeline: check_auth seam, then handler seam
 * only when auth passed. NEW descriptor. */
/* PoP: apisf_require_auth @ gateway/platforms/api_server.py:_require_auth */
json_t *apisf_require_auth(const json_t *request, const apisf_seams_t *s);

/* Shared /api/jobs prelude: auth -> (drain) -> cron available -> (job_id).
 * Returns NEW error descriptor or NULL; *job_id_out = malloc'd job id or NULL
 * (the invalid-format branch returns the id ALONGSIDE the error). */
/* PoP: apisf_cron_request_guard @ gateway/platforms/api_server.py:_cron_request_guard */
json_t *apisf_cron_request_guard(const json_t *request, bool need_job_id, bool check_draining,
                                 bool cron_available, const apisf_seams_t *s, char **job_id_out);

/* scan_available models `_scan_cron_prompt is not None`. NULL or NEW 400. */
/* PoP: apisf_validate_cron_prompt @ gateway/platforms/api_server.py:_validate_cron_prompt */
json_t *apisf_validate_cron_prompt(const char *prompt, bool scan_available, const apisf_seams_t *s);

/* X-Hermes-Session-Key validation: *key_out = malloc'd key or NULL; returns
 * NEW error descriptor or NULL. */
/* PoP: apisf_parse_session_key_header @ gateway/platforms/api_server.py:_parse_session_key_header */
json_t *apisf_parse_session_key_header(const json_t *request, const char *api_key,
                                       const apisf_seams_t *s, char **key_out);

/* NEW SSE header object (base + CORS seam result + extra, in update order). */
/* PoP: apisf_sse_headers @ gateway/platforms/api_server.py:_sse_headers */
json_t *apisf_sse_headers(const json_t *request, const json_t *extra, const apisf_seams_t *s);

/* ── Approval events / SSE ordering ─────────────────────────────────────── */

/* NEW approval.request event dict: redacted command/description, then the
 * _run_event envelope (fields + computed choices) merged over it. */
/* PoP: apisf_approval_request_event @ gateway/platforms/api_server.py:_approval_request_event */
json_t *apisf_approval_request_event(const char *run_id, const json_t *approval_data,
                                     const json_t *fields, const apisf_seams_t *s);

/* Registration half of _register_session_stream_approval: records
 * state["run_approval_sessions"][run_id] = run_id (internal step, observable
 * via the mutated state). */
/* PoP: apisf_register_session_stream_approval @ gateway/platforms/api_server.py:_register_session_stream_approval */
void apisf_register_session_stream_approval(json_t *state, const char *run_id);

/* The returned _approval_notify closure body: approval event -> set_run_status
 * -> events.enqueue, in that order. */
/* PoP: apisf_session_stream_approval_notify @ gateway/platforms/api_server.py:_register_session_stream_approval */
void apisf_session_stream_approval_notify(const char *run_id, const json_t *approval_data,
                                          const char *message_id, const apisf_seams_t *s);

/* ── _SessionEventQueue state flows ─────────────────────────────────────────
 * qstate models the queue instance: {"session_id", "run_id", "seq", "loop"
 * (identity token), "inflight", "queue" (array double, appended by the
 * put_nowait seam)}. payload/enqueue mutate qstate and the payload object. */

/* NEW [name, payload] tuple; stamps seq/session_id/run_id/ts via setdefault
 * (the clock seam runs even when ts is present — setdefault evaluates its
 * default eagerly). */
/* PoP: apisf_queue_payload @ gateway/platforms/api_server.py:payload */
json_t *apisf_queue_payload(json_t *qstate, const char *name, json_t *payload, const apisf_seams_t *s);

/* Ordered enqueue: payload -> get_running_loop -> same-loop put_nowait OR
 * inflight++ -> call_soon_threadsafe(_apply) (put_nowait + settle) OR settle
 * on the closed-loop RuntimeError path. */
/* PoP: apisf_queue_enqueue @ gateway/platforms/api_server.py:enqueue */
void apisf_queue_enqueue(json_t *qstate, const char *name, json_t *payload, const apisf_seams_t *s);

/* PoP: apisf_queue_settle_one @ gateway/platforms/api_server.py:_settle_one */
void apisf_queue_settle_one(json_t *qstate);

/* Wait ticks until inflight reaches 0 (sleep_zero seam). */
/* PoP: apisf_queue_flush @ gateway/platforms/api_server.py:flush */
void apisf_queue_flush(json_t *qstate, const apisf_seams_t *s);

/* ── _IdempotencyCache state flow ───────────────────────────────────────────
 * store models the OrderedDict: key -> {"resp", "fp", "ts"}; libjson object
 * key order is insertion order, so popitem(last=False) drops keys[0]. */
/* PoP: apisf_idem_purge @ gateway/platforms/api_server.py:_purge */
void apisf_idem_purge(json_t *store, double ttl, long max_items, const apisf_seams_t *s);

#ifdef __cplusplus
}
#endif

#endif /* API_SERVER_FLOW_H */
