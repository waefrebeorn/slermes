/* t_port_matrix_adapter_flow.c -- harness for the Matrix-adapter FLOW port
 * (src/gateway/platforms/matrix_adapter_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the mutated state ("state"). The echoed "in" is the
 * ORIGINAL input (the oracle re-drives the live reference from it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order, or {"__script__": true, "ret": <value>, "raise": {type,...}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it, avoiding basename collisions).
 */
#include "gateway/platforms/matrix_adapter_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*ma_fn_t)(ma_ctx_t *, const json_t *);
typedef struct { const char *func; ma_fn_t fn; } ma_case_t;

static const ma_case_t CASES[] = {
    { "_utf8_len", ma_utf8_len },
    { "_content_bytes", ma_content_bytes },
    { "_csv_set", ma_csv_set },
    { "_thread_root", ma_thread_root },
    { "_normalize_e2ee_mode", ma_normalize_e2ee_mode },
    { "_extract_reply_fallback", ma_extract_reply_fallback },
    { "_strip_reply_fallback", ma_strip_reply_fallback },
    { "_split_reply_fallback", ma_split_reply_fallback },
    { "_is_permanent_matrix_auth_error", ma_is_permanent_matrix_auth_error },
    { "_matrix_event_timestamp_seconds", ma_matrix_event_timestamp_seconds },
    { "_redact_url_for_log", ma_redact_url_for_log },
    { "_looks_like_transport_filename", ma_looks_like_transport_filename },
    { "_looks_like_matrix_image_filename", ma_looks_like_matrix_image_filename },
    { "_looks_like_matrix_media_filename", ma_looks_like_matrix_media_filename },
    { "_is_bare_media_filename", ma_is_bare_media_filename },
    { "_env_truthy", ma_env_truthy },
    { "_env_number", ma_env_number },
    { "_scoped_recovery_key", ma_scoped_recovery_key },
    { "_extra_csv_set", ma_extra_csv_set },
    { "_resolve_e2ee_mode", ma_resolve_e2ee_mode },
    { "_resolve_matrix_bang_command", ma_resolve_matrix_bang_command },
    { "_normalize_matrix_bang_command", ma_normalize_matrix_bang_command },
    { "_resolve_max_message_length", ma_resolve_max_message_length },
    { "_is_self_sender", ma_is_self_sender },
    { "_is_system_or_bridge_sender", ma_is_system_or_bridge_sender },
    { "_user_localpart", ma_user_localpart },
    { "_mxc_to_http", ma_mxc_to_http },
    { "_sanitize_link_url", ma_sanitize_link_url },
    { "_state_event_value", ma_state_event_value },
    { "_extract_server_ed25519", ma_extract_server_ed25519 },
    { "_configured_bool", ma_configured_bool },
    { "_media_too_large", ma_media_too_large },
    { "_matrix_prompt_expired", ma_matrix_prompt_expired },
    { "_apply_relation_metadata", ma_apply_relation_metadata },
    { "_is_duplicate_event", ma_is_duplicate_event },
    { "_invalidate_room_identities", ma_invalidate_room_identities },
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

        const ma_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            ma_ctx_t ctx;
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