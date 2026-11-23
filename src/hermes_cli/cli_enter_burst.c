/*
 * cli_enter_burst.c — faithful C11 port of the fast-input-burst state machine
 * in hermes_cli/cli_enter_burst.py (gate w13_clienterb_07c762).
 * PoP: w13_clienterb_07c762_note_text_change @ hermes_cli/cli_enter_burst.py:note_text_change
 * PoP: w13_clienterb_07c762_handle_burst_enter @ hermes_cli/cli_enter_burst.py:handle_burst_enter
 */
#include "noc_w13_clienterb_07c762.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RAPID_INPUT_ENTER_WINDOW_S 0.05
#define BURST_END_QUIET_S 0.25

static json_t *note_result(double ret, double burst_at, int held) {
    json_t *o = json_object();
    json_set(o, "ret", json_number(ret));
    json_set(o, "burst_at", json_number(burst_at));
    json_set(o, "held", json_bool(held != 0));
    return o;
}

json_t *w13_clienterb_07c762_note_text_change(double now,
                                              double last_text_change,
                                              double paste_burst_at,
                                              int held_present) {
    (void)last_text_change;  /* not read by the live function */
    double burst_at = paste_burst_at;
    int held = held_present ? 1 : 0;
    if (held || (now - burst_at) < BURST_END_QUIET_S) {
        burst_at = now;
        held = 0;
    }
    return note_result(now, burst_at, held);
}

static char *append_newline(const char *doc) {
    size_t n = strlen(doc ? doc : "");
    char *r = (char *)malloc(n + 2);
    memcpy(r, doc ? doc : "", n);
    r[n] = '\n';
    r[n + 1] = '\0';
    return r;
}

json_t *w13_clienterb_07c762_handle_burst_enter(
    const char *doc_in,
    double now, double last_text_change, double paste_burst_at, int held_present,
    int has_key_processor, int has_loop, int input_queue_nonempty, int app_running,
    int settle, int clear_held, int edit_doc, int change_last) {
    (void)clear_held;  /* fake pre-clears the marker; settled state is the same */
    const char *before = doc_in ? doc_in : "";
    double burst_at = paste_burst_at;
    int held = held_present ? 1 : 0;
    int ret, submit = 0, scheduled = 0;
    char *doc = strdup(before);

    /* now - getattr(cli, "_tui_last_text_change", 0.0) >= RAPID window -> human Enter. */
    if ((now - last_text_change) >= RAPID_INPUT_ENTER_WINDOW_S) {
        ret = 0;
    } else {
        int pasted_newline_before = (now - burst_at) < BURST_END_QUIET_S;
        burst_at = now;
        char *with_lf = append_newline(doc);
        free(doc); doc = with_lf;

        if (!has_key_processor || !has_loop || input_queue_nonempty || pasted_newline_before) {
            ret = 1;
        } else {
            /* last Enter of a lone burst: hold a marker, defer the decision. */
            ret = 1;
            held = 1;
            scheduled = 1;
        }

        if (scheduled && settle) {
            /* The fake loop calls _settle after (optionally) mutating state.
             * _settle mutates a SEPARATE post-return snapshot of the state. */
            char *post_doc = strdup(doc);
            int post_held = 0;          /* _settle clears the held marker */
            int post_submit = submit;
            char *extra = NULL;
            char *doc_at_settle = doc;
            if (edit_doc) {
                extra = (char *)malloc(strlen(doc) + 2);
                sprintf(extra, "%sX", doc);
                doc_at_settle = extra;
            }
            double last_at_settle = change_last ? (last_text_change + 1.0) : last_text_change;
            int ok = app_running && !strcmp(doc_at_settle, doc) &&
                     last_at_settle == last_text_change;
            if (ok) {
                free(post_doc); post_doc = strdup(before);
                post_submit = 1;
            } else if (edit_doc) {
                free(post_doc); post_doc = strdup(doc_at_settle);
            }
            free(extra);

            json_t *o = json_object();
            json_set(o, "ret", json_bool(ret != 0));
            json_set(o, "doc", json_string(doc));
            json_set(o, "held", json_bool(held != 0));
            json_set(o, "burst_at", json_number(burst_at));
            json_set(o, "submit", json_number((double)submit));
            json_set(o, "scheduled", json_bool(scheduled != 0));
            json_set(o, "post_doc", json_string(post_doc));
            json_set(o, "post_held", json_bool(post_held != 0));
            json_set(o, "post_submit", json_number((double)post_submit));
            free(post_doc); free(doc);
            return o;
        }
    }

    json_t *o = json_object();
    json_set(o, "ret", json_bool(ret != 0));
    json_set(o, "doc", json_string(doc));
    json_set(o, "held", json_bool(held != 0));
    json_set(o, "burst_at", json_number(burst_at));
    json_set(o, "submit", json_number((double)submit));
    json_set(o, "scheduled", json_bool(scheduled != 0));
    json_set(o, "post_doc", json_string(doc));
    json_set(o, "post_held", json_bool(held != 0));
    json_set(o, "post_submit", json_number((double)submit));
    free(doc);
    return o;
}

