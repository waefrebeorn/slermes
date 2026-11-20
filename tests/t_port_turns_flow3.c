/* t_port_turns_flow3.c - harness for the remaining-unclaimed turns.py FLOW
 * port (src/session_rules/turns_flow3.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {"args": {...}}}.
 * Emits JSON LINES: {"func", "in", "out"} where out carries the decision
 * trace (empty for this pure cluster), the final return ("ret") and an empty
 * "state" map (no mutating params in this cluster).
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it).
 */
#include "session_rules/turns_flow3.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*tf3_fn_t)(tf3_ctx_t *, json_t *);
typedef struct { const char *func; tf3_fn_t fn; } tf3_case_t;

static const tf3_case_t CASES[] = {
    { "_starts_with_summary_prefix",        tf3_starts_with_summary_prefix },
    { "classify_summary_content",           tf3_classify_summary_content },
    { "_is_context_summary_content",        tf3_is_context_summary_content },
    { "_has_compressed_summary_metadata",   tf3_has_compressed_summary_metadata },
    { "_is_context_summary_message",        tf3_is_context_summary_message },
    { "_is_synthetic_compression_user_turn",tf3_is_synthetic_compression_user_turn },
    { "_is_blank_user_turn",                tf3_is_blank_user_turn },
    { "_is_actionable_user_turn",           tf3_is_actionable_user_turn },
    { "_is_real_user_turn",                 tf3_is_real_user_turn },
    { "is_compaction_summary_message",      tf3_is_compaction_summary_message },
    { "_strip_context_summary_handoff_message", tf3_strip_context_summary_handoff_message },
    { "_handoff_only_content",              tf3_handoff_only_content },
    { "split_user_originated_turn",         tf3_split_user_originated_turn },
    { "user_originated_turn_view",          tf3_user_originated_turn_view },
    { "history_before_user_originated_turn",tf3_history_before_user_originated_turn },
    { "project_compaction_message_for_display", tf3_project_compaction_message_for_display },
    { "user_indices",                       tf3_user_indices },
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
    setvbuf(stdout, NULL, _IONBF, 0);
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

        const tf3_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            tf3_ctx_t ctx;
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.cursors = json_object();
            ctx.trace = json_array();
            json_t *ret = tc->fn(&ctx, args ? args : empty_args);
            json_set(out, "ret", tf3_proj(ret));
            json_free(ret);
            json_set(out, "trace", ctx.trace);
            json_set(out, "state", json_object());
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
