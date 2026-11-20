/* t_port_executions_worker_flow.c — harness for the worker-execution-repository
 * FLOW port (src/persistence/executions_worker_flow.c).
 *
 * Reads JSON LINES: {"func": <name>, "in": {"args": {...}, "io": {...}}}.
 * Emits JSON LINES: {"func", "in", "out": {"ret", "trace"}}.
 * "out" captures only what the C decision core did: the ordered seam trace
 * ({"name","args","ret"} events in call order) and the final return. The echoed
 * "in" is the ORIGINAL input so the oracle can re-drive the live reference.
 *
 * Scripted I/O lives in in.io: a map seam-name -> result (a plain value on
 * every call, a LIST consumed one-per-call, or {"__raise__":cls,"message":m}).
 * A missing/exhausted script fail-closes with RuntimeError; no real I/O.
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it, avoiding basename collisions).
 */
#include "persistence/executions_worker_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*ewf_fn_t)(ewf_ctx_t *, const json_t *);
typedef struct { const char *func; ewf_fn_t fn; } ewf_case_t;

static const ewf_case_t CASES[] = {
    { "_key_set", ewf_key_set },
    { "_record", ewf_record },
    { "_messages", ewf_messages },
    { "_annotations", ewf_annotations },
    { "finish", ewf_finish },
    { "record_usage", ewf_record_usage },
    { "record_auxiliary_usage", ewf_record_auxiliary_usage },
    { "acquire_turn_lease", ewf_acquire_turn_lease },
    { "cleanup_turn_lease", ewf_cleanup_turn_lease },
    { "set_session_title", ewf_set_session_title },
    { "set_session_title_source", ewf_set_session_title_source },
    { "next_session_title", ewf_next_session_title },
    { "set_session_billing_route", ewf_set_session_billing_route },
    { "set_latest_user_api_content", ewf_set_latest_user_api_content },
    { "set_message_api_content", ewf_set_message_api_content },
    { "set_user_message_content", ewf_set_user_message_content },
    { "create_session", ewf_create_session },
    { "session_lifecycle", ewf_session_lifecycle },
    { "patch_session_sidecars", ewf_patch_session_sidecars },
    { "set_session_tool_pin", ewf_set_session_tool_pin },
    { "append_transcript", ewf_append_transcript },
    { "compression_watermark", ewf_compression_watermark },
    { "compression_boundary", ewf_compression_boundary },
    { "persist_operation", ewf_persist_operation },
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

        const ewf_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            ewf_ctx_t ctx;
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.trace = json_array();
            ctx.cursors = json_object();
            json_t *ret = tc->fn(&ctx, args ? args : empty_args);
            json_set(out, "ret", ret ? ret : json_null());
            json_set(out, "trace", ctx.trace);
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
