/* t_port_session_finite_flow2.c - harness for the gateway/session_finite.py
 * FLOW remainder port (src/gateway/session_finite_flow2.c).
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
 * in.state carries the ContextVar model (finite_turn / unattended_turn); only
 * present keys are echoed. in.import_fail is a list of halted in-function
 * import module names (ModuleNotFoundError, py3.14). in.args carries the call
 * arguments per function.
 *
 * The port .c is #included directly (self-contained, libjson only).
 *
 * Gate: bash tests/run_one_oracle.sh session_finite_flow2 \
 *           oracle_session_finite_flow2.py "" "lib/libjson/json.o"
 */
#include "gateway/session_finite_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*sf2_fn_t)(sf2_ctx_t *, const json_t *);
typedef struct { const char *func; sf2_fn_t fn; } sf2_case_t;

/* _may_run_unattended takes (connection, ref) rather than an args object;
 * wrap it to the uniform (ctx, args) dispatch shape. */
static json_t *sf2_h_may_run(sf2_ctx_t *c, const json_t *args) {
    return sf2_may_run_unattended(c, json_obj_get(args, "connection"),
                                  json_obj_get(args, "ref"));
}

static const sf2_case_t CASES[] = {
    { "finite_clarify_reply", sf2_finite_clarify_reply },
    { "unattended_turn", sf2_unattended_turn },
    { "finite_turn_scope", sf2_finite_turn_scope },
    { "admit_finite", sf2_admit_finite },
    { "_may_run_unattended", sf2_h_may_run },
    { "execute_finite_admission", sf2_execute_finite_admission },
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

        const sf2_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            sf2_ctx_t ctx;
            ctx.state = (state && state->type == JSON_OBJECT) ? json_copy(state) : json_object();
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.cursors = json_object();
            ctx.trace = json_array();
            ctx.in = in;
            sf2_fn_t fn = tc->fn;
            json_t *ret = fn(&ctx, args ? args : empty_args);
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
