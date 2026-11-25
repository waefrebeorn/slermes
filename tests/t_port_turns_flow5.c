/* t_port_turns_flow5.c - harness for the turns.py REMAINDER-beyond-flow4 FLOW
 * port (src/session_rules/turns_flow5.c): regex scrubber / view projection,
 * payload predicates, absorbed-row bookkeeping, and the 4-pass sequence-repair
 * orchestrator.
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {"args": {...}}}.
 * Entry points MUTATE args in place (the Python defs mutate shared dicts/lists)
 * and return their Python return value. Emits {"func", "in" (echo), "out"} where
 * out = {"ret": <return>, "trace": [], "state": {"args": <mutated args>}}.
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it).
 */
#include "session_rules/turns_flow5.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*tf5_fn_t)(tf5_ctx_t *, json_t *);
typedef struct { const char *func; tf5_fn_t fn; } tf5_case_t;

static const tf5_case_t CASES[] = {
    { "sanitize_context",                    tf5_sanitize_context },
    { "_loaded_view_content",                tf5_loaded_view_content },
    { "comparison_content",                  tf5_comparison_content },
    { "_content_has_payload",                tf5_content_has_payload },
    { "_msg_has_payload",                    tf5_msg_has_payload },
    { "_merge_assistant_into",               tf5_merge_assistant_into },
    { "_remember_absorbed_row",              tf5_remember_absorbed_row },
    { "_count_unnamed_row",                  tf5_count_unnamed_row },
    { "_remember_own_row",                   tf5_remember_own_row },
    { "_retire_dropped_row",                 tf5_retire_dropped_row },
    { "_retire_leading_drops",               tf5_retire_leading_drops },
    { "_merge_consecutive_assistants",       tf5_merge_consecutive_assistants },
    { "_drop_stray_tool_results",            tf5_drop_stray_tool_results },
    { "_prune_unanswered_tool_calls",        tf5_prune_unanswered_tool_calls },
    { "_merge_consecutive_users",            tf5_merge_consecutive_users },
    { "_normalize_sentinel_encoded_content", tf5_normalize_sentinel_encoded_content },
    { "repair_message_sequence",             tf5_repair_message_sequence },
};
#define CASE_COUNT (sizeof(CASES) / sizeof(CASES[0]))

static char *tf5_read_line(FILE *f) {
    size_t cap = 8192, len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    int ch;
    while ((ch = fgetc(f)) != EOF && ch != '\n') {
        if (len + 1 >= cap) {
            cap *= 2; char *nb = realloc(buf, cap);
            if (!nb) { free(buf); return NULL; } buf = nb;
        }
        buf[len++] = (char)ch;
    }
    if (ch == EOF && len == 0) { free(buf); return NULL; }
    buf[len] = '\0';
    return buf;
}

int main(void) {
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    json_t *empty_args = json_object();
    char *line;
    while ((line = tf5_read_line(stdin)) != NULL) {
        if (!line[0]) { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec || rec->type != JSON_OBJECT) { if (rec) json_free(rec); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        if (!in || in->type != JSON_OBJECT) in = NULL;
        json_t *echo_in = in ? json_copy(in) : json_null();
        json_t *out = json_object();

        const tf5_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++)
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
            json_set(out, "trace", json_array());
            json_set(out, "state", json_object());
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = empty_args;
            tf5_ctx_t ctx;
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.cursors = json_object();
            ctx.trace = json_array();
            json_t *ret = tc->fn(&ctx, args);
            json_set(out, "ret", tf5_proj(ret));
            json_free(ret);
            json_set(out, "trace", ctx.trace);
            json_t *state = json_object();
            json_set(state, "args", json_copy(args)); /* mutated args */
            json_set(out, "state", state);
            json_free(ctx.cursors);
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
