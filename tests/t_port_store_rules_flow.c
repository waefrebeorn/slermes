/* t_port_store_rules_flow.c - harness for the conversation-store compaction
 * rules FLOW port (src/persistence/store_rules_flow.c).
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
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it, avoiding basename collisions).
 */
#include "persistence/store_rules_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*sr_fn_t)(sr_ctx_t *, json_t *);
typedef struct { const char *func; sr_fn_t fn; const char *state_keys; } sr_case_t;

/* state_keys: comma-separated dict params echoed (post-mutation) into state. */
static const sr_case_t CASES[] = {
    { "lift_data_use", sr_lift_data_use, "message" },
    { "stamp_message_uid", sr_stamp_message_uid, "msg" },
    { "_is_checkpoint_item", sr_is_checkpoint_item, NULL },
    { "_newest_checkpoint_carrier", sr_newest_checkpoint_carrier, NULL },
    { "_sanitize_surrogates", sr_sanitize_surrogates, NULL },
    { "_localize", sr_localize, NULL },
    { "_parse_iso", sr_parse_iso, NULL },
    { "_parse_timestamp_match", sr_parse_timestamp_match, NULL },
    { "strip_leading_message_timestamps", sr_strip_leading_message_timestamps, NULL },
    { "coerce_epoch", sr_coerce_epoch, NULL },
    { "_is_image_part", sr_is_image_part, NULL },
    { "mint_uid", sr_mint_uid, NULL },
    { "uid_list", sr_uid_list, NULL },
    { "message_uid_or_none", sr_message_uid_or_none, NULL },
    { "_is_empty_content", sr_is_empty_content, NULL },
    { "drop_typed_ask", sr_drop_typed_ask, "message" },
    { "record_absorbed_message", sr_record_absorbed_message, "survivor,dropped" },
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

        const sr_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            sr_ctx_t ctx;
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.cursors = json_object();
            ctx.trace = json_array();
            json_t *ret = tc->fn(&ctx, args ? args : empty_args);
            json_set(out, "ret", ret ? sr_proj(ret) : json_null());
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
                    json_set(state, key, v ? sr_proj(v) : json_null());
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