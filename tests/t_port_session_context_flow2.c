/* t_port_session_context_flow2.c — harness for the gateway/session_context.py
 * FLOW remainder port (src/gateway/session_context_flow2.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the ContextVar state echo ("state"). The echoed "in" is
 * the ORIGINAL input (the oracle re-drives the live reference from it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either a
 * plain JSON value (returned verbatim on every call), a LIST consumed in call
 * order (last value does NOT repeat), or {"__script__": true, "ret": <value>,
 * "raise": {type,message}}. A missing/exhausted script is a fail-closed
 * RuntimeError on both sides; an absent in.io is "no seam channel installed".
 *
 * in.state carries the ContextVar model (vars / turn_event / history_delivery /
 * async_delivery / engaged); only present keys are echoed. in.import_fail and
 * in.runtime_cwd_attrs drive the _runtime_cwd import/attribute gates.
 *
 * The port .c is #included directly (self-contained, libjson only).
 */
#include "gateway/session_context_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*scx2_fn_t)(scx2_ctx_t *, const json_t *);
typedef struct { const char *func; scx2_fn_t fn; } scx2_case_t;

static const scx2_case_t CASES[] = {
    { "_runtime_cwd", scx2_runtime_cwd },
    { "scoped_turn_actor", scx2_scoped_turn_actor },
    { "scoped_session_key", scx2_scoped_session_key },
    { "bound_session_key", scx2_bound_session_key },
    { "source_route_metadata", scx2_source_route_metadata },
    { "_bound_session_value", scx2_bound_session_value },
    { "session_history_delivery_supported", scx2_session_history_delivery_supported },
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

        const scx2_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            scx2_ctx_t ctx;
            ctx.state = (state && state->type == JSON_OBJECT) ? json_copy(state) : json_object();
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.cursors = json_object();
            ctx.trace = json_array();
            ctx.in = in;
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
