/* t_port_feishu_adapter_flow2.c — harness for the Feishu adapter inbound-event
 * + card-action FLOW remainder port
 * (src/gateway/platforms/feishu_adapter_flow2.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {"args": {...}, "io": {...}}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the mutated state ("state"). The echoed "in" is the
 * ORIGINAL input (copied BEFORE dispatch, since the mutating funcs edit
 * args.self / args.states in place); the oracle re-drives the live reference
 * from it.
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order, or {"__script__": true, "ret": <value>, "raise": {type,...}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * State emission: _on_bot_membership_change emits {_chat_info_cache};
 * _is_card_action_duplicate emits {_card_action_tokens};
 * _pop_validated_prompt_state emits {states}; every other func emits {} (its
 * observable output is entirely in ret/trace).
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it, avoiding basename collisions).
 */
#include "gateway/platforms/feishu_adapter_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*fa2_fn_t)(fa2_ctx_t *, const json_t *);
typedef struct { const char *func; fa2_fn_t fn; int accum; } fa2_case_t;
/* accum: 0 = state {}; 1 = {_chat_info_cache}; 2 = {_card_action_tokens};
 *        3 = {states} */
static const fa2_case_t CASES[] = {
    { "format_message", fa2_format_message, 0 },
    { "_on_message_read_event", fa2__on_message_read_event, 0 },
    { "_on_p2p_chat_entered", fa2__on_p2p_chat_entered, 0 },
    { "_on_message_recalled", fa2__on_message_recalled, 0 },
    { "_on_bot_membership_change", fa2__on_bot_membership_change, 1 },
    { "_on_bot_added_to_chat", fa2__on_bot_added_to_chat, 0 },
    { "_on_bot_removed_from_chat", fa2__on_bot_removed_from_chat, 0 },
    { "_loop_accepts_callbacks", fa2__loop_accepts_callbacks, 0 },
    { "_is_interactive_operator_authorized", fa2__is_interactive_operator_authorized, 0 },
    { "_card_response", fa2__card_response, 0 },
    { "_card_toast", fa2__card_toast, 0 },
    { "_validate_card_action", fa2__validate_card_action, 0 },
    { "_handle_approval_card_action", fa2__handle_approval_card_action, 0 },
    { "_handle_update_prompt_card_action", fa2__handle_update_prompt_card_action, 0 },
    { "_pop_validated_prompt_state", fa2__pop_validated_prompt_state, 3 },
    { "_is_card_action_duplicate", fa2__is_card_action_duplicate, 2 },
    { "_on_reaction_event", fa2__on_reaction_event, 0 },
    { "_on_card_action_trigger", fa2__on_card_action_trigger, 0 },
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
        /* echo the ORIGINAL input before dispatch mutates self/states */
        json_t *echo_in = in ? json_copy(in) : json_null();
        json_t *out = json_object();

        const fa2_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            fa2_ctx_t ctx;
            ctx.state = json_object();
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.cursors = json_object();
            ctx.trace = json_array();
            json_t *ret = tc->fn(&ctx, args ? args : empty_args);
            json_set(out, "ret", ret ? ret : json_null());
            json_set(out, "trace", ctx.trace);
            json_t *state = json_object();
            const json_t *self = args ? json_obj_get(args, "self") : NULL;
            if (tc->accum == 1) {
                fa2_set_copy(state, "_chat_info_cache", self ? json_obj_get(self, "_chat_info_cache") : NULL);
            } else if (tc->accum == 2) {
                fa2_set_copy(state, "_card_action_tokens", self ? json_obj_get(self, "_card_action_tokens") : NULL);
            } else if (tc->accum == 3) {
                fa2_set_copy(state, "states", args ? json_obj_get(args, "states") : NULL);
            }
            json_set(out, "state", state);
            json_free(ctx.cursors);
            json_free(ctx.state);
        }
        json_t *echo = json_object();
        json_set_hdr(echo, "func", func);
        json_set(echo, "in", echo_in);
        json_set(echo, "out", out);
        char *s = json_serialize(echo);
        if (s) { printf("%s\n", s); free(s); }
        json_free(echo);
        json_free(rec);
    }
    json_free(empty_args);
    return 0;
}