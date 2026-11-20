/* t_port_signal_adapter_flow2.c — harness for the Signal adapter FLOW remainder
 * port (src/gateway/platforms/signal_adapter_flow2.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {"args": {...},
 * "state": {...self attrs...}, "io": {...seam scripts...}}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the mutated state ("state"). The echoed "in" is the
 * ORIGINAL input (copied BEFORE dispatch, since the flow mutates ctx.state /
 * ctx.event / args.params in place); the oracle re-drives the live reference
 * from it.
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order, or {"__script__": true, "ret": <value>, "raise": {type,...}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * State emission flags per case: bit0 = emit ctx.state as out.state.self
 * (the adapter self double); bit1 = emit ctx.event as out.state.event (the
 * MessageEvent double mutated by on_processing_start/complete). Module leaves
 * (_is_signal_service_id, validate_signal_config), the staticmethods
 * (_styles_for_chunk, _utf16_offsets) and the classmethod
 * (_split_signal_formatted_message) do not emit self.
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it, avoiding basename collisions).
 */
#include "gateway/platforms/signal_adapter_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*sig2_fn_t)(sig2_ctx_t *, const json_t *);
typedef struct { const char *func; sig2_fn_t fn; int flags; } sig2_case_t;

static const sig2_case_t CASES[] = {
    { "_is_signal_service_id", sig2__is_signal_service_id, 0 },
    { "validate_signal_config", sig2__validate_signal_config, 0 },
    { "_remember_recipient_identifiers", sig2__remember_recipient_identifiers, 1 },
    { "_extract_contact_uuid", sig2__extract_contact_uuid, 1 },
    { "_quote_references_own_message", sig2__quote_references_own_message, 1 },
    { "_extract_reaction_target", sig2__extract_reaction_target, 1 },
    { "format_message", sig2__format_message, 1 },
    { "_styles_for_chunk", sig2__styles_for_chunk, 0 },
    { "_utf16_offsets", sig2__utf16_offsets, 0 },
    { "_split_signal_formatted_message", sig2__split_signal_formatted_message, 0 },
    { "_reactions_enabled", sig2__reactions_enabled, 1 },
    { "send_reaction", sig2__send_reaction, 1 },
    { "remove_reaction", sig2__remove_reaction, 1 },
    { "on_processing_start", sig2__on_processing_start, 3 },
    { "on_processing_complete", sig2__on_processing_complete, 3 },
    { "_stop_typing_indicator", sig2__stop_typing_indicator, 1 },
    { "stop_typing", sig2__stop_typing, 1 },
    { "send_typing", sig2__send_typing, 1 },
    { "_resolve_recipient", sig2__resolve_recipient, 1 },
    { "_rpc", sig2__rpc, 1 },
    { "get_chat_info", sig2__get_chat_info, 1 },
    { "send_image", sig2__send_image, 1 },
    { "_send_attachment", sig2__send_attachment, 1 },
    { "send", sig2__send, 1 },
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
        json_t *echo_in = in ? json_copy(in) : json_null();
        json_t *out = json_object();

        const sig2_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *self_in = in ? json_obj_get(in, "state") : NULL;
            json_t *event_in = args ? json_obj_get(args, "event") : NULL;
            sig2_ctx_t ctx;
            ctx.state = (self_in && self_in->type == JSON_OBJECT) ? json_copy(self_in) : json_object();
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.cursors = json_object();
            ctx.trace = json_array();
            ctx.event = (event_in && event_in->type == JSON_OBJECT) ? json_copy(event_in) : json_object();
            json_t *ret = tc->fn(&ctx, args ? args : empty_args);
            json_set(out, "ret", ret ? ret : json_null());
            json_set(out, "trace", ctx.trace);
            json_t *state = json_object();
            if (tc->flags & 1) sig2_set_copy(state, "self", ctx.state);
            if (tc->flags & 2) sig2_set_copy(state, "event", ctx.event);
            json_set(out, "state", state);
            json_free(ctx.cursors);
            json_free(ctx.state);
            json_free(ctx.event);
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