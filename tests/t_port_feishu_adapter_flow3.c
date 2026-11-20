/* t_port_feishu_adapter_flow3.c — harness for the Feishu adapter
 * inbound-admission FLOW remainder port
 * (src/gateway/platforms/feishu_adapter_flow3.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {"args": {...}, "io": {...}}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the mutated state ("state"). The echoed "in" is the
 * ORIGINAL input (copied BEFORE dispatch, since the mutating func edits
 * args.self in place); the oracle re-drives the live reference from it.
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order, or {"__script__": true, "ret": <value>, "raise": {type,...}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * State emission: _warn_once_empty_allowlist_deny emits
 * {_warned_empty_allowlist_deny}; every other func emits {} (its observable
 * output is entirely in ret/trace).
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it, avoiding basename collisions).
 */
#include "gateway/platforms/feishu_adapter_flow3.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*fa3_fn_t)(fa3_ctx_t *, const json_t *);
typedef struct { const char *func; fa3_fn_t fn; int accum; } fa3_case_t;
/* accum: 0 = state {}; 4 = {_warned_empty_allowlist_deny} */
static const fa3_case_t CASES[] = {
    { "_admit", fa3__admit, 0 },
    { "_require_mention_for", fa3__require_mention_for, 0 },
    { "_allow_group_message", fa3__allow_group_message, 0 },
    { "_mentions_self", fa3__mentions_self, 0 },
    { "_message_mentions_bot", fa3__message_mentions_bot, 0 },
    { "_post_mentions_bot", fa3__post_mentions_bot, 0 },
    { "_bot_identity", fa3__bot_identity, 0 },
    { "_warn_once_empty_allowlist_deny", fa3__warn_once_empty_allowlist_deny, 4 },
};
#define CASE_COUNT (sizeof(CASES) / sizeof(CASES[0]))

static char *fa3h_read_line(FILE *f) {
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
    while ((line = fa3h_read_line(stdin)) != NULL) {
        if (!line[0]) { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec || rec->type != JSON_OBJECT) { if (rec) json_free(rec); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        if (!in || in->type != JSON_OBJECT) in = NULL;
        /* echo the ORIGINAL input before dispatch mutates self */
        json_t *echo_in = in ? json_copy(in) : json_null();
        json_t *out = json_object();

        const fa3_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            fa3_ctx_t ctx;
            ctx.state = json_object();
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.cursors = json_object();
            ctx.trace = json_array();
            json_t *ret = tc->fn(&ctx, args ? args : empty_args);
            json_set(out, "ret", ret ? ret : json_null());
            json_set(out, "trace", ctx.trace);
            json_t *state = json_object();
            const json_t *self = args ? json_obj_get(args, "self") : NULL;
            if (tc->accum == 4) {
                const json_t *warned = self ? json_obj_get(self, "_warned_empty_allowlist_deny") : NULL;
                json_set(state, "_warned_empty_allowlist_deny",
                         warned ? json_copy(warned) : json_bool(false));
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
