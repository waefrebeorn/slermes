/* t_port_run_inbound_flow.c — harness for the gateway/run_inbound.py FLOW port
 * (src/gateway/run_inbound_flow.c).
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), and the mutated self/world state ("state", including the
 * final event double as "event_out"). The echoed "in" is the ORIGINAL input
 * (the oracle re-drives the live reference from it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either a
 * plain JSON value (returned verbatim on every call), a LIST consumed in call
 * order, or {"__script__": true, "ret": <value>, "raise": {type,...}}. A
 * missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * The port .c is #included directly (self-contained, libjson only).
 */
#include "gateway/run_inbound_flow.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*ri_fn_t)(ri_ctx_t *, const json_t *);
typedef struct { const char *func; ri_fn_t fn; } ri_case_t;

static const ri_case_t CASES[] = {
    { "_prompt_admin_ids", ri_prompt_admin_ids },
    { "_hm_update_reply_refused", ri_update_reply_refused },
    { "_refused_clarify", ri_refused_clarify },
    { "_refused_slash_confirm", ri_refused_slash_confirm },
    { "_slash_confirm_text_choices", ri_slash_confirm_text_choices },
    { "_release_pending_clarify", ri_release_pending_clarify },
    { "_release_clarify_owning_turn", ri_release_clarify_owning_turn },
    { "_hm_evict_running_agent", ri_evict_running_agent },
    { "_hm_evict_reaped_agent", ri_evict_reaped_agent },
    { "_hm_merge_pending_for_source", ri_merge_pending_for_source },
    { "_hm_drop_held_input", ri_drop_held_input },
    { "_hm_text_only", ri_text_only },
    { "_hm_in_telegram_grace", ri_in_telegram_grace },
    { "_hm_busy_telegram_grace_queue", ri_busy_telegram_grace_queue },
    { "_hm_busy_steer", ri_busy_steer },
    { "_hm_busy_interrupt", ri_busy_interrupt },
    { "_clarify_takeover", ri_clarify_takeover },
    { "_hm_busy_preempt", ri_busy_preempt },
    { "_hm_send_payload_as_turn", ri_send_payload_as_turn },
    { "_hm_unknown_slash_reply", ri_unknown_slash_reply },
    { "_hm_bundle_slash_rewrite", ri_bundle_slash_rewrite },
    { "_consume_pending_native_image_paths", ri_consume_pending_native_image_paths },
    { "_inbound_original_names", ri_inbound_original_names },
    { "_restore_pending_one_turn_model_override", ri_restore_pending_one_turn_model_override },
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

        const ri_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            if (!args || args->type != JSON_OBJECT) args = NULL;
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            ri_ctx_t ctx;
            ctx.state = (state && state->type == JSON_OBJECT) ? json_copy(state) : json_object();
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.cursors = json_object();
            ctx.trace = json_array();
            json_t *ev = args ? json_obj_get(args, "event") : NULL;
            ctx.event = ev ? json_copy(ev) : NULL;
            json_t *ret = tc->fn(&ctx, args ? args : empty_args);
            json_set(out, "ret", ret ? ret : json_null());
            json_set(out, "trace", ctx.trace);
            json_set(ctx.state, "event_out", ctx.event ? json_copy(ctx.event) : json_null());
            json_set(out, "state", ctx.state);
            json_free(ctx.cursors);
            json_free(ctx.event);
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
