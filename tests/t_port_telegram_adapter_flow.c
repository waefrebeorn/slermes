/* t_port_telegram_adapter_flow.c — harness for the Telegram adapter FLOW
 * port (src/gateway/platforms/telegram_adapter_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the adapter state echo ("state"). The echoed "in" is
 * the ORIGINAL input (the oracle re-drives the live reference from it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order, or {"__script__": true, "ret": <value>, "raise": {type,...}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 * in.errors carries the exception-graph registry (id -> class/message/base/
 * cause/context/error_code); in.state the adapter attributes.
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it, avoiding basename collisions).
 */
#include "gateway/platforms/telegram_adapter_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*ta_fn_t)(ta_ctx_t *, const json_t *);
typedef struct { const char *func; ta_fn_t fn; } ta_case_t;

static const ta_case_t CASES[] = {
    { "_metadata_thread_id", ta_metadata_thread_id },
    { "_metadata_direct_messages_topic_id", ta_metadata_direct_messages_topic_id },
    { "_metadata_reply_to_message_id", ta_metadata_reply_to_message_id },
    { "_dm_topic_fallback", ta_dm_topic_fallback },
    { "_is_private_dm_topic_send", ta_is_private_dm_topic_send },
    { "_reply_to_message_id_for_send", ta_reply_to_message_id_for_send },
    { "_message_thread_id_for_send", ta_message_thread_id_for_send },
    { "_message_thread_id_for_typing", ta_message_thread_id_for_typing },
    { "_direct_topic_kwargs", ta_direct_topic_kwargs },
    { "_thread_kwargs_for_send", ta_thread_kwargs_for_send },
    { "_thread_kwargs_for_draft", ta_thread_kwargs_for_draft },
    { "_should_thread_reply", ta_should_thread_reply },
    { "_chunk_reply_routing", ta_chunk_reply_routing },
    { "_compute_single_send_routing", ta_compute_single_send_routing },
    { "_iter_exception_graph", ta_iter_exception_graph },
    { "_exception_graph_matches", ta_exception_graph_matches },
    { "_looks_like_connect_timeout", ta_looks_like_connect_timeout },
    { "_looks_like_pool_timeout", ta_looks_like_pool_timeout },
    { "_is_thread_not_found_error", ta_is_thread_not_found_error },
    { "_is_bad_request_error", ta_is_bad_request_error },
    { "_is_reply_anchor_missing_error", ta_is_reply_anchor_missing_error },
    { "_should_retry_without_dm_topic_reply_anchor", ta_should_retry_without_dm_topic_reply_anchor },
    { "_looks_like_auth_error", ta_looks_like_auth_error },
    { "_looks_like_network_error", ta_looks_like_network_error },
    { "_is_timed_out", ta_is_timed_out },
    { "_is_rich_capability_error", ta_is_rich_capability_error },
    { "_is_rich_fallback_error", ta_is_rich_fallback_error },
    { "_rich_delivery_enabled", ta_rich_delivery_enabled },
    { "_bot_supports_rich", ta_bot_supports_rich },
    { "_needs_rich_rendering", ta_needs_rich_rendering },
    { "_has_telegram_desktop_cjk_rich_garble_shape", ta_has_telegram_desktop_cjk_rich_garble_shape },
    { "_has_telegram_desktop_details_math_crash_shape", ta_has_telegram_desktop_details_math_crash_shape },
    { "_content_fits_rich_limits", ta_content_fits_rich_limits },
    { "_rich_content_ok", ta_rich_content_ok },
    { "_rich_eligible", ta_rich_eligible },
    { "_should_attempt_rich", ta_should_attempt_rich },
    { "prefers_fresh_final_streaming", ta_prefers_fresh_final_streaming },
    { "_rich_transport_available", ta_rich_transport_available },
    { "streaming_overflow_limit", ta_streaming_overflow_limit },
    { "_normalize_chat_type", ta_normalize_chat_type },
    { "_chat_type_str", ta_chat_type_str },
    { "_gateway_chat_type", ta_gateway_chat_type },
    { "_coerce_bool_extra", ta_coerce_bool_extra },
    { "_notification_kwargs", ta_notification_kwargs },
    { "_send_retry_is_final", ta_send_retry_is_final },
};
#define CASE_COUNT (sizeof(CASES) / sizeof(CASES[0]))

static char *read_line(FILE *f) {
    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    int ch;
    while ((ch = fgetc(f)) != EOF && ch != '\n') {
        if (len + 1 >= cap) {
            cap *= 2;
            char *nb = realloc(buf, cap);
            if (!nb) { free(buf); return NULL; }
            buf = nb;
        }
        buf[len++] = (char)ch;
    }
    if (ch == EOF && len == 0) { free(buf); return NULL; }
    buf[len] = '\0';
    return buf;
}

int main(void) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    json_t *empty_args = json_object();
    char *line;
    while ((line = read_line(stdin)) != NULL) {
        if (!line[0]) { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec || rec->type != JSON_OBJECT) { if (rec) json_free(rec); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        if (!in || in->type != JSON_OBJECT) in = NULL;
        json_t *out = json_object();

        const ta_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            ta_ctx_t ctx;
            ctx.state = (state && state->type == JSON_OBJECT) ? json_copy(state) : json_object();
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.cursors = json_object();
            ctx.trace = json_array();
            ctx.errors = in ? json_obj_get(in, "errors") : NULL;
            json_t *ret = tc->fn(&ctx, args ? args : empty_args);
            json_set(out, "ret", ret ? ret : json_null());
            json_set(out, "trace", ctx.trace);
            json_set(out, "state", ctx.state);
            json_free(ctx.cursors);
        }
        json_t *echo = json_object();
        json_set_hdr(echo, "func", func);
        json_set(echo, "in", in ? json_copy(in) : json_null());
        json_set(echo, "out", out);
        char *s = json_serialize(echo);
        if (s) { printf("%s\n", s); free(s); }
        json_free(echo);
        json_free(rec);
    }
    json_free(empty_args);
    return 0;
}