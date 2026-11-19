/* t_port_signal_adapter_flow.c — harness for the Signal adapter FLOW port
 * (src/gateway/platforms/signal_adapter_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {"args": {...},
 * "state": {...self attrs...}, "io": {...seam scripts...}}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the mutated state ("state"). The echoed "in" is the
 * ORIGINAL input (copied BEFORE dispatch, since the flow mutates ctx.state /
 * args.params in place); the oracle re-drives the live reference from it.
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order, or {"__script__": true, "ret": <value>, "raise": {type,...}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * State emission flags per case: bit0 = emit ctx.state as out.state.self
 * (the adapter self double). Instance-method targets set bit0; the module
 * leaf (_ext_to_mime) and the staticmethod (_extract_quote_author) do not.
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it, avoiding basename collisions).
 */
#include "gateway/platforms/signal_adapter_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*sig_fn_t)(sig_ctx_t *, const json_t *);
typedef struct { const char *func; sig_fn_t fn; int flags; } sig_case_t;

static const sig_case_t CASES[] = {
    { "_close_client", sig__close_client, 1 },
    { "_handle_sse_line", sig__handle_sse_line, 1 },
    { "_unwrap_sync_message", sig__unwrap_sync_message, 1 },
    { "_apply_group_mention_rules", sig__apply_group_mention_rules, 1 },
    { "_collect_attachments", sig__collect_attachments, 1 },
    { "_group_allowed", sig__group_allowed, 1 },
    { "_with_target", sig__with_target, 1 },
    { "_rpc_send", sig__rpc_send, 1 },
    { "_resolve_image_path", sig__resolve_image_path, 1 },
    { "_send_attachment_batch", sig__send_attachment_batch, 1 },
    { "_send_file", sig__send_file, 1 },
    { "_send_reaction_rpc", sig__send_reaction_rpc, 1 },
    { "_validate_send_result", sig__validate_send_result, 1 },
    { "_track_sent_timestamp", sig__track_sent_timestamp, 1 },
    { "_remember_sent_message_timestamp", sig__remember_sent_message_timestamp, 1 },
    { "_consume_sent_timestamp", sig__consume_sent_timestamp, 1 },
    { "_extract_quote_author", sig__extract_quote_author, 0 },
    { "_ext_to_mime", sig__ext_to_mime, 0 },
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

        const sig_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *self_in = in ? json_obj_get(in, "state") : NULL;
            sig_ctx_t ctx;
            ctx.state = (self_in && self_in->type == JSON_OBJECT) ? json_copy(self_in) : json_object();
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.cursors = json_object();
            ctx.trace = json_array();
            json_t *ret = tc->fn(&ctx, args ? args : empty_args);
            json_set(out, "ret", ret ? ret : json_null());
            json_set(out, "trace", ctx.trace);
            json_t *state = json_object();
            if (tc->flags & 1) sig_set_copy(state, "self", ctx.state);
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