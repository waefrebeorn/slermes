/* t_port_matrix_adapter_flow3.c -- harness for the Matrix-adapter sync/event
 * + reaction flow port (src/gateway/platforms/matrix_adapter_flow3.c).
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
#include "gateway/platforms/matrix_adapter_flow3.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*ma3_fn_t)(ma3_ctx_t *, const json_t *);
typedef struct { const char *func; ma3_fn_t fn; } ma3_case_t;

static const ma3_case_t CASES[] = {
    { "_warn_encrypted_drops", ma3_warn_encrypted_drops },
    { "_is_allowed_matrix_room_event", ma3_is_allowed_matrix_room_event },
    { "_redact_reaction", ma3_redact_reaction },
    { "on_processing_start", ma3_on_processing_start },
    { "on_processing_complete", ma3_on_processing_complete },
    { "_on_reaction", ma3_on_reaction },
    { "_claim_reaction_prompt", ma3_claim_reaction_prompt },
    { "_handle_approval_reaction", ma3_handle_approval_reaction },
    { "_handle_model_picker_reaction", ma3_handle_model_picker_reaction },
    { "_handle_choice_picker_reaction", ma3_handle_choice_picker_reaction },
    { "_handle_picker_reaction", ma3_handle_picker_reaction },
    { "_validate_matrix_prompt_reactor", ma3_validate_matrix_prompt_reactor },
    { "_send_invalid_reaction_feedback", ma3_send_invalid_reaction_feedback },
    { "_expire_matrix_approval_prompt", ma3_expire_matrix_approval_prompt },
    { "_expire_matrix_model_picker_prompt", ma3_expire_matrix_model_picker_prompt },
    { "_redact_bot_approval_reactions", ma3_redact_bot_approval_reactions },
    { "_redact_bot_model_picker_reactions", ma3_redact_bot_model_picker_reactions },
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

        const ma3_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            ma3_ctx_t ctx;
            ctx.state = (state && state->type == JSON_OBJECT) ? json_copy(state) : json_object();
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.cursors = json_object();
            ctx.trace = json_array();
            json_t *ret = tc->fn(&ctx, args ? args : empty_args);
            /* set-valued state (_warned_encrypted_drop_rooms) emits sorted on both sides */
            json_t *wr = json_obj_get(ctx.state, "_warned_encrypted_drop_rooms");
            if (wr && wr->type == JSON_ARRAY)
                json_set(ctx.state, "_warned_encrypted_drop_rooms", ma3_sorted_copy(wr));
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
