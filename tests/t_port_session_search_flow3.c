/* t_port_session_search_flow3.c — harness for the FTS5 result-shaping remainder
 * of tools/session_search_tool.py ported in src/agent/session_search_flow3.c.
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures
 *   ret   — the port's return value (None==JSON null, raise-spec on raise),
 *   trace — ordered seam events {"name","args","ret"},
 *   state — the mutated state window (echoed; no state mutation in this port).
 * The echoed "in" is the ORIGINAL input: the oracle re-drives the live
 * reference from it. Scripted I/O lives in in.io (map seam-name -> result).
 *
 * The port .c is #included directly (self-contained, libjson only).
 */
#include "agent/session_search_flow3.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*ss3_fn_t)(ss3_ctx_t *, const json_t *);
typedef struct { const char *func; ss3_fn_t fn; } ss3_case_t;

static const ss3_case_t CASES[] = {
    { "content_text",         ss3_content_text_fn },
    { "redact_derived_value", ss3_redact_derived_value_dispatch },
    { "redact_tool_calls",    ss3_redact_tool_calls_dispatch },
    { "discovery_entry",      ss3_discovery_entry },
    { "derived_snippet",      ss3_derived_snippet },
    { "bookend",              ss3_bookend },
    { "discover_payload",     ss3_discover_payload },
    { "hydrate_hit",          ss3_hydrate_hit },
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

        const ss3_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            ss3_ctx_t ctx;
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
