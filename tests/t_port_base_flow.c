/*
 * t_port_base_flow.c — oracle harness for the platform-adapter base FLOW
 * cores ported in src/gateway/platforms/base_flow.c.
 *
 * Self-contained: includes the port .c by bare name. Reads embedded JSON-line
 * cases (tests/base_flow_cases.h), wires the universal seam to the SCRIPTED
 * results in each case's "seams" object (a recording stub appending one trace
 * event per seam call, in call order), and emits
 * {"func":..., "in":..., "out":{"ret":..., "trace":[...], "st":..., "ev":...}}.
 * tests/oracle_base_flow.py drives the LIVE Python with the same scripts.
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "base_flow.c"
#include "base_flow_cases.h"

typedef struct {
    const json_t *in;
    gwb_trace_t *trace;
    json_t *counters;   /* name -> call index */
    json_t *st;         /* adapter state for seam side-effects (none needed) */
} script_ctx_t;

static json_t *args_drop_null(const json_t *args) {
    json_t *out = json_object();
    if (args && args->type == JSON_OBJECT) {
        for (size_t i = 0; i < args->c.count; i++) {
            const json_t *v = args->c.items[i];
            if (v && v->type == JSON_NULL) continue;
            json_set(out, args->c.keys[i], json_copy(v));
        }
    }
    return out;
}

/* Universal seam: consume the scripted result, record {ev,args,ret}, return it. */
static json_t *script_call(void *vctx, const char *name, const json_t *args) {
    script_ctx_t *ctx = vctx;
    const json_t *seams = json_obj_get(ctx->in, "seams");
    const json_t *sc = json_obj_get(seams, name);
    json_t *result = NULL;
    if (sc && sc->type == JSON_ARRAY) {
        double idx = json_get_num(ctx->counters, name, 0);
        size_t i = (size_t)idx;
        size_t n = json_len(sc);
        if (n > 0) result = json_copy(json_get(sc, i < n ? i : n - 1));
        json_set(ctx->counters, name, json_number(idx + 1));
    } else if (sc) {
        result = json_copy(sc);
    }
    json_t *e = json_object();
    json_set(e, "ev", json_string(name));
    json_set(e, "args", args_drop_null(args));
    json_set(e, "ret", result ? json_copy(result) : json_null());
    gwb_trace_add(ctx->trace, e);
    /* Input-echoing seams: the recorded ret stays the script marker, but the
     * value returned to the core echoes the argument (identity escape / len). */
    if (result && result->type == JSON_OBJECT) {
        if (!strcmp(name, "ea_escape") && json_get_bool(result, "identity", false)) {
            json_free(result);
            return json_copy(json_obj_get(args, "text"));
        }
        if (!strcmp(name, "measure") && json_get_bool(result, "len", false)) {
            json_free(result);
            return json_number((double)cp_len(jstr(json_obj_get(args, "s"))));
        }
    }
    return result;
}

/* typed callbacks */
static json_t *thunk_cb(void *vctx, int *exc) {
    script_ctx_t *ctx = vctx;
    const json_t *th = json_obj_get(ctx->in, "thunk");
    *exc = (int)json_get_num(th, "exc", 0);
    return json_copy(json_obj_get(th, "value"));
}
static int measure_cb(void *vctx, const char *s) {
    script_ctx_t *ctx = vctx;
    json_t *a = json_object();
    json_set(a, "s", json_string(s ? s : ""));
    json_t *r = script_call(ctx, "measure", a);
    json_free(a);
    int u = r && r->type == JSON_NUMBER ? (int)r->num_val : 0;
    json_free(r);
    return u;
}
static double uniform_cb(void *vctx, double lo, double hi) {
    script_ctx_t *ctx = vctx;
    json_t *a = json_object();
    json_set(a, "lo", json_number(lo));
    json_set(a, "hi", json_number(hi));
    json_t *r = script_call(ctx, "uniform", a);
    json_free(a);
    double v = r && r->type == JSON_NUMBER ? r->num_val : 0.0;
    json_free(r);
    return v;
}

#define S(k) json_get_str(in, k, NULL)
#define J(k) json_obj_get(in, k)
#define B(k, d) json_get_bool(in, k, d)
#define N(k, d) json_get_num(in, k, d)
#define I(k, d) ((int)json_get_num(in, k, d))

static void ret_str(json_t *out, char *s) {
    json_set(out, "ret", s ? json_string(s) : json_null());
    free(s);
}
static void ret_json(json_t *out, json_t *r) { json_set(out, "ret", r ? r : json_null()); }
static void ret_bool(json_t *out, bool b) { json_set(out, "ret", json_bool(b)); }
static void ret_num(json_t *out, double d) { json_set(out, "ret", json_number(d)); }

static void run_case(const char *func, const json_t *in, json_t *out) {
    json_t *st = json_copy(J("st"));
    json_t *event = json_copy(J("event"));
    gwb_trace_t trace = {NULL};
    json_t *counters = json_object();
    script_ctx_t ctx = {in, &trace, counters, st};
    gwb_seams_t seams = {&ctx, script_call};
    const gwb_seams_t *sp = &seams;

    if (!strcmp(func, "or_default")) {
        int out_exc = 0;
        json_t *r = gwb_or_default(thunk_cb, &ctx, J("default"), I("exc_mask", 3), &out_exc);
        json_t *o = json_object();
        json_set(o, "value", r ? r : json_null());
        json_set(o, "exc", json_number(out_exc));
        ret_json(out, o);
    } else if (!strcmp(func, "thread_metadata_for_source")) {
        ret_json(out, gwb_thread_metadata_for_source(J("source"), S("reply_to")));
    } else if (!strcmp(func, "thread_metadata_for_event")) {
        ret_json(out, gwb_thread_metadata_for_event(event, sp));
    } else if (!strcmp(func, "mark_notify_metadata")) {
        ret_json(out, gwb_mark_notify_metadata(J("metadata")));
    } else if (!strcmp(func, "media_failure_text")) {
        ret_str(out, gwb_media_failure_text(S("kind"), S("file_name"), sp));
    } else if (!strcmp(func, "should_send_media_as_audio")) {
        ret_bool(out, gwb_should_send_media_as_audio(S("platform"), S("ext"), B("is_voice", false)));
    } else if (!strcmp(func, "custom_unit_to_cp")) {
        ret_num(out, gwb_custom_unit_to_cp(S("s"), I("budget", 0), measure_cb, &ctx));
    } else if (!strcmp(func, "fit_prefix")) {
        ret_str(out, gwb_fit_prefix(S("text"), I("budget", 0), measure_cb, &ctx, S("suffix")));
    } else if (!strcmp(func, "prefix_within_utf16_limit")) {
        ret_str(out, gwb_prefix_within_utf16_limit(S("s"), I("limit", 0)));
    } else if (!strcmp(func, "has_media_directives")) {
        ret_bool(out, gwb_has_media_directives(S("text")));
    } else if (!strcmp(func, "terminal_sentinel_start")) {
        ret_num(out, gwb_terminal_sentinel_start(S("text")));
    } else if (!strcmp(func, "code_spans")) {
        ret_json(out, gwb_code_spans(S("content")));
    } else if (!strcmp(func, "blank_spans")) {
        ret_str(out, gwb_blank_spans(S("text"), J("spans")));
    } else if (!strcmp(func, "delete_spans")) {
        ret_str(out, gwb_delete_spans(S("text"), J("spans")));
    } else if (!strcmp(func, "append_text")) {
        ret_str(out, gwb_append_text(S("existing"), S("new")));
    } else if (!strcmp(func, "any_in")) {
        const json_t *nd = J("needles");
        size_t n = json_len(nd);
        const char **needles = malloc((n + 1) * sizeof(char *));
        for (size_t i = 0; i < n; i++) needles[i] = jstr(json_get(nd, i));
        ret_bool(out, gwb_any_in(S("blob"), needles, n));
        free(needles);
    } else if (!strcmp(func, "classify_send_error")) {
        ret_str(out, strdup(gwb_classify_send_error(J("exc"), S("error_text"))));
    } else if (!strcmp(func, "split_post_delivery_entry")) {
        ret_json(out, gwb_split_post_delivery_entry(J("entry")));
    } else if (!strcmp(func, "extract_images")) {
        ret_json(out, gwb_extract_images(S("content")));
    } else if (!strcmp(func, "coerce_float_extra")) {
        bool has_max = json_has(in, "max_value") && J("max_value")->type != JSON_NULL;
        ret_num(out, gwb_coerce_float_extra(J("value"), N("default", 0.0), N("min_value", 0.0),
                                            has_max, N("max_value", 0.0), S("key"), sp));
    } else if (!strcmp(func, "configure_text_batch_delays")) {
        gwb_configure_text_batch_delays(st, sp);
        ret_json(out, NULL);
    } else if (!strcmp(func, "text_batch_delay_for")) {
        ret_num(out, gwb_text_batch_delay_for(st, J("pending")));
    } else if (!strcmp(func, "owner_transport_profile")) {
        const char *r = gwb_owner_transport_profile(st);
        ret_str(out, r ? strdup(r) : NULL);
    } else if (!strcmp(func, "is_rate_limited_error")) {
        ret_bool(out, gwb_is_rate_limited_error(S("error")));
    } else if (!strcmp(func, "is_timeout_error")) {
        ret_bool(out, gwb_is_timeout_error(S("error")));
    } else if (!strcmp(func, "send_retry_is_final")) {
        ret_bool(out, gwb_send_retry_is_final(J("result")));
    } else if (!strcmp(func, "is_partial_delivery")) {
        ret_bool(out, gwb_is_partial_delivery(J("result")));
    } else if (!strcmp(func, "merge_caption")) {
        ret_str(out, gwb_merge_caption(S("existing"), S("new")));
    } else if (!strcmp(func, "requeue_backoff_delay")) {
        ret_num(out, gwb_requeue_backoff_delay(st, S("session_key"), J("pending"), J("dispatched"), sp));
    } else if (!strcmp(func, "get_human_delay")) {
        ret_num(out, gwb_get_human_delay(st, uniform_cb, &ctx));
    } else if (!strcmp(func, "wants_auto_tts")) {
        ret_bool(out, gwb_wants_auto_tts(st, event, S("session_key"), J("interrupt_event"),
                                         S("text_content"), J("media_files"), sp));
    } else if (!strcmp(func, "shared_runtime_owns_input")) {
        ret_bool(out, gwb_shared_runtime_owns_input(st, event, sp));
    } else if (!strcmp(func, "clear_session_guard")) {
        gwb_clear_session_guard(st, S("session_key"));
        ret_json(out, NULL);
    } else if (!strcmp(func, "warning_notifications_enabled")) {
        ret_bool(out, gwb_warning_notifications_enabled(st, J("logical_platform"), sp));
    } else if (!strcmp(func, "warning_text")) {
        const char *r = gwb_warning_text(st, S("visible"), S("hidden"), J("logical_platform"),
                                         J("chat_id"), J("metadata"), sp);
        ret_str(out, r ? strdup(r) : NULL);
    } else if (!strcmp(func, "exec_approval_actions")) {
        ret_json(out, gwb_exec_approval_actions(B("allow_permanent", true), B("allow_session", true),
                                                B("smart_denied", false), sp));
    } else if (!strcmp(func, "exec_approval_prompt_choices")) {
        ret_json(out, gwb_exec_approval_prompt_choices(J("actions")));
    } else if (!strcmp(func, "text_debounce_cancel_timer")) {
        json_t *state = json_copy(J("state"));
        gwb_text_debounce_cancel_timer(state, json_has(in, "unless"), I("unless_id", -1));
        ret_json(out, NULL);
        json_set(out, "state", json_copy(json_obj_get(state, "task")));
        json_free(state);
    } else if (!strcmp(func, "unauthorized_action_notice")) {
        ret_str(out, gwb_unauthorized_action_notice(J("platform"), sp));
    } else if (!strcmp(func, "get_inbound_media_max_bytes")) {
        ret_num(out, (double)gwb_get_inbound_media_max_bytes(sp));
    } else if (!strcmp(func, "validate_inbound_media_size")) {
        ret_str(out, gwb_validate_inbound_media_size((long long)N("size", 0), S("media_type"),
                                                     J("max_bytes"), sp));
    } else if (!strcmp(func, "read_httpx_body_with_limit")) {
        ret_json(out, gwb_read_httpx_body_with_limit(J("headers"), J("chunks"), S("media_type"), sp));
    } else if (!strcmp(func, "is_network_accessible")) {
        ret_bool(out, gwb_is_network_accessible(S("host"), sp));
    } else if (!strcmp(func, "detect_macos_system_proxy")) {
        json_t *cache = json_copy(J("cache"));
        char *r = gwb_detect_macos_system_proxy(cache, sp);
        ret_str(out, r);
        json_set(out, "cache", cache ? cache : json_null());
    } else if (!strcmp(func, "reset_macos_proxy_cache")) {
        json_t *cache = json_copy(J("cache"));
        gwb_reset_macos_proxy_cache(&cache);
        ret_json(out, NULL);
        json_set(out, "cache", cache ? cache : json_null());
    } else if (!strcmp(func, "resolve_proxy_url")) {
        ret_str(out, gwb_resolve_proxy_url(S("platform_env_var"), J("target_hosts"), S("configured"), sp));
    } else if (!strcmp(func, "config_section")) {
        ret_json(out, gwb_config_section(S("name"), sp));
    } else if (!strcmp(func, "proxy_kwargs_for_aiohttp")) {
        ret_json(out, gwb_proxy_kwargs_for_aiohttp(S("proxy_url"), sp));
    } else if (!strcmp(func, "aiohttp_socks_connector")) {
        ret_json(out, gwb_aiohttp_socks_connector(S("proxy_url"), sp));
    } else if (!strcmp(func, "should_bypass_proxy")) {
        ret_bool(out, gwb_should_bypass_proxy(J("target_hosts"), sp));
    } else if (!strcmp(func, "is_host_excluded_by_no_proxy")) {
        ret_bool(out, gwb_is_host_excluded_by_no_proxy(S("hostname"), J("no_proxy_value"), sp));
    } else if (!strcmp(func, "format_message")) {
        ret_str(out, gwb_format_message(S("content")));
    } else if (!strcmp(func, "message_len_fn")) {
        ret_num(out, gwb_message_len_fn(S("s")));
    } else if (!strcmp(func, "supports_draft_streaming")) {
        ret_bool(out, gwb_supports_draft_streaming(J("chat_type"), J("metadata"), J("chat_id")));
    } else if (!strcmp(func, "send_draft")) {
        ret_str(out, gwb_send_draft(S("class_name")));
    } else if (!strcmp(func, "mark_degraded")) {
        gwb_mark_degraded(st, sp);
        ret_json(out, NULL);
    } else if (!strcmp(func, "lazy_attr")) {
        ret_json(out, gwb_lazy_attr(st, S("name"), sp));
    } else if (!strcmp(func, "accepts_kwarg")) {
        ret_bool(out, gwb_accepts_kwarg(J("params"), S("name"), B("var_kw", false), B("unknown", false)));
    } else if (!strcmp(func, "prepare_tts_text")) {
        ret_str(out, gwb_prepare_tts_text(S("text"), sp));
    } else if (!strcmp(func, "media_delivery_scope")) {
        ret_json(out, gwb_media_delivery_scope(st, J("source"), sp));
    } else if (!strcmp(func, "pending_recovery_owners")) {
        ret_json(out, gwb_pending_recovery_owners(st, event, S("session_key"), sp));
    } else if (!strcmp(func, "track_session_task")) {
        json_t *task = json_copy(J("task"));
        bool r = gwb_track_session_task(st, S("session_key"), task);
        ret_bool(out, r);
        json_free(task);
    } else if (!strcmp(func, "stop_typing_quietly")) {
        gwb_stop_typing_quietly(S("chat_id"), J("metadata"), sp);
        ret_json(out, NULL);
    } else if (!strcmp(func, "start_typing_refresh")) {
        ret_json(out, gwb_start_typing_refresh(st, event, J("interrupt_event"), J("metadata"), sp));
    } else if (!strcmp(func, "chain_callbacks")) {
        gwb_chain_callbacks((size_t)I("n", 0), sp);
        ret_json(out, NULL);
    } else if (!strcmp(func, "send_plain_fallback")) {
        ret_json(out, gwb_send_plain_fallback(st, S("chat_id"), S("content"), J("reply_to"), J("metadata"), sp));
    } else if (!strcmp(func, "resume_partial_send")) {
        ret_json(out, gwb_resume_partial_send(st, S("chat_id"), J("result"), J("reply_to"), J("metadata")));
    } else if (!strcmp(func, "play_tts_file")) {
        ret_bool(out, gwb_play_tts_file(st, event, S("text_content"), S("tts_path"), B("first", false),
                                        J("metadata"), sp));
    } else if (!strcmp(func, "record_delivery_obligation")) {
        ret_json(out, gwb_record_delivery_obligation(st, event, S("session_key"), S("text_content"),
                                                     J("delivery_adapter"), B("is_ephemeral_response", false), sp));
    } else if (!strcmp(func, "finalize_delivery_obligation")) {
        gwb_finalize_delivery_obligation(st, S("obligation_id"), J("result"), event, J("delivery_adapter"), sp);
        ret_json(out, NULL);
    } else if (!strcmp(func, "fire_post_delivery_callback")) {
        gwb_fire_post_delivery_callback(st, S("session_key"), J("generation"), sp);
        ret_json(out, NULL);
    } else if (!strcmp(func, "release_turn_marker")) {
        gwb_release_turn_marker(st, event, sp);
        ret_json(out, NULL);
    } else if (!strcmp(func, "ledger_attempted_final")) {
        ret_bool(out, gwb_ledger_attempted_final(st, event, S("session_key"), S("response"), S("error"), sp));
    } else if (!strcmp(func, "session_key_profile")) {
        ret_json(out, gwb_session_key_profile(st, J("source"), sp));
    } else if (!strcmp(func, "canonicalize")) {
        ret_json(out, gwb_canonicalize(st, J("source"), sp));
    } else if (!strcmp(func, "drop_unresolved")) {
        ret_bool(out, gwb_drop_unresolved(st, event, sp));
    } else if (!strcmp(func, "ea_fit")) {
        ret_str(out, gwb_ea_fit(S("text"), I("budget", 0), S("suffix"), sp));
    } else if (!strcmp(func, "ea_deadline_line")) {
        ret_str(out, gwb_ea_deadline_line(st, sp));
    } else if (!strcmp(func, "exec_approval_cmd_budget")) {
        ret_num(out, gwb_exec_approval_cmd_budget(st, S("description"), B("smart_denied", false), sp));
    } else if (!strcmp(func, "supports_exec_approval_buttons")) {
        ret_bool(out, gwb_supports_exec_approval_buttons(B("overrides_prompt", false)));
    } else if (!strcmp(func, "is_queue_text_debounce_candidate")) {
        ret_bool(out, gwb_is_queue_text_debounce_candidate(st, event, sp));
    } else if (!strcmp(func, "event_session_key")) {
        ret_str(out, gwb_event_session_key(st, event, sp));
    } else if (!strcmp(func, "source_session_key")) {
        ret_str(out, gwb_source_session_key(st, J("source"), sp));
    } else if (!strcmp(func, "text_batch_key")) {
        ret_str(out, gwb_text_batch_key(st, event, sp));
    } else if (!strcmp(func, "pop_text_batch")) {
        ret_json(out, gwb_pop_text_batch(st, S("key")));
    } else if (!strcmp(func, "dispatch_text_batch")) {
        gwb_dispatch_text_batch(st, event, sp);
        ret_json(out, NULL);
    } else if (!strcmp(func, "handle_message_while_active")) {
        ret_json(out, gwb_handle_message_while_active(st, event, S("session_key"), sp));
    } else if (!strcmp(func, "deliver_media_attachments")) {
        gwb_deliver_media_attachments(st, event, J("media_files"), J("local_files"),
                                      B("force_document_attachments", false), N("human_delay", 0.0),
                                      J("metadata"), sp);
        ret_json(out, NULL);
    } else if (!strcmp(func, "send_image_batch")) {
        gwb_send_image_batch(st, event, J("images"), J("metadata"), N("human_delay", 0.0), sp);
        ret_json(out, NULL);
    } else if (!strcmp(func, "acquire_platform_lock")) {
        ret_bool(out, gwb_acquire_platform_lock(st, S("scope"), S("identity"), S("resource_desc"), sp));
    } else if (!strcmp(func, "release_platform_lock")) {
        gwb_release_platform_lock(st, sp);
        ret_json(out, NULL);
    } else if (!strcmp(func, "wire_plugin_handlers")) {
        gwb_wire_plugin_handlers(st, J("native_id"), sp);
        ret_json(out, NULL);
    } else if (!strcmp(func, "rewire_plugin_handlers")) {
        gwb_rewire_plugin_handlers(st, sp);
        ret_json(out, NULL);
    } else if (!strcmp(func, "enqueue_text_event")) {
        gwb_enqueue_text_event(st, event, sp);
        ret_json(out, NULL);
    } else if (!strcmp(func, "flush_text_batch")) {
        ret_json(out, gwb_flush_text_batch(st, S("key"), I("current_task_id", -1), sp));
    } else if (!strcmp(func, "flush_text_batch_now")) {
        gwb_flush_text_batch_now(st, S("key"), sp);
        ret_json(out, NULL);
    } else if (!strcmp(func, "finish_session_task")) {
        gwb_finish_session_task(st, S("session_key"), I("current_task_id", -1), J("interrupt_event"), sp);
        ret_json(out, NULL);
    } else if (!strcmp(func, "spawn_drain_task")) {
        gwb_spawn_drain_task(st, J("pending_event"), S("session_key"), N("delay", 0.0), sp);
        ret_json(out, NULL);
    } else if (!strcmp(func, "drain_after")) {
        gwb_drain_after(st, J("pending_event"), S("session_key"), N("delay", 0.0), J("guard"), sp);
        ret_json(out, NULL);
    } else if (!strcmp(func, "send_final_ledgered")) {
        ret_json(out, gwb_send_final_ledgered(st, event, S("session_key"), S("text_content"), J("metadata"),
                                              J("reply_to"), B("is_ephemeral_response", false), sp));
    } else if (!strcmp(func, "send_final_text")) {
        gwb_send_final_text(st, event, S("session_key"), S("text_content"), J("metadata"),
                            B("is_ephemeral_response", false), I("ephemeral_ttl", 0), sp);
        ret_json(out, NULL);
    } else if (!strcmp(func, "notify_turn_error")) {
        ret_json(out, gwb_notify_turn_error(st, event, sp));
    } else if (!strcmp(func, "deliver_attachments")) {
        gwb_deliver_attachments(st, event, J("extracted"), J("metadata"), B("anything_sent", false), sp);
        ret_json(out, NULL);
    } else if (!strcmp(func, "emit_warning")) {
        ret_json(out, gwb_emit_warning(st, S("chat_id"), S("content"), J("reply_to"), J("metadata"),
                                       J("logical_platform"), sp));
    } else if (!strcmp(func, "emit_media_warning")) {
        ret_json(out, gwb_emit_media_warning(st, S("chat_id"), S("notice"), S("caption"), J("reply_to"),
                                             J("metadata"), J("shown_metadata"), sp));
    } else if (!strcmp(func, "send_media_fallback_notice")) {
        ret_json(out, gwb_send_media_fallback_notice(st, S("method"), S("kind"), S("path"), S("chat_id"),
                                                     S("caption"), J("reply_to"), J("metadata"), S("file_name"), sp));
    } else if (!strcmp(func, "send_exec_approval")) {
        ret_json(out, gwb_send_exec_approval(st, S("chat_id"), S("command"), S("session_key"), J("description"),
                                             J("metadata"), B("allow_permanent", true), B("allow_session", true),
                                             B("smart_denied", false), J("request_id"), sp));
    } else if (!strcmp(func, "send_clarify")) {
        ret_json(out, gwb_send_clarify(st, S("chat_id"), S("question"), J("choices"), S("clarify_id"),
                                       S("session_key"), J("metadata"), sp));
    } else if (!strcmp(func, "build_source")) {
        ret_json(out, gwb_build_source(st, J("fields"), sp));
    } else if (!strcmp(func, "dispatch_inline_reply")) {
        gwb_dispatch_inline_reply(st, event, J("log_cmd"), sp);
        ret_json(out, NULL);
    } else if (!strcmp(func, "send_image")) {
        ret_json(out, gwb_send_image(st, S("chat_id"), S("image_url"), S("caption"), J("reply_to"), J("metadata"), sp));
    } else if (!strcmp(func, "send_animation")) {
        ret_json(out, gwb_send_animation(st, S("chat_id"), S("animation_url"), S("caption"), J("reply_to"), J("metadata"), sp));
    } else if (!strcmp(func, "send_voice")) {
        ret_json(out, gwb_send_voice(st, S("chat_id"), S("audio_path"), S("caption"), J("reply_to"), J("metadata"), sp));
    } else if (!strcmp(func, "send_video")) {
        ret_json(out, gwb_send_video(st, S("chat_id"), S("video_path"), S("caption"), J("reply_to"), J("metadata"), sp));
    } else if (!strcmp(func, "send_document")) {
        ret_json(out, gwb_send_document(st, S("chat_id"), S("file_path"), S("caption"), S("file_name"),
                                        J("reply_to"), J("metadata"), sp));
    } else if (!strcmp(func, "send_image_file")) {
        ret_json(out, gwb_send_image_file(st, S("chat_id"), S("image_path"), S("caption"), J("reply_to"), J("metadata"), sp));
    } else if (!strcmp(func, "play_tts")) {
        ret_json(out, gwb_play_tts(st, S("chat_id"), S("audio_path"), J("kwargs"), sp));
    } else if (!strcmp(func, "send_slash_confirm")) {
        ret_json(out, gwb_send_slash_confirm(st, S("chat_id"), S("title"), S("message"), S("session_key"),
                                             S("confirm_id"), J("metadata")));
    } else if (!strcmp(func, "send_exec_approval_prompt_default")) {
        ret_json(out, gwb_send_exec_approval_prompt_default(st, J("prompt")));
    } else {
        fprintf(stderr, "unknown func: %s\n", func);
        ret_json(out, NULL);
    }

    json_set(out, "trace", trace.events ? json_copy(trace.events) : json_array());
    if (json_has(in, "st")) json_set(out, "st", st ? st : json_object());
    if (json_has(in, "event")) json_set(out, "ev", event ? event : json_object());
    gwb_trace_free(&trace);
    json_free(counters);
    json_free(st);
    json_free(event);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    for (size_t i = 0; i < sizeof(BF_CASES) / sizeof(BF_CASES[0]); i++) {
        char *err = NULL;
        json_t *c = json_parse(BF_CASES[i], &err);
        if (err || !c || c->type != JSON_OBJECT) {
            fprintf(stderr, "bad case %zu: %s\n", i, err ? err : "not object");
            free(err);
            json_free(c);
            continue;
        }
        const char *func = json_get_str(c, "func", "");
        const json_t *in = json_obj_get(c, "in");
        json_t *empty = json_object();
        json_t *out = json_object();
        run_case(func, in ? in : empty, out);
        json_free(empty);
        char *in_s = json_serialize(in);
        char *out_s = json_serialize(out);
        char *func_s = json_escape_string(func);
        printf("{\"func\":%s,\"in\":%s,\"out\":%s}\n", func_s,
               in_s ? in_s : "null", out_s ? out_s : "null");
        free(in_s); free(out_s); free(func_s);
        json_free(out);
        json_free(c);
    }
    return 0;
}