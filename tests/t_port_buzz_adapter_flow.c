/* t_port_buzz_adapter_flow.c — harness for the Buzz adapter FLOW port
 * (src/gateway/platforms/buzz_adapter_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {"args": {...},
 * "state": {...self attrs...}, "io": {...seam scripts...}}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the mutated state ("state"). The echoed "in" is the
 * ORIGINAL input (copied BEFORE dispatch, since the state cluster mutates
 * args.state / ctx.state in place); the oracle re-drives the live reference
 * from it.
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order, or {"__script__": true, "ret": <value>, "raise": {type,...}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * State emission flags per case: bit0 = emit ctx.state as out.state.self
 * (the adapter self double); bit1 = emit args.state as out.state.state (the
 * channel-state dict); bit2 = emit args.bucket as out.state.bucket.
 *
 * The port .c is #included directly by area-qualified path (self-contained,
 * libjson only; -I src resolves it, avoiding basename collisions).
 */
#include "gateway/platforms/buzz_adapter_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*ba_fn_t)(ba_ctx_t *, const json_t *);
typedef struct { const char *func; ba_fn_t fn; int flags; } ba_case_t;

static const ba_case_t CASES[] = {
    { "_split_csv", ba__split_csv, 0 },
    { "_add_pubkey", ba__add_pubkey, 4 },
    { "_normalize_user_ref", ba__normalize_user_ref, 0 },
    { "_pubkey_set", ba__pubkey_set, 0 },
    { "_ttl_get", ba__ttl_get, 0 },
    { "_credentials_key", ba__credentials_key, 0 },
    { "_bounded_cli_message", ba__bounded_cli_message, 0 },
    { "_json_or", ba__json_or, 0 },
    { "_cli_error_message", ba__cli_error_message, 0 },
    { "_parse_send_receipt", ba__parse_send_receipt, 0 },
    { "_parse_json_list", ba__parse_json_list, 0 },
    { "_attachment_rejection_note", ba__attachment_rejection_note, 0 },
    { "_e_tags", ba__e_tags, 0 },
    { "_event_reply_parent_id", ba__event_reply_parent_id, 0 },
    { "_p_tagged", ba__p_tagged, 0 },
    { "_scoped_platform_setting", ba__scoped_platform_setting, 0 },
    { "_setting_or", ba__setting_or, 0 },
    { "_configured_relay", ba__configured_relay, 0 },
    { "_configured_home_channel", ba__configured_home_channel, 0 },
    { "_configured_credentials_file", ba__configured_credentials_file, 0 },
    { "_reply_to_mode", ba__reply_to_mode, 0 },
    { "_resolve_credentials_data", ba__resolve_credentials_data, 0 },
    { "_resolve_private_key", ba__resolve_private_key, 0 },
    { "normalize_user_id", ba_normalize_user_id, 0 },
    { "_extract_thread_root", ba__extract_thread_root, 0 },
    { "_store_event_meta", ba__store_event_meta, 2 },
    { "_lookup_event_meta", ba__lookup_event_meta, 0 },
    { "_trim_seen", ba__trim_seen, 2 },
    { "_remember_event", ba__remember_event, 2 },
    { "_remember_event_meta", ba__remember_event_meta, 1 },
    { "_new_channel_state", ba__new_channel_state, 0 },
    { "_restore_channel_state", ba__restore_channel_state, 1 },
    { "_mark_seen", ba__mark_seen, 1 },
    { "_record_thread_root", ba__record_thread_root, 1 },
    { "_resolve_reply_anchor", ba__resolve_reply_anchor, 1 },
    { "_reply_args", ba__reply_args, 1 },
    { "_may_reclassify_as_dm", ba__may_reclassify_as_dm, 1 },
    { "_p_tagged_to_self", ba__p_tagged_to_self, 1 },
    { "_is_mentioned", ba__is_mentioned, 1 },
    { "_is_addressed", ba__is_addressed, 1 },
    { "_is_direct_message_event", ba__is_direct_message_event, 1 },
    { "_maybe_latch_dm", ba__maybe_latch_dm, 3 },
    { "_strip_mention", ba__strip_mention, 1 },
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

        const ba_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *self_in = in ? json_obj_get(in, "state") : NULL;
            ba_ctx_t ctx;
            ctx.state = (self_in && self_in->type == JSON_OBJECT) ? json_copy(self_in) : json_object();
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.cursors = json_object();
            ctx.trace = json_array();
            json_t *ret = tc->fn(&ctx, args ? args : empty_args);
            json_set(out, "ret", ret ? ret : json_null());
            json_set(out, "trace", ctx.trace);
            json_t *state = json_object();
            if (tc->flags & 1) ba_set_copy(state, "self", ctx.state);
            if (tc->flags & 2) ba_set_copy(state, "state", args ? json_obj_get(args, "state") : NULL);
            if (tc->flags & 4) ba_set_copy(state, "bucket", args ? json_obj_get(args, "bucket") : NULL);
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
