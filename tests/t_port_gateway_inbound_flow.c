/* t_port_gateway_inbound_flow.c — harness for the session-authority +
 * inbound-message FLOW port (src/gateway/gateway_inbound_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the mutated self/world state ("state"). The echoed
 * "in" is the ORIGINAL input (the oracle re-drives the live reference from
 * it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order, or {"__script__": true, "ret": <value>, "raise": {type,...},
 * "live": <live-session spec>}. "live" is the restore seams' shared state
 * effect (sessions[session_id] installation) — the SAME effect the oracle's
 * recording stubs apply. A missing/exhausted script is a fail-closed
 * RuntimeError on both sides.
 *
 * The port .c is #included directly (self-contained, libjson only).
 */
#include "gateway_inbound_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*gif_fn_t)(gif_ctx_t *, const json_t *);
typedef struct { const char *func; gif_fn_t fn; } gif_case_t;

static const gif_case_t CASES[] = {
    { "admission_row", gif_admission_row },
    { "_require_admission_open", gif_require_admission_open },
    { "gate_refusal", gif_gate_refusal },
    { "logical_owner", gif_logical_owner },
    { "physical_target", gif_physical_target },
    { "_current_owner", gif_current_owner },
    { "_receipt", gif_receipt },
    { "_handle", gif_handle },
    { "authorize", gif_authorize },
    { "adopt_agent", gif_adopt_agent },
    { "publish_execution", gif_publish_execution },
    { "discord_triggering_note", gif_discord_triggering_note },
    { "strip_discord_triggering_note", gif_strip_discord_triggering_note },
    { "_hm_estop_turn_allowed", gif_estop_turn_allowed },
    { "_hm_estop_gate", gif_estop_gate },
    { "_hm_update_prompt_reply", gif_update_prompt_reply },
    { "_hm_slash_confirm_reply", gif_slash_confirm_reply },
    { "_hm_busy_slash_or_photo", gif_busy_slash_or_photo },
    { "_hm_admit_event", gif_admit_event },
    { "_hm_send_unauthorized_decline", gif_send_unauthorized_decline },
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

        const gif_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            gif_ctx_t ctx;
            ctx.state = (state && state->type == JSON_OBJECT) ? json_copy(state) : json_object();
            /* LIVE SessionAuthority.__init__ seeds these containers even when
             * the fixture omits them; mirror for the authority funcs only. */
            static const char *sa_funcs[] = {"admission_row", "_require_admission_open",
                "gate_refusal", "logical_owner", "physical_target", "_current_owner",
                "_receipt", "_handle", "authorize", "adopt_agent", "publish_execution"};
            bool is_sa = false;
            for (size_t fi = 0; fi < sizeof(sa_funcs) / sizeof(sa_funcs[0]); fi++)
                if (strcmp(sa_funcs[fi], func) == 0) { is_sa = true; break; }
            if (is_sa) {
              static const char *seeded[] = {"adopted", "delivered_stops", "executing",
                                             "pending_steers", "pending_stops",
                                             "rotated_owners", "sessions"};
              for (size_t si = 0; si < sizeof(seeded) / sizeof(seeded[0]); si++)
                  if (!json_obj_get(ctx.state, seeded[si]))
                      json_set(ctx.state, seeded[si], json_object());
              if (!json_obj_get(ctx.state, "retiring"))
                  json_set(ctx.state, "retiring", json_bool(false));
            }
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
