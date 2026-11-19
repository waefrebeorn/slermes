/* t_port_slack_adapter_flow.c — harness for the Slack plugin-adapter FLOW
 * port (src/gateway/platforms/slack_adapter_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the mutated state ("state", _trim_slack_dm_cache's
 * dm_cache). The echoed "in" is the ORIGINAL input (the oracle re-drives the
 * live reference from it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order, or {"__script__": true, "ret": <value>, "raise": {type,...}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it, avoiding basename collisions).
 */
#include "gateway/platforms/slack_adapter_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*sa_fn_t)(sa_ctx_t *, const json_t *);
typedef struct { const char *func; sa_fn_t fn; } sa_case_t;

static const sa_case_t CASES[] = {
    { "_slack_unfurl_kwargs", sa_slack_unfurl_kwargs },
    { "_is_table_row", sa_is_table_row },
    { "_disp_width", sa_disp_width_case },
    { "_pad", sa_pad },
    { "_split_table_row", sa_split_table_row },
    { "_align_table", sa_align_table },
    { "_wrap_markdown_tables", sa_wrap_markdown_tables },
    { "_collect_slack_block_mentions", sa_collect_slack_block_mentions },
    { "_slack_mention_detection_text", sa_slack_mention_detection_text },
    { "_slack_permalink_path", sa_slack_permalink_path },
    { "_str_or_empty", sa_str_or_empty },
    { "_int_or_zero", sa_int_or_zero },
    { "_first_truthy", sa_first_truthy },
    { "_slack_str_field", sa_slack_str_field },
    { "_render_slack_inline_element", sa_render_slack_inline_element },
    { "_render_inline_elements", sa_render_inline_elements },
    { "_unescape_slack_entities", sa_unescape_slack_entities },
    { "_resolve_slack_audio_ext", sa_resolve_slack_audio_ext },
    { "_is_slack_voice_clip", sa_is_slack_voice_clip },
    { "_attachment_label", sa_attachment_label },
    { "_standalone_post_kwargs", sa_standalone_post_kwargs },
    { "_slack_response_payload", sa_slack_response_payload },
    { "_model_picker_expired_notice", sa_model_picker_expired_notice },
    { "_rewrite_known_bang_command", sa_rewrite_known_bang_command },
    { "_slack_dedup_ttl_seconds", sa_slack_dedup_ttl_seconds },
    { "_resolve_slack_proxy_url", sa_resolve_slack_proxy_url },
    { "_trim_slack_dm_cache", sa_trim_slack_dm_cache },
    { "_load_slack_bot_tokens", sa_load_slack_bot_tokens },
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

        const sa_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set(out, "error", json_string("unknown func"));
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            sa_ctx_t ctx;
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
        json_set(echo, "func", json_string(func));
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