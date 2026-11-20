/* t_port_store_identity_flow.c - harness for the conversation-store
 * message-identity codec FLOW port (src/persistence/store_identity_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the mutated dict params ("state", keyed by parameter
 * name; {} for the non-mutating funcs). The echoed "in" is the ORIGINAL input
 * (copied BEFORE the call - mutating funcs rewrite their args in place; the
 * oracle re-drives the live reference from the original).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order, or {"__script__": true, "ret": <value>, "raise": {type,...}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 * Seams: uid_list (claimed session_rules/turns.py sibling), logger.warning
 * (common.py deserialization-failure warning).
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it, avoiding basename collisions).
 */
#include "persistence/store_identity_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*si_fn_t)(si_ctx_t *, json_t *);
typedef struct { const char *func; si_fn_t fn; const char *state_keys; } si_case_t;

/* state_keys: comma-separated dict params echoed (post-mutation) into state. */
static const si_case_t CASES[] = {
    { "_live_or_column", si_live_or_column, NULL },
    { "_uid_list", si_uid_list, NULL },
    { "_uid_map", si_uid_map, NULL },
    { "_tool_call_uid_map", si_tool_call_uid_map, NULL },
    { "_absorbed_uids_json", si_absorbed_uids_json, NULL },
    { "_tool_call_uids_json", si_tool_call_uids_json, NULL },
    { "_tool_call_uid_or_none", si_tool_call_uid_or_none, NULL },
    { "_live_extends", si_live_extends, NULL },
    { "_fill_missing_tool_call_uids", si_fill_missing_tool_call_uids, "msg" },
    { "_restore_row_identity", si_restore_row_identity, "msg" },
    { "_restore_identity_columns", si_restore_identity_columns, "msg" },
    { "_stable_tool_key", si_stable_tool_key, NULL },
    { "_json_or", si_json_or, NULL },
    { "coalesce_tool_call_id", si_coalesce_tool_call_id, NULL },
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

        const si_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            si_ctx_t ctx;
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.cursors = json_object();
            ctx.trace = json_array();
            json_t *ret = tc->fn(&ctx, args ? args : empty_args);
            json_set(out, "ret", ret ? si_proj(ret) : json_null());
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
                    json_set(state, key, v ? si_proj(v) : json_null());
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
