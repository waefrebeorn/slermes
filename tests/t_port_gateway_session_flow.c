/* t_port_gateway_session_flow.c — harness for the gateway/session.py FLOW port
 * (src/gateway/gateway_session_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the mutated self/world state ("state"). The echoed "in"
 * is the ORIGINAL input (the oracle re-drives the live reference from it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either a
 * plain JSON value (returned verbatim on every call), a LIST consumed in call
 * order, or {"__script__": true, "ret": <value>, "raise": {type,...}}. A
 * missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * The port .c is #included directly (self-contained, libjson only). The harness
 * materializes the self-model state (full entry/decision dicts, sorted
 * _rows_pending) via gse_state_setup, mirroring the oracle's per-case setup.
 */
#include "gateway/gateway_session_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*gse_fn_t)(gse_ctx_t *, const json_t *);
typedef struct { const char *func; gse_fn_t fn; } gse_case_t;

static const gse_case_t CASES[] = {
    { "_is_path_unsafe", gse_is_path_unsafe },
    { "_session_key_namespace", gse_session_key_namespace },
    { "_describe", gse_describe },
    { "_canonical_participant", gse_canonical_participant },
    { "_should_redact_pii", gse_should_redact_pii },
    { "label_shared_sender", gse_label_shared_sender },
    { "_slack_platform_notes", gse_slack_platform_notes },
    { "_discord_platform_notes", gse_discord_platform_notes },
    { "schedule_reset", gse_schedule_reset },
    { "_lazy", gse_lazy },
    { "_has_active_processes_safe", gse_has_active_processes_safe },
    { "lookup_by_session_key", gse_lookup_by_session_key },
    { "set_session_yolo", gse_set_session_yolo },
    { "release_route", gse_release_route },
    { "purge_profile_routing", gse_purge_profile_routing },
    { "rekey_profile_routing", gse_rekey_profile_routing },
    { "remove_by_session_id", gse_remove_by_session_id },
    { "restore_route", gse_restore_route },
    { "_replace_route_locked", gse_replace_route_locked },
    { "_route_recover", gse_route_recover },
    { "_route_create", gse_route_create },
    { "_apply_route_checks", gse_apply_route_checks },
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

        const gse_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            gse_ctx_t ctx;
            ctx.state = (state && state->type == JSON_OBJECT) ? json_copy(state) : json_object();
            gse_state_setup(ctx.state);
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