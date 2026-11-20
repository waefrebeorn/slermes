/* t_port_matrix_adapter_flow2.c -- harness for the Matrix-adapter FLOW
 * remainder port (src/gateway/platforms/matrix_adapter_flow2.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the mutated state ("state"). The echoed "in" is the
 * ORIGINAL input (the oracle re-drives the live reference from it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order, or {"__script__": true, "ret": <value>, "raise": {type,...}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it, avoiding basename collisions).
 */
#include "gateway/platforms/matrix_adapter_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*ma2_fn_t)(ma2_ctx_t *, const json_t *);
typedef struct { const char *func; ma2_fn_t fn; } ma2_case_t;

static const ma2_case_t CASES[] = {
    { "_extra_truthy", ma2_extra_truthy },
    { "_parse_require_mention", ma2_parse_require_mention },
    { "_parse_thread_require_mention", ma2_parse_thread_require_mention },
    { "_reset_clock_skew_detector", ma2_reset_clock_skew_detector },
    { "_note_late_grace_drop", ma2_note_late_grace_drop },
    { "_is_authorized_user", ma2_is_authorized_user },
    { "_on_invite", ma2_on_invite },
    { "_schedule_pending_invite_joins", ma2_schedule_pending_invite_joins },
    { "_extract_invite_dm_signal", ma2_extract_invite_dm_signal },
    { "_join_room_by_id", ma2_join_room_by_id },
    { "_fetch_m_direct", ma2_fetch_m_direct },
    { "_refresh_dm_cache", ma2_refresh_dm_cache },
    { "_record_dm_room", ma2_record_dm_room },
    { "_is_dm_room", ma2_is_dm_room },
    { "_send_reaction", ma2_send_reaction },
    { "send_read_receipt", ma2_send_read_receipt },
    { "_client_op", ma2_client_op },
    { "redact_message", ma2_redact_message },
    { "invite_user", ma2_invite_user },
    { "set_presence", ma2_set_presence },
    { "_get_room_member_count", ma2_get_room_member_count },
    { "_get_room_state_value", ma2_get_room_state_value },
    { "_get_display_name", ma2_get_display_name },
    { "_set_typing", ma2_set_typing },
    { "send_typing", ma2_send_typing },
    { "stop_typing", ma2_stop_typing },
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

        const ma2_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            ma2_ctx_t ctx;
            ctx.state = (state && state->type == JSON_OBJECT) ? json_copy(state) : json_object();
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.cursors = json_object();
            ctx.trace = json_array();
            json_t *ret = tc->fn(&ctx, args ? args : empty_args);
            /* set-valued state (_joined_rooms) emits sorted on both sides */
            json_t *jr = json_obj_get(ctx.state, "_joined_rooms");
            if (jr && jr->type == JSON_ARRAY)
                json_set(ctx.state, "_joined_rooms", ma2_sorted_copy(jr));
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