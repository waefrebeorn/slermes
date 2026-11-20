/* t_port_discord_adapter_flow5.c -- harness for the Discord adapter FLOW5
 * message-construction/classification port
 * (src/gateway/platforms/discord_adapter_flow5.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the state echo ("state" -- no ported function mutates
 * state here, so it is the unchanged input copy). The echoed "in" is the
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
#include "gateway/platforms/discord_adapter_flow5.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*da5_fn_t)(da5_ctx_t *, const json_t *);
typedef struct { const char *func; da5_fn_t fn; } da5_case_t;

static const da5_case_t CASES[] = {
    { "_message_channel_ids", da5_message_channel_ids },
    { "_thread_parent_channel", da5_thread_parent_channel },
    { "_thread_created", da5_thread_created },
    { "_get_parent_channel_id", da5_get_parent_channel_id },
    { "_interaction_guild_id", da5_interaction_guild_id },
    { "_derive_auto_thread_name", da5_derive_auto_thread_name },
    { "_derive_forum_thread_name", da5_derive_forum_thread_name },
    { "_forum_thread_parts", da5_forum_thread_parts },
    { "_message_reference_from_ids", da5_message_reference_from_ids },
    { "_reply_reference_for_send", da5_reply_reference_for_send },
    { "format_tool_preview", da5_format_tool_preview },
    { "format_message", da5_format_message },
    { "_is_down_notice_content", da5_is_down_notice_content },
    { "_is_reply_reference_rejected", da5_is_reply_reference_rejected },
    { "_is_length_overflow_error", da5_is_length_overflow_error },
    { "_first_embed", da5_first_embed },
    { "_config_embed", da5_config_embed },
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

        const da5_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            da5_ctx_t ctx;
            ctx.state = (state && state->type == JSON_OBJECT) ? json_copy(state) : json_object();
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
