/* t_port_tui_methods_tools_flow2.c — harness for the methods_tools.py FLOW2
 * port (src/gateway/tui_methods_tools_flow2.c). Reads JSONL cases from stdin,
 * emits {func,in,out} where out carries the ordered seam trace, the return and
 * the (empty) state mirror. The echoed "in" is the ORIGINAL input so the oracle
 * re-drives its reference from exactly the same case.
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A plain value is
 * returned on every call; a bare LIST is a per-call queue; {"__script__":true,
 * "ret":v} returns v; {"__script__":true,"raise":{type,message}} raises.
 * Missing/exhausted scripts are fail-closed RuntimeErrors on both sides.
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it).
 */
#include "gateway/tui_methods_tools_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*tmt2_fn_t)(tmt2_ctx_t *, const json_t *);
typedef struct { const char *func; tmt2_fn_t fn; } tmt2_case_t;

static const tmt2_case_t CASES[] = {
    { "_session_toolsets", tmt2_session_toolsets },
    { "_toolset_rows", tmt2_toolset_rows },
    { "_skill_dispatch_or_refuse", tmt2_skill_dispatch_or_refuse },
    { "_bundle_key_for", tmt2_bundle_key_for },
    { "_dispatch_bundle", tmt2_dispatch_bundle },
    { "_dispatch_skill", tmt2_dispatch_skill },
    { "_prompt_builtin", tmt2_prompt_builtin },
    { "prompt_builtin.call", tmt2_prompt_builtin_call },
    { "_dispatch_quick", tmt2_dispatch_quick },
    { "_profile_skill_command", tmt2_profile_skill_command },
    { "_cmd_init", tmt2_cmd_init },
    { "_cmd_moa", tmt2_cmd_moa },
    { "_cmd_focus", tmt2_cmd_focus },
    { "_cmd_retry", tmt2_cmd_retry },
    { "_cmd_steer", tmt2_cmd_steer },
    { "_cmd_goal", tmt2_cmd_goal },
    { "_cmd_loop", tmt2_cmd_loop },
    { "_cmd_undo", tmt2_cmd_undo },
    { "_cmd_compress", tmt2_cmd_compress },
    { "_pending_subcommand_store", tmt2_pending_subcommand_store },
    { "_write_approval_mode_setter", tmt2_write_approval_mode_setter },
    { "write_approval_mode_setter.call", tmt2_write_approval_mode_setter_call },
    { "_run_pending_review", tmt2_run_pending_review },
    { "_cmd_memory", tmt2_cmd_memory },
    { "_cmd_skills", tmt2_cmd_skills },
    { "_slash_worker_for", tmt2_slash_worker_for },
    { "_container_checkpoint_refusal", tmt2_container_checkpoint_refusal },
    { "_configure_session_tools", tmt2_configure_session_tools },
    { "_skills_install", tmt2_skills_install },
    { "register", tmt2_register },
};
#define CASE_COUNT (sizeof(CASES) / sizeof(CASES[0]))

static char *read_line(FILE *f) {
    size_t cap = 8192, len = 0;
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

        const tmt2_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (CASES[i].func && strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            tmt2_ctx_t ctx;
            ctx.state = json_object();
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
