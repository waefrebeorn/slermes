/* t_port_matrix_adapter_flow8.c -- harness for the Matrix-adapter remainder
 * flow port (src/gateway/platforms/matrix_adapter_flow8.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the ordered seam
 * events {"name","args","ret"} in "trace", the final return ("ret") and the
 * state echo ("state"). The echoed "in" is the ORIGINAL input.
 *
 * Scripted I/O lives in in.io: a map seam-name -> result (plain value, a
 * call-order LIST, or {"__script__":true,"ret"/"raise":...}). A missing or
 * exhausted script is a fail-closed RuntimeError on both sides.
 *
 * The port .c is #included directly by area-qualified path (libjson only).
 */
#include "gateway/platforms/matrix_adapter_flow8.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*ma8_fn_t)(ma8_ctx_t *, const json_t *);
typedef struct { const char *func; ma8_fn_t fn; } ma8_case_t;

static const ma8_case_t CASES[] = {
    { "_EA_HEADER", ma8_ea_header },
    { "_send_room_message", ma8_send_room_message },
    { "_connect_authenticate", ma8_connect_authenticate },
    { "_connect_setup_e2ee", ma8_connect_setup_e2ee },
    { "_download_external_media_with_cap", ma8_download_external_media_with_cap },
    { "_handle_media_message", ma8_handle_media_message },
    { "_schedule_reaction_redaction", ma8_schedule_reaction_redaction },
    { "_send_exec_approval_prompt", ma8_send_exec_approval_prompt },
    { "_send_picker", ma8_send_picker },
    { "_send_local_file", ma8_send_local_file },
    { "_send_reaction_prompt", ma8_send_reaction_prompt },
    { "_markdown_to_html_fallback", ma8_markdown_to_html_fallback },
    { "_standalone_payloads", ma8_standalone_payloads },
    { "_standalone_send", ma8_standalone_send },
    { "_write_matrix_recovery_key_output_file", ma8_write_recovery_key_output_file },
    { "create_handoff_thread", ma8_create_handoff_thread },
};
#define CASE_COUNT (sizeof(CASES) / sizeof(CASES[0]))

static char *read_line(FILE *f) {
    size_t cap = 8192, len = 0;
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
    setvbuf(stdout, NULL, _IONBF, 0);
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

        const ma8_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            ma8_ctx_t ctx;
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
