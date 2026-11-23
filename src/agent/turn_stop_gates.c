/*
 * turn_stop_gates.c — C port of upstream Hermes agent/turn_stop_gates.py.
 *
 * PoP: turnstopga_apply @ agent/turn_stop_gates.py:apply_stop_gates
 * PoP: turnstopga_apply @ agent/turn_stop_gates.py:_continue
 * PoP: turnstopga_apply @ agent/turn_stop_gates.py:_append_interim_answer
 * PoP: turnstopga_apply @ agent/turn_stop_gates.py:_late_lsp_verdicts
 * PoP: turnstopga_apply @ agent/turn_stop_gates.py:_verify_on_stop_nudge
 * PoP: turnstopga_apply @ agent/turn_stop_gates.py:_pre_verify_nudge
 * PoP: turnstopga_apply @ agent/turn_stop_gates.py:_kanban_stop_nudge
 *
 * Gate order (first truthy wins): late LSP -> verify-on-stop -> pre_verify ->
 * kanban. Every nudge source is an explicit seam supplied in the input object;
 * agent effects are recorded as counters for the differential oracle.
 */
#include "noc_w7_turnstopga_8193c5.h"
#include "hermes_json.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SB_CAP 32768
typedef struct { char buf[SB_CAP]; size_t n; } SB;
static void sb_init(SB *s) { s->buf[0] = '\0'; s->n = 0; }
static void sb_addf(SB *s, const char *fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int k = vsnprintf(s->buf + s->n, SB_CAP - s->n, fmt, ap);
    va_end(ap);
    if (k > 0) s->n += (size_t)k < SB_CAP - s->n ? (size_t)k : SB_CAP - s->n - 1;
}

typedef struct {
    json_t *final_msg;
    json_t *messages;
    int emit_count;
    int flush_attempts;
    int diagnostic_count;
    const char *emit_finish_reason;
} Effects;

static void append_row(Effects *e, const char *content, const char *flag_key) {
    json_t *row = json_object();
    json_set(row, "role", json_string("user"));
    json_set(row, "content", json_string(content ? content : ""));
    if (flag_key) json_set(row, flag_key, json_bool(true));
    json_append(e->messages, row);
}

/* _append_interim_answer: emit interim, append the answer row, flush unless hidden. */
static void append_interim_answer(Effects *e, int flush_fail) {
    (void)flush_fail;              /* a raised flush is swallowed by the caller */
    e->emit_count++;
    e->emit_finish_reason = json_get_str(e->final_msg, "finish_reason", "");
    json_append(e->messages, json_copy(e->final_msg));
    const char *dk = json_get_str(e->final_msg, "display_kind", "");
    if (dk && !strcmp(dk, "hidden")) return;
    e->flush_attempts++;
}

static const char *nonempty_or_null(const json_t *in, const char *key) {
    json_t *v = json_obj_get(in, key);
    if (!v || v->type != JSON_STRING || !v->str_val || !v->str_val[0]) return NULL;
    return v->str_val;
}

json_t *turnstopga_apply(const struct json_t *in) {
    Effects e;
    e.final_msg = json_copy(json_obj_get(in, "final_msg"));
    if (!json_is_object(e.final_msg)) { json_free(e.final_msg); e.final_msg = json_object(); }
    e.messages = json_copy(json_obj_get(in, "messages"));
    if (!json_is_array(e.messages)) { json_free(e.messages); e.messages = json_array(); }
    e.emit_count = 0;
    e.flush_attempts = 0;
    e.diagnostic_count = 0;
    e.emit_finish_reason = "";

    const char *final_response = json_get_str(in, "final_response", "");
    const char *pending = json_get_str(in, "pending_verification_response", "");
    int pending_previewed = json_get_bool(in, "pending_verification_response_previewed", false) ? 1 : 0;
    int previewed = json_get_bool(in, "interim_streamed", false) ? 1 : 0;
    int flush_fail = json_get_bool(in, "flush_fail", false) ? 1 : 0;

    int lsp_issued = (int)json_get_num(in, "lsp_issued", 0);
    int verification_stop_nudges = 0, pre_verify_nudges = 0, kanban_stop_nudges = 0;
    int cont = 0;
    const char *nudge = NULL;
    const char *flag_key = NULL;

    /* 1. late LSP verdicts */
    json_t *notes = json_obj_get(in, "lsp_notes");
    if (lsp_issued < 3 && json_is_array(notes) && json_len(notes) > 0) {
        SB s; sb_init(&s);
        sb_addf(&s, "%s", "[System note: the language server reported problems in files you edited this turn, after "
                          "those tool results had returned. Fix them if they are real, then give your final answer.]\n\n");
        int first = 1;
        for (size_t i = 0; i < json_len(notes); i++) {
            json_t *note = json_get(notes, i);
            json_t *txtv = json_is_array(note) ? json_get(note, 1) : NULL;
            const char *text = (txtv && txtv->type == JSON_STRING && txtv->str_val) ? txtv->str_val : "";
            if (!first) sb_addf(&s, "\n\n");
            sb_addf(&s, "%s", text ? text : "");
            first = 0;
        }
        nudge = strdup(s.buf);
        lsp_issued++;
        json_set(e.final_msg, "finish_reason", json_string("verification_required"));
        append_interim_answer(&e, flush_fail);
        append_row(&e, nudge, NULL);          /* durable row, no scaffolding flag */
        cont = 1;
        flag_key = NULL;                       /* LSP branch returns verdict inline */
    }

    /* 2. verify-on-stop */
    if (!cont && (nudge = nonempty_or_null(in, "verify_nudge")) != NULL) {
        verification_stop_nudges++;
        json_set(e.final_msg, "finish_reason", json_string("verification_required"));
        append_interim_answer(&e, flush_fail);
        cont = 1;
        flag_key = "_verification_stop_synthetic";
    }

    /* 3. pre_verify hook */
    if (!cont && (nudge = nonempty_or_null(in, "pre_verify_nudge")) != NULL) {
        pre_verify_nudges++;
        json_set(e.final_msg, "finish_reason", json_string("verify_hook_continue"));
        append_interim_answer(&e, flush_fail);
        cont = 1;
        flag_key = "_pre_verify_synthetic";
    }

    /* 4. kanban terminal-tool guard */
    if (!cont && (nudge = nonempty_or_null(in, "kanban_nudge")) != NULL) {
        kanban_stop_nudges++;
        json_set(e.final_msg, "finish_reason", json_string("kanban_terminal_required"));
        json_set(e.final_msg, "_kanban_stop_synthetic", json_bool(true));
        /* append_message(messages, final_msg) — no interim emit, no flush */
        json_append(e.messages, json_copy(e.final_msg));
        cont = 1;
        flag_key = "_kanban_stop_synthetic";
        e.diagnostic_count++;
    }

    if (cont && flag_key) append_row(&e, nudge, flag_key);

    json_t *verdict = json_object();
    json_set(verdict, "continue_turn", json_bool(cont ? true : false));
    json_set(verdict, "final_response", cont ? json_null() : json_string(final_response));
    json_set(verdict, "pending_verification_response", json_string(cont ? final_response : pending));
    json_set(verdict, "pending_verification_response_previewed",
             json_bool((cont ? previewed : pending_previewed) ? true : false));

    json_t *out = json_object();
    json_set(out, "verdict", verdict);
    json_set(out, "final_msg", e.final_msg);
    json_set(out, "messages", e.messages);
    json_set(out, "lsp_issued", json_number((double)lsp_issued));
    json_set(out, "verification_stop_nudges", json_number((double)verification_stop_nudges));
    json_set(out, "pre_verify_nudges", json_number((double)pre_verify_nudges));
    json_set(out, "kanban_stop_nudges", json_number((double)kanban_stop_nudges));
    json_set(out, "emit_count", json_number((double)e.emit_count));
    json_set(out, "emit_finish_reason", json_string(e.emit_finish_reason));
    json_set(out, "flush_attempts", json_number((double)e.flush_attempts));
    json_set(out, "diagnostic_count", json_number((double)e.diagnostic_count));
    return out;
}
