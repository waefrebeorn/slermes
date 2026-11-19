/* t_port_tui_methods_sessions_flow.c — harness for the
 * tui_gateway/methods_session.py FLOW port
 * (src/gateway/tui_methods_sessions_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the mutated caller-dict mirror ("state": session).
 * The echoed "in" is the ORIGINAL input (the oracle re-drives the live
 * reference from it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order, or {"__script__": true, "ret": <value>, "raise": {type,...}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it, avoiding basename collisions).
 */
#include "gateway/tui_methods_sessions_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*tms_fn_t)(tms_ctx_t *, const json_t *);
typedef struct { const char *func; tms_fn_t fn; } tms_case_t;

static const tms_case_t CASES[] = {
    { "_session_arg", tms_session_arg },
    { "_session_method", tms_session_method },
    { "_with_db", tms_with_db },
    { "_new_runtime_ids", tms_new_runtime_ids },
    { "_profile_build_scope", tms_profile_build_scope },
    { "_make_agent_in_context", tms_make_agent_in_context },
    { "_cwd_info", tms_cwd_info },
    { "_session_row_summary", tms_session_row_summary },
    { "_row_of", tms_row_of },
    { "_listing_rows", tms_listing_rows },
    { "_snapshot_sessions", tms_snapshot_sessions },
    { "_pet_display_cfg", tms_pet_display_cfg },
    { "_pet_emit", tms_pet_emit },
    { "_pet_gen_abort", tms_pet_gen_abort },
    { "_pet_method", tms_pet_method },
    { "_active_pet", tms_active_pet },
    { "_b64", tms_b64 },
    { "_pet_config_followup", tms_pet_config_followup },
    { "_pet_pick_provider", tms_pet_pick_provider },
    { "_billing_call", tms_billing_call },
    { "_billing_invalid", tms_billing_invalid },
    { "_billing_pending_change", tms_billing_pending_change },
    { "_billing_route", tms_billing_route },
    { "_unarchive_recoverable", tms_unarchive_recoverable },
    { "_session_list_by_title", tms_session_list_by_title },
    { "_find_live_unpersisted", tms_find_live_unpersisted },
    { "_status_row", tms_status_row },
    { "_try_get_session", tms_try_get_session },
    { "_title_read", tms_title_read },
    { "_auto_reload", tms_auto_reload },
    { "_legacy_spawn_tree_entry", tms_legacy_spawn_tree_entry },
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

        const tms_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (CASES[i].func && strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            tms_ctx_t ctx;
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