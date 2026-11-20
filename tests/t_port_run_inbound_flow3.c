/* t_port_run_inbound_flow3.c — harness for the UNCLAIMED remainder FLOW of
 * gateway/run_inbound.py (src/gateway/run_inbound_flow3.c): pairing /
 * ignored-DM, update-response write, clarify reply, evict-idle /
 * stop-owner-turn, running-session fast path, media/text-prep cluster.
 *
 * Reads JSON LINES from stdin: {"func": <name>, "in": {...}}.
 * Emits JSON LINES: {"func", "in", "out"} where out captures the decision
 * SEQUENCE (ordered seam events {"name","args","ret"} in "trace"), the final
 * return ("ret"), the mutated world state ("state") and the possibly
 * mutated INPUT event ("event"). The echoed "in" is the ORIGINAL input
 * (the oracle re-drives the live reference from it).
 *
 * Scripted I/O lives in in.io: a map seam-name -> result. A result is either
 * a plain JSON value (returned verbatim on every call), a LIST consumed in
 * call order, or {"__script__": true, "ret": <value>, "raise": {type,...}}.
 * A missing/exhausted script is a fail-closed RuntimeError on both sides.
 *
 * The port .c is #included directly (self-contained, libjson only).
 */
#include "gateway/run_inbound_flow3.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef json_t *(*ri3_fn_t)(ri3_ctx_t *, json_t *);
typedef struct { const char *func; ri3_fn_t fn; } ri3_case_t;

static const ri3_case_t CASES[] = {
    { "_hm_offer_pairing_code", ri3_hm_offer_pairing_code },
    { "_hm_report_ignored_dm", ri3_hm_report_ignored_dm },
    { "_hm_write_update_response", ri3_hm_write_update_response },
    { "_hm_clarify_reply", ri3_hm_clarify_reply },
    { "_hm_evict_idle_stale_agent", ri3_hm_evict_idle_stale_agent },
    { "_hm_stop_owner_turn", ri3_hm_stop_owner_turn },
    { "_hm_handle_running_session_message", ri3_hm_handle_running_session_message },
    { "_prefix_inbound_sender_context", ri3_prefix_inbound_sender_context },
    { "_classify_inbound_media", ri3_classify_inbound_media },
    { "_enrich_inbound_images", ri3_enrich_inbound_images },
    { "_echo_stt_transcripts", ri3_echo_stt_transcripts },
    { "_inbound_attachment_display_name", ri3_inbound_attachment_display_name },
    { "_prepend_inbound_media_file_notes", ri3_prepend_inbound_media_file_notes },
    { "_prepend_inbound_document_notes", ri3_prepend_inbound_document_notes },
    { "_prepend_inbound_reply_context", ri3_prepend_inbound_reply_context },
    { "_inbound_model_context_length", ri3_inbound_model_context_length },
    { "_expand_inbound_context_references", ri3_expand_inbound_context_references },
    { "_prepare_clarify_reply_text", ri3_prepare_clarify_reply_text },
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

        const ri3_case_t *tc = NULL;
        for (size_t i = 0; i < CASE_COUNT; i++) {
            if (CASES[i].func && strcmp(CASES[i].func, func) == 0) { tc = &CASES[i]; break; }
        }
        if (!tc) {
            json_set_hdr(out, "error", "unknown func");
        } else {
            /* working copy: the port may mutate args.event in place; the
             * echoed "in" stays the pristine fixture. */
            json_t *args = in ? json_obj_get(in, "args") : NULL;
            json_t *work = (args && args->type == JSON_OBJECT) ? json_copy(args) : json_object();
            json_t *state = in ? json_obj_get(in, "state") : NULL;
            ri3_ctx_t ctx;
            ctx.state = (state && state->type == JSON_OBJECT) ? json_copy(state) : json_object();
            ctx.io = in ? json_obj_get(in, "io") : NULL;
            ctx.cursors = json_object();
            ctx.trace = json_array();
            json_t *ret = tc->fn(&ctx, work);
            json_set(out, "ret", ret ? ret : json_null());
            json_set(out, "trace", ctx.trace);
            json_set(out, "state", ctx.state);
            json_t *ev_out = json_obj_get(work, "event");
            json_set(out, "event", ev_out ? json_copy(ev_out) : json_null());
            json_free(ctx.cursors);
            json_free(work);
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
    return 0;
}