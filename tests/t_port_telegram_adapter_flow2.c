/* t_port_telegram_adapter_flow2.c — harness for the Telegram adapter FLOW
 * remainder port (src/gateway/platforms/telegram_adapter_flow2.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the MUTATED adapter state echo ("state"). The echoed
 * "in" is the ORIGINAL input (the oracle re-drives the live reference from it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order, or {"__script__": true, "ret": <value>, "raise": {type,...}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 * in.errors carries the exception-graph registry; in.state the adapter
 * attributes (mutated in place by the ported flows).
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it, avoiding basename collisions).
 */
#include "gateway/platforms/telegram_adapter_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*ta2_fn_t)(ta2_ctx_t *, const json_t *);
typedef struct { const char *func; ta2_fn_t fn; } ta2_case_t;

static const ta2_case_t CASES[] = {
    { "_fence_polling", ta2_fence_polling },
    { "_begin_polling_generation", ta2_begin_polling_generation },
    { "_record_polling_progress", ta2_record_polling_progress },
    { "_observe_polling_request_result", ta2_observe_polling_request_result },
    { "_record_updates_received", ta2_record_updates_received },
    { "_verifier_stale", ta2_verifier_stale },
    { "_recovery_in_flight", ta2_recovery_in_flight },
    { "_check_ingress_dispatch_stall", ta2_check_ingress_dispatch_stall },
    { "_current_bot_username", ta2_current_bot_username },
    { "_note_bot_username", ta2_note_bot_username },
    { "_observe_bot_identity_from_message", ta2_observe_bot_identity_from_message },
    { "_bot_identity_is_fresh", ta2_bot_identity_is_fresh },
    { "_is_reply_to_bot", ta2_is_reply_to_bot },
    { "_effective_message_thread_id", ta2_effective_message_thread_id },
    { "_extra_bool", ta2_extra_bool },
    { "_extra_str_set", ta2_extra_str_set },
    { "_telegram_ignored_threads", ta2_telegram_ignored_threads },
    { "_telegram_observe_allowed_chats", ta2_telegram_observe_allowed_chats },
    { "_telegram_is_free_response_topic", ta2_telegram_is_free_response_topic },
    { "_int_or_zero", ta2_int_or_zero },
    { "_telegram_media_too_large_note", ta2_telegram_media_too_large_note },
    { "_telegram_media_size_allowed", ta2_telegram_media_size_allowed },
    { "_missing_media_path_error", ta2_missing_media_path_error },
    { "_media_send_kwargs", ta2_media_send_kwargs },
    { "_sniff_raster_format", ta2_sniff_raster_format },
    { "_cold_boot_drop_pending", ta2_cold_boot_drop_pending },
    { "_looks_like_polling_conflict", ta2_looks_like_polling_conflict },
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

        const ta2_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            ta2_ctx_t ctx;
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
