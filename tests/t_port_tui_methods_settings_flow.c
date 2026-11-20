/* t_port_tui_methods_settings_flow.c -- harness for the
 * tui_gateway/methods_config_set.py FLOW port
 * (src/gateway/tui_methods_settings_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the mutated state mirror ("state": the session carrier).
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
#include "gateway/tui_methods_settings_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*tset_fn_t)(tset_ctx_t *, const json_t *);
typedef struct { const char *func; tset_fn_t fn; } tset_case_t;

static const tset_case_t CASES[] = {
    { "_word", tset_word },
    { "_raw_word", tset_raw_word },
    { "_kv", tset_kv_case },
    { "_cfgset_model_ok", tset_cfgset_model_ok_case },
    { "_emit_session_info", tset_emit_session_info_case },
    { "_emit_all_session_info", tset_emit_all_session_info_case },
    { "_cfgset_await_agent", tset_cfgset_await_agent_case },
    { "_refuse_managed_keys", tset_refuse_managed_keys_case },
    { "_refuse_managed_model_pick", tset_refuse_managed_model_pick_case },
    { "_write_display_sections", tset_write_display_sections_case },
    { "_stash_pending_model_switch", tset_stash_pending_model_switch_case },
    { "_cfgset_guarded", tset_cfgset_guarded_case },
    { "_set_word", tset_set_word_case },
    { "_set_toggle", tset_set_toggle_case },
    { "_set_busy", tset_set_busy },
    { "_set_approval_mode", tset_set_approval_mode },
    { "_set_details_section", tset_set_details_section },
    { "_set_verbose", tset_set_verbose },
    { "_set_focus", tset_set_focus },
    { "_set_display_toggle", tset_set_display_toggle },
    { "_set_daybreak", tset_set_daybreak },
    { "_set_skin", tset_set_skin },
    { "_set_prompt", tset_set_prompt },
    { "_set_personality", tset_set_personality },
    { "_set_cwd", tset_set_cwd },
    { "_set_yolo", tset_set_yolo },
    { "_set_yolo_persist", tset_set_yolo_persist },
    { "_set_reasoning", tset_set_reasoning },
    { "_set_fast", tset_set_fast },
    { "_set_model", tset_set_model },
    { "_h_config_set", tset_h_config_set },
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

        const tset_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (CASES[i].func && strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            tset_ctx_t ctx;
            ctx.state = (state && state->type == JSON_OBJECT) ? json_copy(state) : json_object();
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.cursors = json_object();
            ctx.trace = json_array();
            ctx.last_raise = false;
            ctx.has_override = false;
            ctx.session_override = NULL;
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