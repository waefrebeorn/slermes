/* t_port_tui_methods_projects_flow.c — harness for the tui_gateway/methods_projects.py
 * FLOW port (src/gateway/tui_methods_projects_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the mutated module-global mirror ("state":
 * _DIR_EXISTS_CACHE / _registry_pending). The echoed "in" is the ORIGINAL
 * input (the oracle re-drives the live reference from it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order, or {"__script__": true, "ret": <value>, "raise": {type,...}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it, avoiding basename collisions).
 */
#include "gateway/tui_methods_projects_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*tmp_fn_t)(tmp_ctx_t *, const json_t *);
typedef struct { const char *func; tmp_fn_t fn; } tmp_case_t;

static const tmp_case_t CASES[] = {
    { "_pick", tmp_pick },
    { "_project_tree_row", tmp_project_tree_row },
    { "_non_workspace_dirs", tmp_non_workspace_dirs_entry },
    { "_is_repo_junk", tmp_is_repo_junk_entry },
    { "_is_session_cwd_junk", tmp_is_session_cwd_junk_entry },
    { "_repo_discovery_policy", tmp_repo_discovery_policy },
    { "_repo_discovery_policy_key", tmp_repo_discovery_policy_key_entry },
    { "_repo_discovery_policy_is_default", tmp_repo_discovery_policy_is_default_entry },
    { "_dir_exists_cached", tmp_dir_exists_cached },
    { "_require_project", tmp_require_project_entry },
    { "_projects_payload", tmp_projects_payload_entry },
    { "_projects_method", tmp_projects_method },
    { "_register_project_mutator", tmp_register_project_mutator },
    { "_h_projects_list", tmp_projects_list },
    { "_h_projects_get", tmp_projects_get },
    { "_h_projects_create", tmp_projects_create },
    { "_h_projects_archive", tmp_projects_archive },
    { "_h_projects_delete", tmp_projects_delete },
    { "_h_projects_set_active", tmp_projects_set_active },
    { "_h_projects_for_cwd", tmp_projects_for_cwd },
    { "_scan_discovered_repos_remote", tmp_scan_discovered_repos_remote },
    { "_discover_repos_payload", tmp_discover_repos_payload },
    { "_project_tree_inputs", tmp_project_tree_inputs },
    { "_build_project_tree", tmp_build_project_tree },
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

        const tmp_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (CASES[i].func && strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            tmp_ctx_t ctx;
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