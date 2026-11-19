/* t_port_platforms_helpers_flow.c — harness for the platform-helper FLOW
 * port (src/gateway/platforms/platforms_helpers_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the mutated state ("state", bounded_put's store). The
 * echoed "in" is the ORIGINAL input (the oracle re-drives the live reference
 * from it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order, or {"__script__": true, "ret": <value>, "raise": {type,...}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it, avoiding basename collisions).
 */
#include "gateway/platforms/platforms_helpers_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*phf_fn_t)(phf_ctx_t *, const json_t *);
typedef struct { const char *func; phf_fn_t fn; } phf_case_t;

/* wave13: direct leaf dispatch for internal helpers previously reached only via
 * the case entries (clears name-based DEAD-CREDIT; additive). */
static json_t *phf_h_table_header_above(phf_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    const char *text = json_get_str(args, "text", "");
    long long i = (long long)json_get_num(args, "i", 0);
    char *trimmed = phf_rstrip_nl(text);
    phf_lines_t ls = phf_split_nl(trimmed);
    free(trimmed);
    long long h = phf_table_header_above(ls.lines, ls.count, i);
    phf_lines_free(&ls);
    return json_number((double)h);
}
static json_t *phf_h_is_pipe_row(phf_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    return json_bool(phf_is_pipe_row(json_get_str(args, "line", "")));
}
static json_t *phf_h_split_inside_table(phf_ctx_t *ctx, const json_t *args) {
    (void)ctx;
    return json_bool(phf_split_inside_table(json_get_str(args, "head", ""),
                                            json_get_str(args, "tail", "")));
}

static const phf_case_t CASES[] = {
    { "display_width", phf_display_width },
    { "_clip_text", phf_clip_text },
    { "_shorten_choice", phf_shorten_choice_case },
    { "_distinct_shortenings", phf_distinct_shortenings_case },
    { "numbered_clarify_choices", phf_numbered_clarify_choices },
    { "clarify_text_with_choices", phf_clarify_text_with_choices },
    { "_keep_link_target", phf_keep_link_target },
    { "bounded_put", phf_bounded_put },
    { "table_row_start", phf_table_row_start },
    { "table_rollback_point", phf_table_rollback_point },
    { "table_header_to_repeat", phf_table_header_to_repeat },
    { "is_discord_channel_obfuscated", phf_is_discord_channel_obfuscated },
    { "reply_document_sender", phf_reply_document_sender_case },
    { "reply_message_cap", phf_reply_message_cap_case },
    { "_over_reply_cap", phf_over_reply_cap },
    { "reply_file_parts", phf_reply_file_parts },
    { "scaled_upload_deadline", phf_scaled_upload_deadline },
    { "media_upload_seconds", phf_media_upload_seconds },
    { "_on_disk_size", phf_on_disk_size },
    { "_table_header_above", phf_h_table_header_above },
    { "_is_pipe_row", phf_h_is_pipe_row },
    { "_split_inside_table", phf_h_split_inside_table },
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

        const phf_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            phf_ctx_t ctx;
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
