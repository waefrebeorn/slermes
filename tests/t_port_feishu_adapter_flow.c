/* t_port_feishu_adapter_flow.c — harness for the Feishu adapter
 * message-normalization FLOW port
 * (src/gateway/platforms/feishu_adapter_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {"args": {...}, "io": {...}}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the mutated accumulator state ("state"). The echoed
 * "in" is the ORIGINAL input (copied BEFORE dispatch, since the renderer /
 * parse mutate the args accumulators in place); the oracle re-drives the live
 * reference from it.
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order, or {"__script__": true, "ret": <value>, "raise": {type,...}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * State emission: parse_feishu_post_payload emits {mentions_map}; the
 * renderer trio (_render_post_element / _join_nested_posts /
 * _render_nested_post) emits {image_keys, media_refs, mentions_map}; every
 * other func emits {} (its observable output is entirely in ret).
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it, avoiding basename collisions).
 */
#include "gateway/platforms/feishu_adapter_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*fa_fn_t)(fa_ctx_t *, const json_t *);
typedef struct { const char *func; fa_fn_t fn; int accum; } fa_case_t;
/* accum: 0 = state {}; 1 = trio {image_keys,media_refs,mentions_map};
 *        2 = parse {mentions_map} */
static const fa_case_t CASES[] = {
    { "_is_bot_sender", fa__is_bot_sender, 0 },
    { "_sender_identity", fa__sender_identity, 0 },
    { "_escape_markdown_text", fa__escape_markdown_text, 0 },
    { "_to_boolean", fa__to_boolean, 0 },
    { "_is_style_enabled", fa__is_style_enabled, 0 },
    { "_wrap_inline_code", fa__wrap_inline_code, 0 },
    { "_sanitize_fence_language", fa__sanitize_fence_language, 0 },
    { "_coerce_int", fa__coerce_int, 0 },
    { "_to_post_payload", fa__to_post_payload, 0 },
    { "_load_feishu_payload", fa__load_feishu_payload, 0 },
    { "_normalize_feishu_text", fa__normalize_feishu_text, 0 },
    { "_unique_lines", fa__unique_lines, 0 },
    { "_extract_mention_ids", fa__extract_mention_ids, 0 },
    { "_build_mention_hint", fa__build_mention_hint, 0 },
    { "_strip_edge_self_mentions", fa__strip_edge_self_mentions, 0 },
    { "_build_markdown_post_rows", fa__build_markdown_post_rows, 0 },
    { "matches", fa_matches, 0 },
    { "_walk_nodes", fa__walk_nodes, 0 },
    { "_render_text_element", fa__render_text_element, 0 },
    { "_render_code_block_element", fa__render_code_block_element, 0 },
    { "_render_post_element", fa__render_post_element, 1 },
    { "_join_nested_posts", fa__join_nested_posts, 1 },
    { "_render_nested_post", fa__render_nested_post, 1 },
    { "parse_feishu_post_payload", fa_parse_feishu_post_payload, 2 },
    { "_resolve_post_payload", fa__resolve_post_payload, 0 },
    { "_resolve_locale_payload", fa__resolve_locale_payload, 0 },
    { "_build_markdown_post_payload", fa__build_markdown_post_payload, 0 },
    { "_coerce_required_int", fa__coerce_required_int, 0 },
    { "_strip_markdown_to_plain_text", fa__strip_markdown_to_plain_text, 0 },
    { "_attachment_placeholder", fa__attachment_placeholder, 0 },
    { "_first_non_empty_text", fa__first_non_empty_text, 0 },
    { "_find_header_title", fa__find_header_title, 0 },
    { "_find_first_text", fa__find_first_text, 0 },
    { "_first_text_field", fa__first_text_field, 0 },
    { "_build_media_ref_from_payload", fa__build_media_ref_from_payload, 0 },
    { "_collect_text_segments", fa__collect_text_segments, 0 },
    { "_collect_card_lines", fa__collect_card_lines, 0 },
    { "_collect_action_labels", fa__collect_action_labels, 0 },
    { "_collect_forward_entries", fa__collect_forward_entries, 0 },
    { "_normalize_merge_forward_message", fa__normalize_merge_forward_message, 0 },
    { "_normalize_share_chat_message", fa__normalize_share_chat_message, 0 },
    { "_normalize_interactive_message", fa__normalize_interactive_message, 0 },
    { "_build_mentions_map", fa__build_mentions_map, 0 },
    { "normalize_feishu_message", fa_normalize_feishu_message, 0 },
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
        /* echo the ORIGINAL input before dispatch mutates accumulators */
        json_t *echo_in = in ? json_copy(in) : json_null();
        json_t *out = json_object();

        const fa_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            fa_ctx_t ctx;
            ctx.state = json_object();
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.cursors = json_object();
            ctx.trace = json_array();
            json_t *ret = tc->fn(&ctx, args ? args : empty_args);
            json_set(out, "ret", ret ? ret : json_null());
            json_set(out, "trace", ctx.trace);
            json_t *state = json_object();
            if (tc->accum == 1) {
                fa_set_copy(state, "image_keys", args ? json_obj_get(args, "image_keys") : NULL);
                fa_set_copy(state, "media_refs", args ? json_obj_get(args, "media_refs") : NULL);
                fa_set_copy(state, "mentions_map", args ? json_obj_get(args, "mentions_map") : NULL);
            } else if (tc->accum == 2) {
                fa_set_copy(state, "mentions_map", args ? json_obj_get(args, "mentions_map") : NULL);
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