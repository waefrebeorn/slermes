/* t_port_matrix_adapter_flow5.c -- harness for the Matrix-adapter mention /
 * classification / config helper flow port
 * (src/gateway/platforms/matrix_adapter_flow5.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the ordered seam
 * events {"name","args","ret"} in "trace", the final return ("ret") and the
 * state echo ("state"). The echoed "in" is the ORIGINAL input (the oracle
 * re-drives the live reference from it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order, or {"__script__": true, "ret": <value>, "raise": {type,...}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it, avoiding basename collisions).
 */
#include "gateway/platforms/matrix_adapter_flow5.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*ma5_fn_t)(ma5_ctx_t *, const json_t *);
typedef struct { const char *func; ma5_fn_t fn; } ma5_case_t;

static const ma5_case_t CASES[] = {
    { "_run_media_tool", ma5_run_media_tool },
    { "_apply_yaml_config", ma5_apply_yaml_config },
    { "_is_connected", ma5_is_connected },
    { "_recovery_key_output_path", ma5_recovery_key_output_path },
    { "_content_mentions_bot", ma5_content_mentions_bot },
    { "_is_bot_mentioned", ma5_is_bot_mentioned },
    { "_has_outbound_room_mention", ma5_has_outbound_room_mention },
    { "_extract_outbound_mentions", ma5_extract_outbound_mentions },
    { "_inject_outbound_mention_links", ma5_inject_outbound_mention_links },
    { "_voice_may_park", ma5_voice_may_park },
    { "_strip_mention", ma5_strip_mention },
    { "_classify_inbound_media", ma5_classify_inbound_media },
    { "get_chat_info", ma5_get_chat_info },
    { "_query_own_device_keys", ma5_query_own_device_keys },
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

        const ma5_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            ma5_ctx_t ctx;
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
