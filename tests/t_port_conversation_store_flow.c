/* t_port_conversation_store_flow.c — harness for the conversation-store
 * carried-row coverage FLOW port (src/persistence/conversation_store_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the mutated state ("state"; coverage never mutates its
 * inputs, so it is always {}). The echoed "in" is the ORIGINAL input (the
 * oracle re-drives the live reference from it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order, or {"__script__": true, "ret": <value>, "raise": {type,...}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it, avoiding basename collisions).
 */
#include "persistence/conversation_store_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*cst_fn_t)(cst_ctx_t *, const json_t *);
typedef struct { const char *func; cst_fn_t fn; } cst_case_t;

static const cst_case_t CASES[] = {
    { "_text_of", cst_text_of },
    { "_holds_as_block", cst_holds_as_block },
    { "_images_replaced", cst_images_replaced },
    { "_carried_tail_slots", cst_carried_tail_slots },
    { "_uncounted_merged_rows", cst_uncounted_merged_rows },
    { "_tail_originals", cst_tail_originals },
    { "_index_held_dicts", cst_index_held_dicts },
    { "_replays_as", cst_replays_as },
    { "_copy_of_current_row", cst_copy_of_current_row },
    { "_claim_stored_copy", cst_claim_stored_copy },
    { "_rewrite_explains", cst_rewrite_explains },
    { "_rewritten_carry", cst_rewritten_carry },
    { "_uids_active_outside", cst_uids_active_outside },
    { "_carried_tail_ids", cst_carried_tail_ids },
    { "_matching_active_ids", cst_matching_active_ids },
    { "_matching_retired_ids", cst_matching_retired_ids },
    { "_resolve_carried_row_ids", cst_resolve_carried_row_ids },
    { "_merged_user_run", cst_merged_user_run },
    { "_proved_coverage", cst_proved_coverage },
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

        const cst_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            cst_ctx_t ctx;
            ctx.state = json_object();
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
