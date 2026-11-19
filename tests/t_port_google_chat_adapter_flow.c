/* t_port_google_chat_adapter_flow.c — harness for the Google Chat adapter
 * FLOW port (src/gateway/platforms/google_chat_adapter_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {"args": {...},
 * "state": {...}, "io": {...}}}. Emits JSON LINES: {"func", "in", "out"}
 * where out captures the decision SEQUENCE (ordered seam events
 * {"name","args","ret"} in "trace"), the final return ("ret"), and the
 * mutated adapter state ("state"). The echoed "in" is the ORIGINAL input
 * (copied BEFORE dispatch); the oracle re-drives the live reference from it.
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order, or {"__script__": true, "ret": <value>, "raise": {type,...}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it, avoiding basename collisions).
 */
#include "gateway/platforms/google_chat_adapter_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*gca_fn_t)(gca_ctx_t *, const json_t *);
typedef struct { const char *func; gca_fn_t fn; } gca_case_t;

static const gca_case_t CASES[] = {
    { "_thread_body", gca__thread_body },
    { "_create_kwargs", gca__create_kwargs },
    { "_mime_for_message_type", gca__mime_for_message_type },
    { "_http_status", gca__http_status },
    { "_is_retryable_error", gca__is_retryable_error },
    { "_redact_sensitive", gca__redact_sensitive },
    { "_proxy_error", gca__proxy_error },
    { "_exc_text", gca__exc_text },
    { "_is_google_owned_host", gca__is_google_owned_host },
    { "_adc_would_borrow_foreign_credentials", gca__adc_would_borrow_foreign_credentials },
    { "_validate_config", gca__validate_config },
    { "_is_connected", gca__is_connected },
    { "_env_inbound_settings", gca__env_inbound_settings },
    { "_env_inbound_configured", gca__env_inbound_configured },
    { "check_google_chat_requirements", gca_check_google_chat_requirements },
    { "_check_for_registry", gca__check_for_registry },
    { "_env_enablement", gca__env_enablement },
    { "_standalone_error", gca__standalone_error },
    { "_int_setting", gca__int_setting },
    { "_str_setting", gca__str_setting },
    { "_extract_message_payload", gca__extract_message_payload },
    { "_prepare_inbound", gca__prepare_inbound },
    { "_resolve_thread_id", gca__resolve_thread_id },
    { "_note_rate_limit", gca__note_rate_limit },
    { "_invalidate_user_creds", gca__invalidate_user_creds },
    { "stop_typing", gca_stop_typing },
    { "_track_outbound_thread", gca__track_outbound_thread },
    { "_release_unadmitted", gca__release_unadmitted },
    { "_connect_failed", gca__connect_failed },
    { "_chunk_text", gca__chunk_text },
    { "verify_http_event_request", gca_verify_http_event_request },
    { "_load_cached_bot_id", gca__load_cached_bot_id },
    { "_log_background_failure", gca__log_background_failure },
    { "_loop_accepts_callbacks", gca__loop_accepts_callbacks },
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
        /* echo the ORIGINAL input before dispatch mutates state */
        json_t *echo_in = in ? json_copy(in) : json_null();
        json_t *out = json_object();

        const gca_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            gca_ctx_t ctx;
            ctx.state = (state && state->type == JSON_OBJECT) ? json_copy(state) : json_object();
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.cursors = json_object();
            ctx.trace = json_array();
            json_t *ret = tc->fn(&ctx, args ? args : empty_args);
            json_set(out, "ret", ret ? ret : json_null());
            json_set(out, "trace", ctx.trace);
            json_set(out, "state", ctx.state);
            json_free(ctx.cursors);
        }
        json_t *echo = json_object();
        json_set_hdr(echo, "func", func);
        json_set(echo, "in", echo_in);
        json_set(echo, "out", out);
        char *s = json_serialize(echo);
        if (s) { printf("%s\n", s); free(s); }
        json_free(echo);
        json_free(rec);
    }
    json_free(empty_args);
    return 0;
}