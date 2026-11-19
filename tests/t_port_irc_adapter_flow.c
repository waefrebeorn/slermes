/* t_port_irc_adapter_flow.c — harness for the IRC adapter FLOW port
 * (src/gateway/platforms/irc_adapter_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {"args": {...},
 * "state": {...self/conn attrs...}, "io": {...seam scripts...}}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the mutated state ("state"). The echoed "in" is the
 * ORIGINAL input (copied BEFORE dispatch, since the flow mutates ctx.state in
 * place); the oracle re-drives the live reference from it.
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order, or {"__script__": true, "ret": <value>, "raise": {type,...}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * State emission flags per case: bit0 = emit ctx.state as out.state.self
 * (the adapter self double; only _handle_line mutates self attrs).
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it, avoiding basename collisions).
 */
#include "gateway/platforms/irc_adapter_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*irc_fn_t)(irc_ctx_t *, const json_t *);
typedef struct { const char *func; irc_fn_t fn; int flags; } irc_case_t;
static const irc_case_t CASES[] = {
    { "_parse_irc_message", irc__parse_irc_message, 0 },
    { "_extract_nick", irc__extract_nick, 0 },
    { "_ms_id", irc__ms_id, 0 },
    { "_env_or_extra", irc__env_or_extra, 0 },
    { "_server_channel", irc__server_channel, 0 },
    { "_chunk_paragraph", irc__chunk_paragraph, 0 },
    { "_privmsg_budget", irc__privmsg_budget, 0 },
    { "_split_lines", irc__split_lines, 0 },
    { "_encode_line", irc__encode_line, 0 },
    { "_ssl_ctx", irc__ssl_ctx, 0 },
    { "check_requirements", irc_check_requirements, 0 },
    { "validate_config", irc_validate_config, 0 },
    { "is_connected", irc_is_connected, 0 },
    { "_env_enablement", irc__env_enablement, 0 },
    { "_strip_irc_control_chars", irc__strip_irc_control_chars, 0 },
    { "_is_irc_channel", irc__is_irc_channel, 0 },
    { "_sa_error", irc__sa_error, 0 },
    { "name", irc_name, 0 },
    { "_fail", irc__fail, 0 },
    { "_split_message", irc__split_message, 0 },
    { "_strip_markdown", irc__strip_markdown, 0 },
    { "_send_raw", irc__send_raw, 0 },
    { "_handle_line", irc__handle_line, 1 },
    { "_handle_privmsg", irc__handle_privmsg, 0 },
    { "_dispatch_message", irc__dispatch_message, 0 },
    { "send", irc_send, 0 },
    { "send_typing", irc_send_typing, 0 },
    { "get_chat_info", irc_get_chat_info, 0 },
    { "_receive_loop", irc__receive_loop, 0 },
    { "raw", irc_raw, 0 },
    { "pump", irc_pump, 0 },
    { "close", irc_close, 0 },
    { "_sa_register", irc__sa_register, 0 },
    { "_sa_join", irc__sa_join, 0 },
    { "_standalone_send", irc__standalone_send, 0 },
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

        const irc_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *self_in = in ? json_obj_get(in, "state") : NULL;
            irc_ctx_t ctx;
            ctx.state = (self_in && self_in->type == JSON_OBJECT) ? json_copy(self_in) : json_object();
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.cursors = json_object();
            ctx.trace = json_array();
            json_t *ret = tc->fn(&ctx, args ? args : empty_args);
            json_set(out, "ret", ret ? ret : json_null());
            json_set(out, "trace", ctx.trace);
            json_t *state = json_object();
            if (tc->flags & 1) irc_set_copy(state, "self", ctx.state);
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
