/* t_port_tui_methods_voice_flow.c -- harness for the TUI voice-method FLOW port
 * (src/gateway/tui_methods_voice_flow.c). Quarry module:
 * tui_gateway/methods_voice.py.
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the module-global mirror ("state"). The echoed "in" is
 * the ORIGINAL input (the oracle re-drives the live reference from it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either a
 * plain JSON value (returned verbatim on every call), a LIST consumed in call
 * order, or {"__script__": true, "ret": <value>, "raise": {type,...}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 */
#include "gateway/tui_methods_voice_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*tmv_fn_t)(tmv_ctx_t *, const json_t *);
typedef struct { const char *func; tmv_fn_t fn; } tmv_case_t;

static const tmv_case_t CASES[] = {
    { "_caller_transport", tmv_caller_transport },
    { "_voice_emit", tmv_voice_emit },
    { "_resume_voice_wake", tmv_resume_voice_wake },
    { "_voice_mode_enabled", tmv_voice_mode_enabled },
    { "_voice_tts_enabled", tmv_voice_tts_enabled },
    { "_session_voice_enabled", tmv_session_voice_enabled },
    { "_set_session_voice_off", tmv_set_session_voice_off },
    { "_capture_live", tmv_capture_live },
    { "_end_voice_chat", tmv_end_voice_chat },
    { "_end_session_voice_chat", tmv_end_session_voice_chat },
    { "_running_sessions", tmv_running_sessions },
    { "_any_session_running", tmv_any_session_running },
    { "_arm_barge_listener_if_enabled", tmv_arm_barge_listener_if_enabled },
    { "_fd_barge_params", tmv_fd_barge_params },
    { "_voice_cfg_dict", tmv_voice_cfg_dict },
    { "_voice_cfg_number", tmv_voice_cfg_number },
    { "_voice_status_payload", tmv_voice_status_payload },
    { "_wake_owner_snapshot", tmv_wake_owner_snapshot },
    { "_release_wake_for_transport", tmv_release_wake_for_transport },
    { "_release_gateway_wake_owner", tmv_release_gateway_wake_owner },
    { "_persist_wake_enabled", tmv_persist_wake_enabled },
    { "_owner_result", tmv_owner_result },
    { "_deliver_fd_transcript", tmv_deliver_fd_transcript },
    { "_voice_toggle_status", tmv_voice_toggle_status },
    { "_set_voice_tts", tmv_set_voice_tts },
    { "_voice_toggle_tts", tmv_voice_toggle_tts },
    { "_vr_transcript", tmv_vr_transcript },
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

        const tmv_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (CASES[i].func && strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            tmv_ctx_t ctx;
            ctx.state = (state && state->type == JSON_OBJECT) ? json_copy(state) : json_object();
            /* Deterministic module-global mirror: seed every known key with its
             * Python default so reads and writes project identically on both
             * sides (json_obj_get distinguishes present-null from absent). */
            if (!json_obj_get(ctx.state, "voice_event_sid")) json_set_hdr(ctx.state, "voice_event_sid", "");
            if (!json_obj_get(ctx.state, "voice_event_transport")) json_set(ctx.state, "voice_event_transport", json_null());
            if (!json_obj_get(ctx.state, "voice_wake_owner")) json_set(ctx.state, "voice_wake_owner", json_null());
            if (!json_obj_get(ctx.state, "voice_capture_sid")) json_set_hdr(ctx.state, "voice_capture_sid", "");
            if (!json_obj_get(ctx.state, "voice_capture_gen")) json_set(ctx.state, "voice_capture_gen", json_number(0));
            if (!json_obj_get(ctx.state, "wake_owner_transport")) json_set(ctx.state, "wake_owner_transport", json_null());
            if (!json_obj_get(ctx.state, "wake_owner_surface")) json_set_hdr(ctx.state, "wake_owner_surface", "");
            if (!json_obj_get(ctx.state, "fd_listener_active")) json_set(ctx.state, "fd_listener_active", json_bool(false));
            if (!json_obj_get(ctx.state, "sessions")) json_set(ctx.state, "sessions", json_object());
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
