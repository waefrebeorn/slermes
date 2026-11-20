/* t_port_tui_methods_status_flow.c — harness for the TUI session live-status
 * and registry FLOW port (src/gateway/tui_methods_status_flow.c).
 *
 * The task named tui_gateway/methods_status.py, which does not exist in the
 * quarry; the real defining module is tui_gateway/session_registry.py (the
 * live session STATUS methods + registry claim/reuse + build-wait + RPC
 * session-resolver cluster). See the port header for the resolution and the
 * seam contract.
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the mutated module-global mirror ("state": _sessions).
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
#include "gateway/tui_methods_status_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*tmst_fn_t)(tmst_ctx_t *, const json_t *);
typedef struct { const char *func; tmst_fn_t fn; } tmst_case_t;

static const tmst_case_t CASES[] = {
    { "_wait_agent", tmst__wait_agent },
    { "_agent_build_wait_cap", tmst__agent_build_wait_cap },
    { "_bind_build_profile_scopes", tmst__bind_build_profile_scopes },
    { "_release_build_profile_scopes", tmst__release_build_profile_scopes },
    { "_deferred_build_agent_kwargs", tmst__deferred_build_agent_kwargs },
    { "_await_resume_history", tmst__await_resume_history },
    { "_attach_built_agent", tmst__attach_built_agent },
    { "_start_session_services", tmst__start_session_services },
    { "_finish_agent_build", tmst__finish_agent_build },
    { "_sess_nowait", tmst__sess_nowait },
    { "_sess_building", tmst__sess_building },
    { "_sess", tmst__sess },
    { "_new_session_key", tmst__new_session_key },
    { "_is_server_minted_key", tmst__is_server_minted_key },
    { "_any_live_session_claims_key", tmst__any_live_session_claims_key },
    { "_live_profile_matches", tmst__live_profile_matches },
    { "_session_lookup_key", tmst__session_lookup_key },
    { "_find_live_session_by_key", tmst__find_live_session_by_key },
    { "_claim_parked_runtimes", tmst__claim_parked_runtimes },
    { "_finalize_superseded_runtimes", tmst__finalize_superseded_runtimes },
    { "_claim_or_reuse_live", tmst__claim_or_reuse_live },
    { "_session_pending_kind", tmst__session_pending_kind },
    { "_session_live_status", tmst__session_live_status },
    { "_session_live_title", tmst__session_live_title },
    { "_durable_ids", tmst__durable_ids },
    { "_legacy_display_anchor", tmst__legacy_display_anchor },
    { "_reconcile_display_with_live", tmst__reconcile_display_with_live },
    { "_main_runtime_from_agent", tmst__main_runtime_from_agent },
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

        const tmst_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (CASES[i].func && strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            tmst_ctx_t ctx;
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