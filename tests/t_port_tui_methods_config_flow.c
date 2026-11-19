/* t_port_tui_methods_config_flow.c -- harness for the tui_gateway/methods_config.py
 * FLOW port (src/gateway/tui_methods_config_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the mutated state mirror ("state": inflight pairs).
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
#include "gateway/tui_methods_config_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* getter-signature adapters (ctx, params, home) -> the uniform case shape */
static json_t *h_cfg_get_provider(tmc_ctx_t *ctx, const json_t *args) {
    return tmc_cfg_get_provider(ctx, json_obj_get(args, "params"),
                                json_get_str(args, "hermes_home", ""));
}
static json_t *h_cfg_get_project(tmc_ctx_t *ctx, const json_t *args) {
    return tmc_cfg_get_project(ctx, json_obj_get(args, "params"),
                               json_get_str(args, "hermes_home", ""));
}
static json_t *h_cfg_get_personality(tmc_ctx_t *ctx, const json_t *args) {
    return tmc_cfg_get_personality(ctx, json_obj_get(args, "params"),
                                   json_get_str(args, "hermes_home", ""));
}
static json_t *h_cfg_get_reasoning(tmc_ctx_t *ctx, const json_t *args) {
    return tmc_cfg_get_reasoning(ctx, json_obj_get(args, "params"),
                                 json_get_str(args, "hermes_home", ""));
}
static json_t *h_cfg_get_fast(tmc_ctx_t *ctx, const json_t *args) {
    return tmc_cfg_get_fast(ctx, json_obj_get(args, "params"),
                            json_get_str(args, "hermes_home", ""));
}
static json_t *h_cfg_get_thinking_mode(tmc_ctx_t *ctx, const json_t *args) {
    return tmc_cfg_get_thinking_mode(ctx, json_obj_get(args, "params"),
                                     json_get_str(args, "hermes_home", ""));
}
static json_t *h_cfg_get_mtime(tmc_ctx_t *ctx, const json_t *args) {
    return tmc_cfg_get_mtime(ctx, json_obj_get(args, "params"),
                             json_get_str(args, "hermes_home", ""));
}

typedef json_t *(*tmc_fn_t)(tmc_ctx_t *, const json_t *);
typedef struct { const char *func; tmc_fn_t fn; } tmc_case_t;

static const tmc_case_t CASES[] = {
    { "_display_raw", tmc_display_raw },
    { "_display_word", tmc_display_word },
    { "_safe_client_label", tmc_safe_client_label },
    { "_redact_client_text", tmc_redact_client_text },
    { "_cfg_get_provider", h_cfg_get_provider },
    { "_cfg_get_project", h_cfg_get_project },
    { "_cfg_get_personality", h_cfg_get_personality },
    { "_cfg_get_reasoning", h_cfg_get_reasoning },
    { "_cfg_get_fast", h_cfg_get_fast },
    { "_cfg_get_thinking_mode", h_cfg_get_thinking_mode },
    { "_cfg_get_mtime", h_cfg_get_mtime },
    { "_config_getter", tmc_config_getter },
    { "_h_config_get", tmc_config_get },
    { "_reconcile_repo_discovery", tmc_reconcile_repo_discovery },
    { "_stamped_project_tree", tmc_stamped_project_tree },
    { "_h_projects_discover_repos", tmc_projects_discover_repos },
    { "_h_projects_record_repos", tmc_projects_record_repos },
    { "_h_projects_tree", tmc_projects_tree },
    { "_h_projects_project_sessions", tmc_projects_project_sessions },
    { "_readiness_cleared", tmc_readiness_cleared },
    { "_readiness_share", tmc_readiness_share },
    { "_readiness_check", tmc_readiness_check },
    { "_h_setup_status", tmc_setup_status },
    { "_h_setup_runtime_check", tmc_setup_runtime_check },
    { "_h_share_nous", tmc_share_nous },
    { "_projects_handler", tmc_projects_handler },
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

        const tmc_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (CASES[i].func && strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            tmc_ctx_t ctx;
            ctx.state = (state && state->type == JSON_OBJECT) ? json_copy(state) : json_object();
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.cursors = json_object();
            ctx.trace = json_array();
            ctx.last_raise = false;
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