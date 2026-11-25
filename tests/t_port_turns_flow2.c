/* t_port_turns_flow2.c - harness for the unclaimed turns.py pure-helper FLOW
 * port (src/session_rules/turns_flow2.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {"args": {...}, "io": {...}}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events in "trace"), the final return ("ret") and the
 * mutated mapping params ("state", keyed by parameter name; {} for the rest).
 * The echoed "in" is the ORIGINAL input copied BEFORE the call (the one
 * mutating func, index_tool_call_uids, rewrites its index arg in place).
 *
 * Scripted I/O lives in in.io (map seam-name -> result); this cluster has no
 * external collaborators (all leaves call each other directly), so io is
 * carried for contract parity only. A missing script is a fail-closed
 * RuntimeError on both sides (never exercised by this cluster's fixtures).
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it).
 */
#include "session_rules/turns_flow2.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*tf2_fn_t)(tf2_ctx_t *, json_t *);
typedef struct { const char *func; tf2_fn_t fn; const char *state_keys; } tf2_case_t;

/* state_keys: comma-separated dict params echoed (post-mutation) into state. */
static const tf2_case_t CASES[] = {
    { "_uid_occurrences",                tf2_uid_occurrences,               NULL },
    { "merge_tool_call_uids",            tf2_merge_tool_call_uids,          NULL },
    { "_tc_field",                       tf2_tc_field,                      NULL },
    { "_expand_tool_id_variants",        tf2_expand_tool_id_variants,       NULL },
    { "tool_call_id_variants",           tf2_tool_call_id_variants,         NULL },
    { "tool_result_id_variants",         tf2_tool_result_id_variants,       NULL },
    { "per_occurrence_tool_call_uids",   tf2_per_occurrence_tool_call_uids, NULL },
    { "index_tool_call_uids",            tf2_index_tool_call_uids,          "index" },
    { "resolve_tool_call_uid",           tf2_resolve_tool_call_uid,         NULL },
    { "retired_row_payload",             tf2_retired_row_payload,           NULL },
    { "_is_multimodal_tool_result",      tf2_is_multimodal_tool_result,     NULL },
    { "_is_text_part",                   tf2_is_text_part,                  NULL },
    { "_multimodal_text_summary",        tf2_multimodal_text_summary,       NULL },
    { "_is_ephemeral_scaffolding",       tf2_is_ephemeral_scaffolding,      NULL },
    { "_image_marker",                   tf2_image_marker,                  NULL },
    { "_durable_content",                tf2_durable_content,               NULL },
    { "_content_has_payload",            tf2_content_has_payload,           NULL },
    { "_msg_has_payload",                 tf2_msg_has_payload,               NULL },
    { "_is_codex_interim",                tf2_is_codex_interim,              NULL },
    { "_json_or",                         tf2_json_or,                       NULL },
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
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
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

        const tf2_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            tf2_ctx_t ctx;
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.cursors = json_object();
            ctx.trace = json_array();
            json_t *ret = tc->fn(&ctx, args ? args : empty_args);
            json_set(out, "ret", tf2_proj(ret));
            json_free(ret);
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
                    json_set(state, key, v ? tf2_proj(v) : json_null());
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
