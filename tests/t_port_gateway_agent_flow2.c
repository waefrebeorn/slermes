/* t_port_gateway_agent_flow2.c — harness for the UNCLAIMED event-handler /
 * settings / server-request FLOW remainder of hermes_cli/gateway_agent.py
 * (src/cli/gateway_agent_flow2.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), the mutated agent state ("state") and the mutated working
 * args ("args", e.g. the starts map). The echoed "in" is the ORIGINAL input
 * (the oracle re-drives the live reference from it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order, or {"__script__": true, "ret": <value>, "raise": {type,...}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * The port .c is #included directly (self-contained, libjson only).
 */
#include "cli/gateway_agent_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

        bool known = false;
        for (size_t i = 0; i < GA2_CASE_COUNT; i++)
            if (!strcmp(GA2_CASES[i].func, func)) { known = true; break; }
        if (!known) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args_in = in ? json_obj_get(in, "args") : NULL;
            json_t *work = (args_in && args_in->type == JSON_OBJECT) ? json_copy(args_in) : json_object();
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            ga2_ctx_t ctx;
            ctx.state = (state && state->type == JSON_OBJECT) ? json_copy(state) : json_object();
            ga2_init_defaults(ctx.state);
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.cursors = json_object();
            ctx.trace = json_array();
            json_t *ret = ga2_run(func, &ctx, work);
            json_set(out, "ret", ret ? ret : json_null());
            json_set(out, "trace", ctx.trace);
            json_set(out, "state", ctx.state);
            json_set(out, "args", json_copy(work));
            json_free(ctx.cursors);
            json_free(work);
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
    return 0;
}