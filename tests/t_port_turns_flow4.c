/* t_port_turns_flow4.c - harness for the turns.py REMAINDER-beyond-flow3 FLOW
 * port (src/session_rules/turns_flow4.c): sentinel codec, skill-scaffold
 * describer cluster and gateway auto-load registry.
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {"args": {...},
 * "state": {"scaffolds": [...]}}}. Emits {"func", "in", "out"} where out
 * carries the decision trace (empty for this pure cluster), the final return
 * ("ret") and the mutated auto-load registry ("state.scaffolds").
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it).
 */
#include "session_rules/turns_flow4.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*tf4_fn_t)(tf4_ctx_t *, json_t *);
typedef struct { const char *func; tf4_fn_t fn; } tf4_case_t;

static const tf4_case_t CASES[] = {
    { "decode_content",                          tf4_decode_content },
    { "extract_user_instruction_from_skill_message", tf4_extract_user_instruction_from_skill_message },
    { "_describe_auto_loaded_skill_turn",        tf4_describe_auto_loaded_skill_turn },
    { "describe_skill_invocation",               tf4_describe_skill_invocation },
    { "_cut_after",                              tf4_cut_after },
    { "register_auto_load_scaffold",             tf4_register_auto_load_scaffold },
    { "_registered_auto_load_tail",              tf4_registered_auto_load_tail },
    { "_auto_load_typed_text",                   tf4_auto_load_typed_text },
};
#define CASE_COUNT (sizeof(CASES) / sizeof(CASES[0]))

static char *tf4_read_line(FILE *f) {
    size_t cap = 8192, len = 0;
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

/* Build the ctx registry from in.state.scaffolds (ordered string array). */
static void tf4_seed_scaffolds(tf4_ctx_t *ctx, const json_t *in) {
    ctx->scaffolds = json_array();
    const json_t *st = (in && in->type == JSON_OBJECT) ? json_obj_get(in, "state") : NULL;
    const json_t *sc = (st && st->type == JSON_OBJECT) ? json_obj_get(st, "scaffolds") : NULL;
    if (sc && sc->type == JSON_ARRAY) {
        size_t n = json_len(sc);
        for (size_t i = 0; i < n; i++) {
            const char *s = tf4_str(json_get(sc, i));
            json_append(ctx->scaffolds, json_string(s ? s : ""));
        }
    }
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    json_t *empty_args = json_object();
    char *line;
    while ((line = tf4_read_line(stdin)) != NULL) {
        if (!line[0]) { free(line); continue; }
        json_t *rec = json_parse(line, NULL);
        free(line);
        if (!rec || rec->type != JSON_OBJECT) { if (rec) json_free(rec); continue; }
        const char *func = json_get_str(rec, "func", "");
        json_t *in = json_obj_get(rec, "in");
        if (!in || in->type != JSON_OBJECT) in = NULL;
        json_t *echo_in = in ? json_copy(in) : json_null();
        json_t *out = json_object();

        const tf4_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
            json_set(out, "state", json_object());
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            tf4_ctx_t ctx;
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.cursors = json_object();
            ctx.trace = json_array();
            tf4_seed_scaffolds(&ctx, in);
            json_t *ret = tc->fn(&ctx, args ? args : empty_args);
            json_set(out, "ret", tf4_proj(ret));
            json_free(ret);
            json_set(out, "trace", ctx.trace);
            json_t *state = json_object();
            json_set(state, "scaffolds", ctx.scaffolds); /* ctx.scaffolds owned -> moved */
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
