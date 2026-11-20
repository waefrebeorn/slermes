/* t_port_tui_methods_session_flow2.c -- harness for the
 * tui_gateway/methods_session.py FLOW2 port (unclaimed remainder).
 *
 * Reads JSON lines from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON lines: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret") and the mutated caller-dict mirror ("state").
 *
 * Scripted I/O lives in in.io: a map seam-name -> result (plain JSON value,
 * a per-call LIST/queue, or {"__script__":true,"ret":...,"raise":{...}}).
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 */
#include "gateway/tui_methods_session_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*tms2_fn_t)(tms2_ctx_t *, const json_t *);
typedef struct { const char *func; tms2_fn_t fn; } tms2_case_t;

static const tms2_case_t CASES[] = {
    { "_b64", tms2_b64_2 },
    { "terminal.resize", tms2_terminal_resize },
    { "delegation.status", tms2_delegation_status },
    { "delegation.pause", tms2_delegation_pause },
    { "session.events.stats", tms2_events_stats },
    { "session.events.since", tms2_events_since },
    { "pet.disable", tms2_pet_disable },
    { "pet.scale", tms2_pet_scale },
    { "pet.cancel", tms2_pet_cancel },
    { "pet.generate.status", tms2_pet_gen_status },
    { "session.close", tms2_session_close },
    { "session.branch", tms2_session_branch },
    { "session.branch_whole", tms2_session_branch_whole },
    { "_save_via_compute_host", tms2_save_via_compute_host },
    { "spawn_tree.load", tms2_spawn_tree_load },
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

        const tms2_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (CASES[i].func && strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            tms2_ctx_t ctx;
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
