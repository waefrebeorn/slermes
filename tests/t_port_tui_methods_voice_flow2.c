/* t_port_tui_methods_voice_flow2.c -- harness for the flow2 remainder port
 * (src/gateway/tui_methods_voice_flow2.c). Quarry module:
 * tui_gateway/methods_voice.py. 9 unclaimed funcs beyond the flow1 oracle.
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the module-global mirror ("state"). The echoed "in" is
 * the ORIGINAL input (the oracle re-drives the live reference from it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either a
 * plain JSON value (returned verbatim on every call), a LIST consumed in call
 * order, or {"__script__": true, "ret": <value>, "raise": {type,message}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 */
#include "gateway/tui_methods_voice_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*tmv2_fn_t)(tmv2_ctx_t *, const json_t *);
typedef struct { const char *func; tmv2_fn_t fn; } tmv2_case_t;

static const tmv2_case_t CASES[] = {
    { "_frame_fields", tmv2_frame_fields },
    { "register", tmv2_register },
    { "_vr_on_status", tmv2_vr_on_status },
    { "_vr_on_silent_limit", tmv2_vr_on_silent_limit },
    { "_vr_on_stop_phrase", tmv2_vr_on_stop_phrase },
    { "_stop_session_voice_capture", tmv2_stop_session_voice_capture },
    { "_arm_full_duplex_listener", tmv2_arm_full_duplex_listener },
    { "_wake_probe", tmv2_wake_probe },
    { "_voice_toggle_mode", tmv2_voice_toggle_mode },
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

        const tmv2_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (CASES[i].func && strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            tmv2_ctx_t ctx;
            ctx.state = (state && state->type == JSON_OBJECT) ? json_copy(state) : json_object();
            /* Deterministic module-global mirror: seed every known key with its
             * Python default so reads and writes project identically on both
             * sides (json_obj_get distinguishes present-null from absent). */
            if (!json_obj_get(ctx.state, "voice_capture_sid")) json_set_hdr(ctx.state, "voice_capture_sid", "");
            if (!json_obj_get(ctx.state, "voice_capture_gen")) json_set(ctx.state, "voice_capture_gen", json_number(0));
            if (!json_obj_get(ctx.state, "fd_listener_active")) json_set(ctx.state, "fd_listener_active", json_bool(false));
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
