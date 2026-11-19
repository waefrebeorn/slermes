/* t_port_photon_adapter_flow.c — harness for the Photon adapter FLOW port
 * (src/gateway/platforms/photon_adapter_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {"args","state","io"}}.
 * Emits JSON LINES: {"func","in","out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the adapter state echo ("state" — ctx.state, mutated in
 * place by the state helpers; module-function cases pass state {} so emit {}).
 * The echoed "in" is the ORIGINAL input (the oracle re-drives live from it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order, or {"__script__": true, "ret": <value>, "raise": {type,...}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it, avoiding basename collisions).
 */
#include "gateway/platforms/photon_adapter_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*ph_fn_t)(ph_ctx_t *, const json_t *);
typedef struct { const char *func; ph_fn_t fn; } ph_case_t;

static const ph_case_t CASES[] = {
    { "_normalize_content", ph__normalize_content },
    { "_normalize_group_content", ph__normalize_group_content },
    { "_normalize_binary_payload", ph__normalize_binary_payload },
    { "_is_voice_payload", ph__is_voice_payload },
    { "_cached_media_type", ph__cached_media_type },
    { "_attachment_message_type", ph__attachment_message_type },
    { "_format_richlink_content", ph__format_richlink_content },
    { "_group_item_contents", ph__group_item_contents },
    { "_reply_inner", ph__reply_inner },
    { "_carries_voice", ph__carries_voice },
    { "_mention_gate_text", ph__mention_gate_text },
    { "_richlink_url_from_content", ph__richlink_url_from_content },
    { "_url_only_candidate", ph__url_only_candidate },
    { "_is_richlink_preview_attachment", ph__is_richlink_preview_attachment },
    { "_richlink_preview_label", ph__richlink_preview_label },
    { "_is_richlink_preview_content", ph__is_richlink_preview_content },
    { "_markdown_enabled", ph__markdown_enabled },
    { "_richlink_candidate", ph__richlink_candidate },
    { "_sidecar_payload_text", ph__sidecar_payload_text },
    { "_attachment_label", ph__attachment_label },
    { "_attachment_body", ph__attachment_body },
    { "_normalize_chat_key", ph__normalize_chat_key },
    { "_record_sent_message", ph__record_sent_message },
    { "_put_by_chat", ph__put_by_chat },
    { "_record_last_inbound", ph__record_last_inbound },
    { "_record_recent_richlink", ph__record_recent_richlink },
    { "_is_recent_richlink_preview", ph__is_recent_richlink_preview },
    { "_reactions_enabled", ph__reactions_enabled },
    { "_is_retryable_error", ph__is_retryable_error },
    { "_is_permanent_sidecar_failure", ph__is_permanent_sidecar_failure },
    { "_send_retry_is_final", ph__send_retry_is_final },
    { "_sidecar_try", ph__sidecar_try },
    { "_post_send", ph__post_send },
    { "_sidecar_send_poll", ph__sidecar_send_poll },
    { "_sidecar_send_attachment", ph__sidecar_send_attachment },
    { "_sidecar_send", ph__sidecar_send },
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

        const ph_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            ph_ctx_t ctx;
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