/* t_port_store_rules_flow2.c - harness for the unclaimed turn-policy leaves FLOW
 * port (src/persistence/store_rules_flow2.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the mutated dict params ("state", keyed by parameter
 * name; {} for the non-mutating funcs). The echoed "in" is the ORIGINAL input
 * (copied BEFORE the call; mutating funcs rewrite their args in place).
 *
 * Scripted I/O lives in in.io (map seam-name -> result); unused by this cluster
 * (all collaborators are direct leaves) but carried for contract parity.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it).
 */
#include "persistence/store_rules_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*sr2_fn_t)(sr2_ctx_t *, json_t *);
typedef struct { const char *func; sr2_fn_t fn; const char *state_keys; } sr2_case_t;

/* state_keys: comma-separated dict params echoed (post-mutation) into state. */
static const sr2_case_t CASES[] = {
    { "_field",                     sr2_field,                     NULL },
    { "_part_text",                 sr2_part_text,                 NULL },
    { "_with_part_text",            sr2_with_part_text,            NULL },
    { "_content_text_for_contains", sr2_content_text_for_contains, NULL },
    { "is_content_blank",           sr2_is_content_blank,          NULL },
    { "drop_stale_api_content",     sr2_drop_stale_api_content,    "msg" },
    { "_display_metadata_str",      sr2_display_metadata_str,      NULL },
    { "withheld_prompt_text",       sr2_withheld_prompt_text,      NULL },
    { "typed_prompt_content",       sr2_typed_prompt_content,      NULL },
    { "retryable_user_text",        sr2_retryable_user_text,       NULL },
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
        json_t *echo_in = in ? json_copy(in) : json_null();
        json_t *out = json_object();

        const sr2_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            sr2_ctx_t ctx;
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.cursors = json_object();
            ctx.trace = json_array();
            json_t *ret = tc->fn(&ctx, args ? args : empty_args);
            json_set(out, "ret", ret ? sr2_proj(ret) : json_null());
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
                    json_set(state, key, v ? sr2_proj(v) : json_null());
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
