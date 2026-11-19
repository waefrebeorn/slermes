/*
 * gateway_base_flow.h — platform-adapter base FLOW cores of
 * gateway/platforms/base.py (LIVE authority: BasePlatformAdapter).
 *
 * Ports the DECISION sequences of the adapter base: busy-message routing,
 * send/receive orchestration, lifecycle lock/plugin wiring, text batching,
 * session-task ownership handoff, ledgered final delivery, warning/media
 * fallback presentation, exec-approval/clarify prompts and source building.
 *
 * SEAM DOCTRINE: every live effect (network sends, sleeps, task creation,
 * config/env reads, i18n, logging, ledger/store calls, foreign-module helpers)
 * enters through ONE universal seam interface:
 *
 *   json_t *(*call)(void *ctx, const char *name, const json_t *args)
 *
 * `name` identifies the live call (tables below document which live call each
 * name stands in for); `args` is a borrowed JSON object of the call arguments;
 * the seam returns a NEW json_t* result (NULL == Python None). A raising live
 * call is scripted as an object {"__raise__": "<ExcClass>", ...extra fields};
 * the core propagates it exactly where the live try/except would catch it.
 * The harness seam implementation records one trace event per call
 * {"ev": name, "args": args, "ret": result} in call order; the oracle's
 * recording monkeypatch stubs emit the identical shape driving LIVE Python
 * with the same scripts. The decision cores perform NO I/O themselves.
 *
 * Adapter state is an injected json_t* object (`st`) mirroring the live
 * instance attributes (_active_sessions, _pending_messages, _session_tasks,
 * _requeue_counts, _pending_text_batches, _pending_text_batch_tasks,
 * _text_debounce, _post_delivery_callbacks, plugin/lock/profile fields...).
 * asyncio.Event is modeled as {"id": <int>, "set": <bool>}; asyncio.Task as
 * {"id": <int>, "done": <bool>, "cancelled": <bool>}; identity comparisons
 * (`is`) compare the "id" field. Mutations happen in place on st/event.
 *
 * JSON values are libjson json_t. Ownership: returned json_t* and char* are
 * malloc'd (caller frees) unless documented borrowed.
 */

#ifndef GATEWAY_BASE_FLOW_H
#define GATEWAY_BASE_FLOW_H

#include <stdbool.h>
#include <stddef.h>

#include "json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Trace recorder (harness infrastructure, no Python counterpart) ── */
typedef struct gwb_trace {
    json_t *events;   /* owned JSON_ARRAY; NULL until first add */
} gwb_trace_t;

void gwb_trace_add(gwb_trace_t *t, json_t *ev);
void gwb_trace_free(gwb_trace_t *t);

/* ── Universal seam interface ── */
typedef struct gwb_seams {
    void *ctx;
    /* Returns a NEW json_t* (NULL == None). {"__raise__": cls} == exception. */
    json_t *(*call)(void *ctx, const char *name, const json_t *args);
} gwb_seams_t;

/* Seam helpers (implemented in base_flow.c). */
json_t *gwb_call(const gwb_seams_t *s, const char *name, const json_t *args);
const char *gwb_raise_class(const json_t *r);   /* NULL when r is not a raise */
bool gwb_caught(const json_t *r, const char *cls); /* raise of cls (or subclass list) */

/* ── Typed callback seams for pure measures ── */
/* len_fn stands in for the live measure callable (len / utf16_len / escaped
 * composition): returns the unit length of the UTF-8 string. */
typedef int (*gwb_len_fn)(void *ctx, const char *s);
/* thunk stands in for _or_default's zero-arg callable; *exc receives
 * 0=ok, 1=TypeError, 2=ValueError, 3=KeyError, 4=other. Returns NEW value. */
typedef json_t *(*gwb_thunk_fn)(void *ctx, int *exc);
/* uniform stands in for random.uniform(lo, hi). */
typedef double (*gwb_uniform_fn)(void *ctx, double lo, double hi);

/* ================================================================
 * Pure / leaf ports (no I/O; seams only where the live def calls out)
 * ================================================================ */

/* _or_default: thunk() or default when it raises one of exc_mask
 * (bit0 TypeError, bit1 ValueError, bit2 KeyError). Uncaught exception:
 * returns NULL and sets *out_exc to the raised kind. default borrowed. */
/* PoP: gwb_or_default @ gateway/platforms/base.py:_or_default */
json_t *gwb_or_default(gwb_thunk_fn thunk, void *ctx, const json_t *defval,
                       int exc_mask, int *out_exc);

/* _thread_metadata_for_source: source is a JSON model of SessionSource
 * (platform {value}|string, thread_id, scope_id, chat_type, message_id,
 * profile). Returns NEW metadata object (insertion order semantic) or NULL. */
/* PoP: gwb_thread_metadata_for_source @ gateway/platforms/base.py:_thread_metadata_for_source */
json_t *gwb_thread_metadata_for_source(const json_t *source, const char *reply_to_message_id);

/* _thread_metadata_for_event: seam "reply_anchor" stands in for
 * _reply_anchor_for_event(event) (claimed port elsewhere). */
/* PoP: gwb_thread_metadata_for_event @ gateway/platforms/base.py:_thread_metadata_for_event */
json_t *gwb_thread_metadata_for_event(const json_t *event, const gwb_seams_t *s);

/* _mark_notify_metadata: clone truthy metadata else {}, set notify=True. */
/* PoP: gwb_mark_notify_metadata @ gateway/platforms/base.py:_mark_notify_metadata */
json_t *gwb_mark_notify_metadata(const json_t *metadata);

/* _media_failure_text: seam "t" stands in for agent/i18n.py:t(key, **kwargs). */
/* PoP: gwb_media_failure_text @ gateway/platforms/base.py:_media_failure_text */
char *gwb_media_failure_text(const char *kind, const char *file_name, const gwb_seams_t *s);

/* should_send_media_as_audio: platform name or {value}; pure. */
/* PoP: gwb_should_send_media_as_audio @ gateway/platforms/base.py:should_send_media_as_audio */
bool gwb_should_send_media_as_audio(const char *platform, const char *ext, bool is_voice);

/* _custom_unit_to_cp: largest codepoint offset n with len_fn(s[:n]) <= budget. */
/* PoP: gwb_custom_unit_to_cp @ gateway/platforms/base.py:_custom_unit_to_cp */
int gwb_custom_unit_to_cp(const char *s, int budget, gwb_len_fn len_fn, void *ctx);

/* fit_prefix: text or longest prefix within budget measure units + suffix. */
/* PoP: gwb_fit_prefix @ gateway/platforms/base.py:fit_prefix */
char *gwb_fit_prefix(const char *text, int budget, gwb_len_fn measure, void *ctx,
                     const char *suffix);

/* _prefix_within_utf16_limit: never splits a surrogate pair. */
/* PoP: gwb_prefix_within_utf16_limit @ gateway/platforms/base.py:_prefix_within_utf16_limit */
char *gwb_prefix_within_utf16_limit(const char *s, int limit);

/* _has_media_directives / _terminal_sentinel_start / _code_spans /
 * _blank_spans / _delete_spans: pure text scanners (codepoint offsets). */
/* PoP: gwb_has_media_directives @ gateway/platforms/base.py:_has_media_directives */
bool gwb_has_media_directives(const char *text);
/* PoP: gwb_terminal_sentinel_start @ gateway/platforms/base.py:_terminal_sentinel_start */
int gwb_terminal_sentinel_start(const char *text);
/* PoP: gwb_code_spans @ gateway/platforms/base.py:_code_spans */
json_t *gwb_code_spans(const char *content);
/* PoP: gwb_blank_spans @ gateway/platforms/base.py:_blank_spans */
char *gwb_blank_spans(const char *text, const json_t *spans);
/* PoP: gwb_delete_spans @ gateway/platforms/base.py:_delete_spans */
char *gwb_delete_spans(const char *text, const json_t *spans);

/* _append_text: existing\nnew when existing truthy (new str()'d, None->"None"),
 * else new verbatim (may be NULL). Returns NEW string or NULL. */
/* PoP: gwb_append_text @ gateway/platforms/base.py:_append_text */
char *gwb_append_text(const char *existing, const char *new_text);

/* _any_in / classify_send_error: exc modeled as {"class": name, "str": text}
 * or NULL; ordered classifier table, first match wins, else "unknown". */
/* PoP: gwb_any_in @ gateway/platforms/base.py:_any_in */
bool gwb_any_in(const char *blob, const char *const *needles, size_t n);
/* PoP: gwb_classify_send_error @ gateway/platforms/base.py:classify_send_error */
const char *gwb_classify_send_error(const json_t *exc, const char *error_text);

/* _split_post_delivery_entry: 2-element array (tuple) -> [gen, cb] verbatim;
 * anything else -> [null, entry]. Returns NEW 2-element array. */
/* PoP: gwb_split_post_delivery_entry @ gateway/platforms/base.py:_split_post_delivery_entry */
json_t *gwb_split_post_delivery_entry(const json_t *entry);

/* extract_images: ![alt](url) + <img src=...> extraction with image-marker
 * filter, tag removal of extracted urls only, blank-line collapse + strip.
 * Returns NEW {"images": [[url, alt], ...], "cleaned": str}. */
/* PoP: gwb_extract_images @ gateway/platforms/base.py:extract_images */
json_t *gwb_extract_images(const char *content);

/* _coerce_float_extra: value is extra.get(key) as JSON (null == None/absent);
 * float() coercion, NaN/Inf/negative -> default, clamp [min_value, max_value].
 * Seam "log" (level=warning) on ceiling clamp; max_is_float selects Python
 * str(float) vs str(int) rendering of the ceiling in that message. */
/* PoP: gwb_coerce_float_extra @ gateway/platforms/base.py:_coerce_float_extra */
double gwb_coerce_float_extra(const json_t *value, double defval, double min_value,
                              bool has_max, double max_value, bool max_is_float,
                              const char *key, const gwb_seams_t *s);

/* _configure_text_batch_delays: reads st.config.extra; mutates
 * st._text_batch_delay_seconds / _text_batch_split_delay_seconds. */
/* PoP: gwb_configure_text_batch_delays @ gateway/platforms/base.py:_configure_text_batch_delays */
void gwb_configure_text_batch_delays(json_t *st, const gwb_seams_t *s);

/* _text_batch_delay_for: pending NULL or _last_chunk_len < _SPLIT_THRESHOLD
 * (4000) -> _text_batch_delay_seconds else split delay. */
/* PoP: gwb_text_batch_delay_for @ gateway/platforms/base.py:_text_batch_delay_for */
double gwb_text_batch_delay_for(const json_t *st, const json_t *pending);

/* _owner_transport_profile: owner str with non-blank strip, else NULL. */
/* PoP: gwb_owner_transport_profile @ gateway/platforms/base.py:_owner_transport_profile */
const char *gwb_owner_transport_profile(const json_t *st);

/* _is_rate_limited_error / _is_timeout_error / _send_retry_is_final /
 * _is_partial_delivery / _merge_caption: pure send-result predicates. */
/* PoP: gwb_is_rate_limited_error @ gateway/platforms/base.py:_is_rate_limited_error */
bool gwb_is_rate_limited_error(const char *error);
/* PoP: gwb_is_timeout_error @ gateway/platforms/base.py:_is_timeout_error */
bool gwb_is_timeout_error(const char *error);
/* PoP: gwb_send_retry_is_final @ gateway/platforms/base.py:_send_retry_is_final */
bool gwb_send_retry_is_final(const json_t *result);
/* PoP: gwb_is_partial_delivery @ gateway/platforms/base.py:_is_partial_delivery */
bool gwb_is_partial_delivery(const json_t *result);
/* PoP: gwb_merge_caption @ gateway/platforms/base.py:_merge_caption */
char *gwb_merge_caption(const char *existing_text, const char *new_text);

/* _requeue_backoff_delay: same-event bounce detection + counter mutation on
 * st._requeue_counts; jittered_backoff(attempts, 0.25, 1.0, jitter 0) is
 * deterministic min(0.25*2^(attempts-1), 1.0). Seam "log" (level info/debug). */
/* PoP: gwb_requeue_backoff_delay @ gateway/platforms/base.py:_requeue_backoff_delay */
double gwb_requeue_backoff_delay(json_t *st, const char *session_key,
                                 const json_t *pending_event, const json_t *dispatched_event,
                                 const gwb_seams_t *s);

/* _get_human_delay: bounds falsy -> 0.0 else uniform(lo/1000, hi/1000). */
/* PoP: gwb_get_human_delay @ gateway/platforms/base.py:_get_human_delay */
double gwb_get_human_delay(const json_t *st, gwb_uniform_fn uniform, void *ctx);

/* _wants_auto_tts: seams "should_auto_tts" (_should_auto_tts_for_chat, claimed)
 * and "tts_turn_completed" (_streaming_tts_turn_completed, claimed). */
/* PoP: gwb_wants_auto_tts @ gateway/platforms/base.py:_wants_auto_tts */
bool gwb_wants_auto_tts(const json_t *st, const json_t *event, const char *session_key,
                        const json_t *interrupt_event, const char *text_content,
                        const json_t *media_files, const gwb_seams_t *s);

/* _shared_runtime_owns_input: st._message_handler_self models
 * handler.__self__ ({"session_authority": ...} or NULL); seam "get_command"
 * stands in for event.get_command(). */
/* PoP: gwb_shared_runtime_owns_input @ gateway/platforms/base.py:_shared_runtime_owns_input */
bool gwb_shared_runtime_owns_input(const json_t *st, const json_t *event, const gwb_seams_t *s);

/* _clear_session_guard: clear (set=false), never delete. */
/* PoP: gwb_clear_session_guard @ gateway/platforms/base.py:_clear_session_guard */
void gwb_clear_session_guard(json_t *st, const char *session_key);

/* warning_notifications_enabled: seam "warning_policy" stands in for
 * gateway.warning_notifications.warning_notifications_enabled(platform). */
/* PoP: gwb_warning_notifications_enabled @ gateway/platforms/base.py:warning_notifications_enabled */
bool gwb_warning_notifications_enabled(const json_t *st, const json_t *logical_platform,
                                       const gwb_seams_t *s);

/* warning_text: visible when enabled else hidden (default ""). */
/* PoP: gwb_warning_text @ gateway/platforms/base.py:warning_text */
const char *gwb_warning_text(const json_t *st, const char *visible, const char *hidden,
                             const json_t *logical_platform, const json_t *chat_id,
                             const json_t *metadata, const gwb_seams_t *s);

/* _exec_approval_actions: seam "ea_labels" stands in for
 * base_exec_approval.ea_action_labels() (dict choice->label). */
/* PoP: gwb_exec_approval_actions @ gateway/platforms/base.py:_exec_approval_actions */
json_t *gwb_exec_approval_actions(bool allow_permanent, bool allow_session,
                                  bool smart_denied, const gwb_seams_t *s);

/* ExecApprovalPrompt.choices: projection of actions rows [label, choice, style]. */
/* PoP: gwb_exec_approval_prompt_choices @ gateway/platforms/base.py:choices */
json_t *gwb_exec_approval_prompt_choices(const json_t *actions);

/* TextDebounceState.cancel_timer: state.task {id, done, cancelled}; cancel
 * when live, not `unless` (id match), not done. Mutates state in place. */
/* PoP: gwb_text_debounce_cancel_timer @ gateway/platforms/base.py:cancel_timer */
void gwb_text_debounce_cancel_timer(json_t *state, bool has_unless, int unless_id);

/* unauthorized_action_notice: seam "t". platform {value}|string|null. */
/* PoP: gwb_unauthorized_action_notice @ gateway/platforms/base.py:unauthorized_action_notice */
char *gwb_unauthorized_action_notice(const json_t *platform, const gwb_seams_t *s);

/* get_inbound_media_max_bytes: seam "config_section" stands in for
 * _config_section("gateway") (returns object or {"__raise__": ...}). */
/* PoP: gwb_get_inbound_media_max_bytes @ gateway/platforms/base.py:get_inbound_media_max_bytes */
long long gwb_get_inbound_media_max_bytes(const gwb_seams_t *s);

/* validate_inbound_media_size: returns NEW error message string when the cap
 * is exceeded, else NULL. max_bytes NULL -> resolve via seam "inbound_max". */
/* PoP: gwb_validate_inbound_media_size @ gateway/platforms/base.py:validate_inbound_media_size */
char *gwb_validate_inbound_media_size(long long size, const char *media_type,
                                      const json_t *max_bytes, const gwb_seams_t *s);

/* read_httpx_body_with_limit: headers object + chunks array of strings;
 * seams "config_section" (via inbound max), "log" (level=debug). Returns NEW
 * {"body": str, "error": str|null}. */
/* PoP: gwb_read_httpx_body_with_limit @ gateway/platforms/base.py:read_httpx_body_with_limit */
json_t *gwb_read_httpx_body_with_limit(const json_t *headers, const json_t *chunks,
                                       const char *media_type, const gwb_seams_t *s);

/* is_network_accessible: IP literals parsed (loopback incl. IPv4-mapped);
 * hostnames via seam "getaddrinfo" (returns array of address strings or
 * {"__raise__": "gaierror"}); DNS failure fails closed (True). */
/* PoP: gwb_is_network_accessible @ gateway/platforms/base.py:is_network_accessible */
bool gwb_is_network_accessible(const char *host, const gwb_seams_t *s);

/* _detect_macos_system_proxy: cache is an injected object {"ts": num, "value":
 * str|null} or NULL; seams "is_darwin", "monotonic", "scutil" (returns output
 * string or {"__raise__": ...}). Mutates *cache in place, creating the cache
 * object when absent (live memoises successes AND failures).
 * Returns NEW proxy string or NULL. */
/* PoP: gwb_detect_macos_system_proxy @ gateway/platforms/base.py:_detect_macos_system_proxy */
char *gwb_detect_macos_system_proxy(json_t **cache_p, const gwb_seams_t *s);

/* reset_macos_proxy_cache: *cache = NULL (caller frees old). */
/* PoP: gwb_reset_macos_proxy_cache @ gateway/platforms/base.py:reset_macos_proxy_cache */
void gwb_reset_macos_proxy_cache(json_t **cache);

/* resolve_proxy_url: seams "scoped_secret" (get_scoped_secret(var, "")),
 * "trust_env" (gateway_trust_env), "first_proxy_env" (first_proxy_env_value),
 * "normalize_proxy" (utils.normalize_proxy_url), "macos_proxy"
 * (_detect_macos_system_proxy), "bypass" (should_bypass_proxy). */
/* PoP: gwb_resolve_proxy_url @ gateway/platforms/base.py:resolve_proxy_url */
char *gwb_resolve_proxy_url(const char *platform_env_var, const json_t *target_hosts,
                            const char *configured, const gwb_seams_t *s);

/* _config_section: seam "load_config" (load_config_readonly; object or raise). */
/* PoP: gwb_config_section @ gateway/platforms/base.py:_config_section */
json_t *gwb_config_section(const char *name, const gwb_seams_t *s);

/* proxy_kwargs_for_aiohttp: seam "socks_connector" (_aiohttp_socks_connector).
 * Returns NEW {"session": {...}, "request": {...}}. */
/* PoP: gwb_proxy_kwargs_for_aiohttp @ gateway/platforms/base.py:proxy_kwargs_for_aiohttp */
json_t *gwb_proxy_kwargs_for_aiohttp(const char *proxy_url, const gwb_seams_t *s);

/* _aiohttp_socks_connector: seam "import_connector" (ProxyConnector.from_url;
 * result or {"__raise__": "ImportError"}); seam "log_warning" on SOCKS miss. */
/* PoP: gwb_aiohttp_socks_connector @ gateway/platforms/base.py:_aiohttp_socks_connector */
json_t *gwb_aiohttp_socks_connector(const char *proxy_url, const gwb_seams_t *s);

/* should_bypass_proxy / is_host_excluded_by_no_proxy: seam "bypass_matcher"
 * stands in for agent.proxy_bypass.should_bypass_proxy (foreign module). */
/* PoP: gwb_should_bypass_proxy @ gateway/platforms/base.py:should_bypass_proxy */
bool gwb_should_bypass_proxy(const json_t *target_hosts, const gwb_seams_t *s);
/* PoP: gwb_is_host_excluded_by_no_proxy @ gateway/platforms/base.py:is_host_excluded_by_no_proxy */
bool gwb_is_host_excluded_by_no_proxy(const char *hostname, const json_t *no_proxy_value,
                                      const gwb_seams_t *s);

/* format_message / message_len_fn / supports_draft_streaming / send_draft:
 * live defaults (identity, codepoint len, False, NotImplementedError). */
/* PoP: gwb_format_message @ gateway/platforms/base.py:format_message */
char *gwb_format_message(const char *content);
/* PoP: gwb_message_len_fn @ gateway/platforms/base.py:message_len_fn */
int gwb_message_len_fn(const char *s);
/* PoP: gwb_supports_draft_streaming @ gateway/platforms/base.py:supports_draft_streaming */
bool gwb_supports_draft_streaming(const json_t *chat_type, const json_t *metadata,
                                  const json_t *chat_id);
/* PoP: gwb_send_draft @ gateway/platforms/base.py:send_draft */
char *gwb_send_draft(const char *adapter_class_name);

/* _mark_degraded: seam "write_status" stands in for _write_runtime_status_safe
 * (claimed port); args {context, platform_state, error_code, error_message}. */
/* PoP: gwb_mark_degraded @ gateway/platforms/base.py:_mark_degraded */
void gwb_mark_degraded(json_t *st, const gwb_seams_t *s);

/* _lazy_attr: seam "factory" stands in for the factory callable; mutates st. */
/* PoP: gwb_lazy_attr @ gateway/platforms/base.py:_lazy_attr */
json_t *gwb_lazy_attr(json_t *st, const char *name, const gwb_seams_t *s);

/* _accepts_kwarg: params is a JSON array of {"name": str, "var_keyword": bool}
 * or NULL when the signature cannot be introspected (-> unknown). */
/* PoP: gwb_accepts_kwarg @ gateway/platforms/base.py:_accepts_kwarg */
bool gwb_accepts_kwarg(const json_t *params, const char *name, bool var_kw, bool unknown);

/* prepare_tts_text: seam "spoken" (tools.tts_text_normalize.prepare_spoken_text;
 * result string or {"__raise__": ...}); fallback regex path ported directly. */
/* PoP: gwb_prepare_tts_text @ gateway/platforms/base.py:prepare_tts_text */
char *gwb_prepare_tts_text(const char *text, const gwb_seams_t *s);

/* _media_delivery_scope: seam "scope_resolve" (runner._media_delivery_scope_for_source;
 * context object or raise). Returns NEW {"noop": true} or the resolved context. */
/* PoP: gwb_media_delivery_scope @ gateway/platforms/base.py:_media_delivery_scope */
json_t *gwb_media_delivery_scope(const json_t *st, const json_t *source, const gwb_seams_t *s);

/* _pending_recovery_owners: seam "delivery_adapter" (runner._delivery_adapter_for;
 * returns {"id": str, "active_sessions": [keys]} or NULL/raise).
 * Returns NEW array of owner ids ("self" first). */
/* PoP: gwb_pending_recovery_owners @ gateway/platforms/base.py:_pending_recovery_owners */
json_t *gwb_pending_recovery_owners(const json_t *st, const json_t *event,
                                    const char *session_key, const gwb_seams_t *s);

/* _track_session_task: task {id, done, unhashable?}; mutates st._session_tasks
 * and st._background_tasks (array of ids); False when add raises TypeError. */
/* PoP: gwb_track_session_task @ gateway/platforms/base.py:_track_session_task */
bool gwb_track_session_task(json_t *st, const char *session_key, json_t *task);

/* _stop_typing_quietly: seam "stop_typing_meta" (_stop_typing_with_metadata,
 * claimed); exceptions swallowed. */
/* PoP: gwb_stop_typing_quietly @ gateway/platforms/base.py:_stop_typing_quietly */
void gwb_stop_typing_quietly(const char *chat_id, const json_t *metadata, const gwb_seams_t *s);

/* _start_typing_refresh: seams "typing_wanted" (helpers.typing_refresh_wanted),
 * "accepts_stop_event" (_accepts_kwarg verdict), "create_task". Returns NEW
 * task object or NULL. */
/* PoP: gwb_start_typing_refresh @ gateway/platforms/base.py:_start_typing_refresh */
json_t *gwb_start_typing_refresh(const json_t *st, const json_t *event,
                                 const json_t *interrupt_event, const json_t *metadata,
                                 const gwb_seams_t *s);

/* _chain_callbacks: seam "cb_<i>" per callback (result null or {"__raise__": ...}
 * or {"awaitable": true}); seam "log" (level=debug) on failure. Runs in order, isolated. */
/* PoP: gwb_chain_callbacks @ gateway/platforms/base.py:_chain_callbacks */
void gwb_chain_callbacks(size_t n, const gwb_seams_t *s);

/* _send_plain_fallback: seams "t", "warning_policy", "send". content[:3500]
 * codepoints; warning_text(visible=t(prefix), hidden=content[:3500]). */
/* PoP: gwb_send_plain_fallback @ gateway/platforms/base.py:_send_plain_fallback */
json_t *gwb_send_plain_fallback(const json_t *st, const char *chat_id, const char *content,
                                const json_t *reply_to, const json_t *metadata, const gwb_seams_t *s);

/* _resume_partial_send: live default returns None (cannot resume). */
/* PoP: gwb_resume_partial_send @ gateway/platforms/base.py:_resume_partial_send */
json_t *gwb_resume_partial_send(const json_t *st, const char *chat_id, const json_t *result,
                                const json_t *reply_to, const json_t *metadata);

/* _play_tts_file: seams "play_tts", "record_delivery". Returns caption-delivered. */
/* PoP: gwb_play_tts_file @ gateway/platforms/base.py:_play_tts_file */
bool gwb_play_tts_file(const json_t *st, const json_t *event, const char *text_content,
                       const char *tts_path, bool first, const json_t *metadata,
                       const gwb_seams_t *s);

/* _record_delivery_obligation / _finalize_delivery_obligation: seams
 * "record_final_obligation" / "finalize_final_obligation" (base_delivery_ledger). */
/* PoP: gwb_record_delivery_obligation @ gateway/platforms/base.py:_record_delivery_obligation */
json_t *gwb_record_delivery_obligation(const json_t *st, const json_t *event,
                                       const char *session_key, const char *text_content,
                                       const json_t *delivery_adapter, bool is_ephemeral_response,
                                       const gwb_seams_t *s);
/* PoP: gwb_finalize_delivery_obligation @ gateway/platforms/base.py:_finalize_delivery_obligation */
void gwb_finalize_delivery_obligation(const json_t *st, const char *obligation_id,
                                      const json_t *result, const json_t *event,
                                      const json_t *delivery_adapter, const gwb_seams_t *s);

/* _fire_post_delivery_callback: seams "pop_post_delivery" (claimed pop) and
 * "run_post_delivery" (base_post_delivery.run_post_delivery_callback, 30.0s). */
/* PoP: gwb_fire_post_delivery_callback @ gateway/platforms/base.py:_fire_post_delivery_callback */
void gwb_fire_post_delivery_callback(json_t *st, const char *session_key,
                                     const json_t *generation, const gwb_seams_t *s);

/* _release_turn_marker: seam "clear_durable_turn" (runner._clear_durable_active_turn);
 * sets event._gateway_marker_released {"set": true} (created when absent). */
/* PoP: gwb_release_turn_marker @ gateway/platforms/base.py:_release_turn_marker */
void gwb_release_turn_marker(const json_t *st, json_t *event, const gwb_seams_t *s);

/* ledger_attempted_final: seams "extract_text" (_extract_response_content ->
 * text_content) and "ledger_attempted" (base_delivery_ledger.ledger_attempted). */
/* PoP: gwb_ledger_attempted_final @ gateway/platforms/base.py:ledger_attempted_final */
bool gwb_ledger_attempted_final(const json_t *st, const json_t *event, const char *session_key,
                                const char *response, const char *error, const gwb_seams_t *s);

/* _session_key_profile: seams "identity_of" (returns {"session_key_profile": ...}
 * or NULL) and "store_resolve" (store._resolve_profile_for_key; str or raise). */
/* PoP: gwb_session_key_profile @ gateway/platforms/base.py:_session_key_profile */
json_t *gwb_session_key_profile(const json_t *st, const json_t *source, const gwb_seams_t *s);

/* _canonicalize: seams "identity_of", "canonical_identity" (raise -> NULL).
 * st.gateway_runner models the runner ({"has_transport_owner": bool} or NULL). */
/* PoP: gwb_canonicalize @ gateway/platforms/base.py:_canonicalize */
json_t *gwb_canonicalize(const json_t *st, const json_t *source, const gwb_seams_t *s);

/* _drop_unresolved: seam "drop_rejected_route" (parked_route_refusal). */
/* PoP: gwb_drop_unresolved @ gateway/platforms/base.py:_drop_unresolved */
bool gwb_drop_unresolved(const json_t *st, const json_t *event, const gwb_seams_t *s);

/* _ea_fit: seams "ea_escape" (default identity when NULL result), "measure". */
/* PoP: gwb_ea_fit @ gateway/platforms/base.py:_ea_fit */
char *gwb_ea_fit(const char *text, int budget, const char *suffix, const gwb_seams_t *s);

/* _ea_deadline_line: seams "approval_timeout" (approval_timeout_seconds),
 * "deadline_line" (format_approval_deadline_line), "ea_escape". */
/* PoP: gwb_ea_deadline_line @ gateway/platforms/base.py:_ea_deadline_line */
char *gwb_ea_deadline_line(const json_t *st, const gwb_seams_t *s);

/* _exec_approval_cmd_budget: seam "cmd_budget" (base_exec_approval.exec_approval_cmd_budget). */
/* PoP: gwb_exec_approval_cmd_budget @ gateway/platforms/base.py:_exec_approval_cmd_budget */
int gwb_exec_approval_cmd_budget(const json_t *st, const char *description,
                                 bool smart_denied, const gwb_seams_t *s);

/* supports_exec_approval_buttons: overrides_prompt is the injected identity
 * fact (cls._send_exec_approval_prompt is not BasePlatformAdapter's). */
/* PoP: gwb_supports_exec_approval_buttons @ gateway/platforms/base.py:supports_exec_approval_buttons */
bool gwb_supports_exec_approval_buttons(bool overrides_prompt);

/* _is_queue_text_debounce_candidate: seam "is_command" (event.is_command()). */
bool gwb_is_queue_text_debounce_candidate(const json_t *st, const json_t *event,
                                          const gwb_seams_t *s);

/* _event_session_key / _source_session_key / _text_batch_key: seams
 * "build_session_key", "dm_thread_key_source", "session_db_for_source",
 * "dm_thread_batch_suffix", plus canonicalize/identity seams. */
/* PoP: gwb_event_session_key @ gateway/platforms/base.py:_event_session_key */
char *gwb_event_session_key(const json_t *st, const json_t *event, const gwb_seams_t *s);
/* PoP: gwb_source_session_key @ gateway/platforms/base.py:_source_session_key */
char *gwb_source_session_key(const json_t *st, const json_t *source, const gwb_seams_t *s);
/* PoP: gwb_text_batch_key @ gateway/platforms/base.py:_text_batch_key */
char *gwb_text_batch_key(const json_t *st, const json_t *event, const gwb_seams_t *s);

/* _pop_text_batch / _dispatch_text_batch: seam "handle_message". */
/* PoP: gwb_pop_text_batch @ gateway/platforms/base.py:_pop_text_batch */
json_t *gwb_pop_text_batch(json_t *st, const char *key);
/* PoP: gwb_dispatch_text_batch @ gateway/platforms/base.py:_dispatch_text_batch */
void gwb_dispatch_text_batch(const json_t *st, const json_t *event, const gwb_seams_t *s);

/* ================================================================
 * FLOW ports (decision sequences; seam names documented per flow)
 * ================================================================ */

/* _handle_message_while_active — busy-session routing.
 * Seams: dispatch_busy_burst, get_command, should_bypass, is_interrupt,
 * dispatch_active_session_command, dispatch_inline_reply, clarify_pending
 * (get_pending_for_session), busy_handler, delivery_adapter, get_pending
 * (owner.get_pending_message), start_session (owner._start_session_processing),
 * merge_pending (merge_pending_message_event), queue_debounce
 * (_queue_text_debounce), is_command, log, plus canonicalize/identity seams.
 * Mutates event._gateway_accepted; returns NEW {"accepted": bool|null}. */
/* PoP: gwb_handle_message_while_active @ gateway/platforms/base.py:_handle_message_while_active */
json_t *gwb_handle_message_while_active(json_t *st, json_t *event, const char *session_key,
                                        const gwb_seams_t *s);

/* _deliver_media_attachments — MEDIA/local dispatch by type.
 * Seams: send_multiple_images, send_footer (send_footer_unless_captioned),
 * record_delivery, send_voice, send_video, send_document, notify_failure
 * (_notify_media_delivery_failure), sleep, log, t. */
/* PoP: gwb_deliver_media_attachments @ gateway/platforms/base.py:_deliver_media_attachments */
void gwb_deliver_media_attachments(json_t *st, const json_t *event, const json_t *media_files,
                                   const json_t *local_files, bool force_document_attachments,
                                   double human_delay, const json_t *metadata, const gwb_seams_t *s);

/* _send_image_batch — batch send + footer. Seams: send_multiple_images,
 * record_delivery, send_footer, log. */
/* PoP: gwb_send_image_batch @ gateway/platforms/base.py:_send_image_batch */
void gwb_send_image_batch(json_t *st, const json_t *event, const json_t *images,
                          const json_t *metadata, double human_delay, const gwb_seams_t *s);

/* _acquire_platform_lock — scoped lock lifecycle.
 * Seams: acquire_lock (-> {"acquired": bool, "existing": obj|null} or
 * {"__raise__": "ScopedLockDirUnusable", "filename": ...}), takeover
 * (take_over_scoped_lock_holder -> pid|null), owner_label
 * (scoped_lock_owner_label), dir_remedy (unusable_dir_remedy),
 * set_fatal_error (_set_fatal_error), log. Mutates st lock fields. */
/* PoP: gwb_acquire_platform_lock @ gateway/platforms/base.py:_acquire_platform_lock */
bool gwb_acquire_platform_lock(json_t *st, const char *scope, const char *identity,
                               const char *resource_desc, const gwb_seams_t *s);

/* _release_platform_lock — seam "release_lock" (release_scoped_lock). */
/* PoP: gwb_release_platform_lock @ gateway/platforms/base.py:_release_platform_lock */
void gwb_release_platform_lock(json_t *st, const gwb_seams_t *s);

/* _wire_plugin_handlers — plugin factory wiring.
 * Seams: get_factories (-> array of {id, plugin, qualname, repr} or raise),
 * factory (factory(native, adapter); null or raise), log.
 * native_id: injected identity of the native client (NULL == None).
 * Mutates st._plugin_handler_native / st._plugin_handlers_wired. */
/* PoP: gwb_wire_plugin_handlers @ gateway/platforms/base.py:_wire_plugin_handlers */
void gwb_wire_plugin_handlers(json_t *st, const json_t *native_id, const gwb_seams_t *s);

/* rewire_plugin_handlers — re-wire on the live native client. */
/* PoP: gwb_rewire_plugin_handlers @ gateway/platforms/base.py:rewire_plugin_handlers */
void gwb_rewire_plugin_handlers(json_t *st, const gwb_seams_t *s);

/* _enqueue_text_event — text batch intake.
 * Seams: drop seams, key seams, pad_flags (pad_inline_flags), extend_media,
 * absorb_turn_input, create_task, log. Mutates st batches/tasks + event text. */
/* PoP: gwb_enqueue_text_event @ gateway/platforms/base.py:_enqueue_text_event */
void gwb_enqueue_text_event(json_t *st, json_t *event, const gwb_seams_t *s);

/* _flush_text_batch — quiet-period flush with task-identity race guard.
 * current_task_id: injected asyncio.current_task() identity.
 * Seams: sleep (null or {"__raise__": "CancelledError"}), dispatch
 * (_dispatch_text_batch -> handle_message; may raise CancelledError), log. */
/* PoP: gwb_flush_text_batch @ gateway/platforms/base.py:_flush_text_batch */
json_t *gwb_flush_text_batch(json_t *st, const char *key, int current_task_id, const gwb_seams_t *s);

/* _flush_text_batch_now — immediate dispatch. Seam "handle_message". */
/* PoP: gwb_flush_text_batch_now @ gateway/platforms/base.py:_flush_text_batch_now */
void gwb_flush_text_batch_now(json_t *st, const char *key, const gwb_seams_t *s);

/* _finish_session_task — end-of-task ownership reconciliation.
 * Seams: spawn_drain (internal _spawn_drain_task -> create_task/track),
 * cleanup_finished (_cleanup_finished_session_task, claimed), log. */
/* PoP: gwb_finish_session_task @ gateway/platforms/base.py:_finish_session_task */
void gwb_finish_session_task(json_t *st, const char *session_key, int current_task_id,
                             const json_t *interrupt_event, const gwb_seams_t *s);

/* _spawn_drain_task — drain handoff (never recurse).
 * Seams: create_task, plus track_session_task state effects. */
/* PoP: gwb_spawn_drain_task @ gateway/platforms/base.py:_spawn_drain_task */
void gwb_spawn_drain_task(json_t *st, const json_t *pending_event, const char *session_key,
                          double delay, const gwb_seams_t *s);

/* _drain_after — back-off sleep then process or cleanup.
 * Seams: sleep, flush_debounce (_flush_text_debounce_now, claimed),
 * cleanup_finished, process_background (_process_message_background). */
/* PoP: gwb_drain_after @ gateway/platforms/base.py:_drain_after */
void gwb_drain_after(json_t *st, const json_t *pending_event, const char *session_key,
                     double delay, const json_t *guard, const gwb_seams_t *s);

/* send_final_ledgered — the delivery-ledger bracket.
 * Seams: ledger_bracket drives the bracket: the core passes its _send closure
 * as seam "ledger_send" invoked BY the bracket seam implementation with args
 * {delivery_adapter, obligation_id}; plus record_obligation, finalize_obligation,
 * release_turn_marker seams, send_with_retry (delivery adapter's), stop_clock
 * (stop_reply_clock), log. Returns NEW {"result": SendResult, "adapter": id}. */
/* PoP: gwb_send_final_ledgered @ gateway/platforms/base.py:send_final_ledgered */
json_t *gwb_send_final_ledgered(json_t *st, const json_t *event, const char *session_key,
                                const char *text_content, const json_t *metadata,
                                const json_t *reply_to, bool is_ephemeral_response,
                                const gwb_seams_t *s);

/* _send_final_text — ledger bracket + message-id owner's ephemeral delete.
 * Seams: ledger seams + record_delivery + schedule_delete
 * (_schedule_ephemeral_delete on the delivery adapter) + reply_anchor. */
/* PoP: gwb_send_final_text @ gateway/platforms/base.py:_send_final_text */
void gwb_send_final_text(json_t *st, const json_t *event, const char *session_key,
                         const char *text_content, const json_t *metadata,
                         bool is_ephemeral_response, int ephemeral_ttl, const gwb_seams_t *s);

/* _notify_turn_error — turn-failure notice.
 * Seams: reply_anchor, scope_resolve, wake_muted (diagnostic_wake_muted),
 * warning_policy, t, send, log. Returns NEW metadata object or NULL. */
/* PoP: gwb_notify_turn_error @ gateway/platforms/base.py:_notify_turn_error */
json_t *gwb_notify_turn_error(json_t *st, const json_t *event, const gwb_seams_t *s);

/* _deliver_attachments — attachments on the CURRENT transport.
 * Seams: final_adapter (_final_delivery_adapter -> adapter id), uniform
 * (human delay), image batch + media dispatch seams, log. */
/* PoP: gwb_deliver_attachments @ gateway/platforms/base.py:_deliver_attachments */
void gwb_deliver_attachments(json_t *st, const json_t *event, const json_t *extracted,
                             const json_t *metadata, bool anything_sent, const gwb_seams_t *s);

/* emit_warning — classified diagnostic presentation.
 * Seams: warning_policy, send. Returns NEW SendResult or NULL (suppressed). */
/* PoP: gwb_emit_warning @ gateway/platforms/base.py:emit_warning */
json_t *gwb_emit_warning(const json_t *st, const char *chat_id, const char *content,
                         const json_t *reply_to, const json_t *metadata,
                         const json_t *logical_platform, const gwb_seams_t *s);

/* emit_media_warning — media diagnostic preserving caption.
 * Seams: warning_policy, send. Returns NEW SendResult. */
/* PoP: gwb_emit_media_warning @ gateway/platforms/base.py:emit_media_warning */
json_t *gwb_emit_media_warning(const json_t *st, const char *chat_id, const char *notice,
                               const char *caption, const json_t *reply_to, const json_t *metadata,
                               const json_t *shown_metadata, const gwb_seams_t *s);

/* _send_media_fallback_notice — shared native-send default.
 * Seams: log, t, warning_policy, send. Returns NEW SendResult. */
/* PoP: gwb_send_media_fallback_notice @ gateway/platforms/base.py:_send_media_fallback_notice */
json_t *gwb_send_media_fallback_notice(const json_t *st, const char *method, const char *kind,
                                       const char *path, const char *chat_id, const char *caption,
                                       const json_t *reply_to, const json_t *metadata,
                                       const char *file_name, const gwb_seams_t *s);

/* send_exec_approval — approval card template method.
 * Seams: ea_default_reason, render (render_exec_approval -> {text, truncated}),
 * card_whole (card_shows_whole_command), send_prompt (_send_exec_approval_prompt),
 * ea_labels. Returns NEW SendResult. */
/* PoP: gwb_send_exec_approval @ gateway/platforms/base.py:send_exec_approval */
json_t *gwb_send_exec_approval(json_t *st, const char *chat_id, const char *command,
                               const char *session_key, const json_t *description,
                               const json_t *metadata, bool allow_permanent, bool allow_session,
                               bool smart_denied, const json_t *request_id, const gwb_seams_t *s);

/* send_clarify — clarify prompt default.
 * Seams: get_pending (tools.clarify_gateway.get_pending -> {multi_select} or
 * raise), t, mark_awaiting (mark_awaiting_text), send. Returns NEW SendResult. */
/* PoP: gwb_send_clarify @ gateway/platforms/base.py:send_clarify */
json_t *gwb_send_clarify(const json_t *st, const char *chat_id, const char *question,
                         const json_t *choices, const char *clarify_id, const char *session_key,
                         const json_t *metadata, const gwb_seams_t *s);

/* build_source — SessionSource construction + profile routing.
 * Seams: configured_bot (configured_bot_user), profile_for_source
 * (runner._profile_name_for_source; str|null or {"__raise__":
 * "ProfileRouteRejected", "profile": ...} / other), log.
 * fields: JSON object of the 17 build_source parameters.
 * Returns NEW source object (field insertion order semantic). */
/* PoP: gwb_build_source @ gateway/platforms/base.py:build_source */
json_t *gwb_build_source(json_t *st, const json_t *fields, const gwb_seams_t *s);

/* _dispatch_inline_reply — handler reply inline (no session lifecycle).
 * Seams: reply_anchor, message_handler, unwrap_ephemeral (_unwrap_ephemeral ->
 * {text, ttl}), send_with_retry, schedule_delete, log. */
/* PoP: gwb_dispatch_inline_reply @ gateway/platforms/base.py:_dispatch_inline_reply */
void gwb_dispatch_inline_reply(json_t *st, const json_t *event, const json_t *log_cmd,
                               const gwb_seams_t *s);

/* Default send delegations (live defaults; seam "send" / "send_image" /
 * "send_voice" / "fallback_notice" stand in for the overridden methods). */
/* PoP: gwb_send_image @ gateway/platforms/base.py:send_image */
json_t *gwb_send_image(const json_t *st, const char *chat_id, const char *image_url,
                       const char *caption, const json_t *reply_to, const json_t *metadata,
                       const gwb_seams_t *s);
/* PoP: gwb_send_animation @ gateway/platforms/base.py:send_animation */
json_t *gwb_send_animation(const json_t *st, const char *chat_id, const char *animation_url,
                           const char *caption, const json_t *reply_to, const json_t *metadata,
                           const gwb_seams_t *s);
/* PoP: gwb_send_voice @ gateway/platforms/base.py:send_voice */
json_t *gwb_send_voice(const json_t *st, const char *chat_id, const char *audio_path,
                       const char *caption, const json_t *reply_to, const json_t *metadata,
                       const gwb_seams_t *s);
/* PoP: gwb_send_video @ gateway/platforms/base.py:send_video */
json_t *gwb_send_video(const json_t *st, const char *chat_id, const char *video_path,
                       const char *caption, const json_t *reply_to, const json_t *metadata,
                       const gwb_seams_t *s);
/* PoP: gwb_send_document @ gateway/platforms/base.py:send_document */
json_t *gwb_send_document(const json_t *st, const char *chat_id, const char *file_path,
                          const char *caption, const char *file_name, const json_t *reply_to,
                          const json_t *metadata, const gwb_seams_t *s);
/* PoP: gwb_send_image_file @ gateway/platforms/base.py:send_image_file */
json_t *gwb_send_image_file(const json_t *st, const char *chat_id, const char *image_path,
                            const char *caption, const json_t *reply_to, const json_t *metadata,
                            const gwb_seams_t *s);
/* PoP: gwb_play_tts @ gateway/platforms/base.py:play_tts */
json_t *gwb_play_tts(const json_t *st, const char *chat_id, const char *audio_path,
                     const json_t *kwargs, const gwb_seams_t *s);
/* PoP: gwb_send_slash_confirm @ gateway/platforms/base.py:send_slash_confirm */
json_t *gwb_send_slash_confirm(const json_t *st, const char *chat_id, const char *title,
                               const char *message, const char *session_key, const char *confirm_id,
                               const json_t *metadata);
/* PoP: gwb_send_exec_approval_prompt_default @ gateway/platforms/base.py:_send_exec_approval_prompt */
json_t *gwb_send_exec_approval_prompt_default(const json_t *st, const json_t *prompt);

#ifdef __cplusplus
}
#endif

#endif /* GATEWAY_BASE_FLOW_H */