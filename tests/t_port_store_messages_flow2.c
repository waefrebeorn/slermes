/* t_port_store_messages_flow2.c - harness for the unclaimed pure-leaf FLOW
 * port (src/persistence/store_messages_flow2.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out carries the decision
 * SEQUENCE ("trace", ordered seam events; empty for this pure cluster), the
 * final return ("ret"), and the mutated dict params ("state", keyed by
 * parameter name). The echoed "in" is copied BEFORE dispatch — mutating leaves
 * (sm2_fold_summary_carrier) rewrite their args in place.
 *
 * Scripted I/O lives in in.io (map seam-name -> result); unused here but
 * carried for contract parity. A missing/exhausted script fails closed on
 * both sides. The port .c is #included directly by area-qualified path.
 */
#include "persistence/store_messages_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*sm2_fn_t)(sm2_ctx_t *, json_t *);
typedef struct { const char *func; sm2_fn_t fn; const char *state_keys; } sm2_case_t;

/* state_keys: comma-separated dict params echoed (post-mutation) into state. */
static const sm2_case_t CASES[] = {
    { "_reasoning_json_text",           sm2_reasoning_json_text,           NULL },
    { "_encode_content",                sm2_encode_content,                NULL },
    { "_encode_display_metadata",       sm2_encode_display_metadata,       NULL },
    { "_decode_display_metadata",       sm2_decode_display_metadata,       NULL },
    { "_reaction_list",                 sm2_reaction_list,                 NULL },
    { "_fold_summary_carrier",          sm2_fold_summary_carrier,          "first_by_uid" },
    { "_active_clause",                 sm2_active_clause,                 NULL },
    { "_exact_replayed_user_clone_key", sm2_exact_replayed_user_clone_key, NULL },
    { "_is_model_only_row",             sm2_is_model_only_row,             NULL },
    { "_is_explicit_fork_child_row",    sm2_is_explicit_fork_child_row,    NULL },
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
        /* echo the ORIGINAL input before any in-place mutation */
        json_t *echo_in = in ? json_copy(in) : json_null();
        json_t *out = json_object();

        const sm2_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            sm2_ctx_t ctx;
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.cursors = json_object();
            ctx.trace = json_array();
            json_t *ret = tc->fn(&ctx, args ? args : empty_args);
            json_set(out, "ret", ret ? sm2_proj(ret) : json_null());
            if (ret) json_free(ret);
            json_set(out, "trace", ctx.trace);
            json_t *state = json_object();
            if (tc->state_keys && args) {
                const char *k = tc->state_keys;
                while (*k) {
                    const char *comma = strchr(k, ',');
                    size_t klen = comma ? (size_t)(comma - k) : strlen(k);
                    char key[64];
                    if (klen >= sizeof key) klen = sizeof key - 1;
                    memcpy(key, k, klen);
                    key[klen] = '\0';
                    json_t *v = json_obj_get(args, key);
                    json_set(state, key, v ? sm2_proj(v) : json_null());
                    k = comma ? comma + 1 : k + klen;
                }
            }
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
