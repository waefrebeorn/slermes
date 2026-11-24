/*
 * run_turn_runner_approval_settle.c -- real C11 port of the approval-timeout
 * notice path of upstream Hermes gateway/run_turn_runner_approval_settle.py.
 *
 * PoP: w21_runturnrun_bdbcf9_window @ gateway/platforms/base_exec_approval.py:format_approval_window
 * PoP: w21_runturnrun_bdbcf9_notice @ gateway/platforms/base_exec_approval.py:format_approval_timed_out_notice
 * PoP: w21_runturnrun_bdbcf9_arm @ gateway/run_turn_runner_approval_settle.py:register_timeout_notice
 * PoP: w21_runturnrun_bdbcf9_settle @ gateway/run_turn_runner_approval_settle.py:register_timeout_notice
 * PoP: w21_runturnrun_bdbcf9_card_body @ gateway/run_turn_runner_approval_settle.py:_timed_out_card_body
 * PoP: w21_runturnrun_bdbcf9_post_notice @ gateway/run_turn_runner_approval_settle.py:_post_timeout_notice
 *
 * The i18n lookup and the adapter/turn-runner I/O stay upstream: `t()` is the
 * English catalogue rendered here (the keys are named at each constant), and the
 * adapter answers are the request's `has_edit_message` / `edit_success` /
 * `max_len` / `measure` fields.
 */
#include "hermes_json.h"
#include "noc_w21_runturnrun_bdbcf9.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- locale copy: agent/locales/en/... ----------------------------------- */
/* gateway.exec_approval.timed_out_notice */
static const char RR_NOTICE_FMT[] =
    "\xe2\x8c\x9b" " Approval timed out after %s \xe2\x80\x94 the command was NOT run."
    " Ask me to try again if you still want it, or raise approvals.timeout in config.yaml.";
/* gateway.exec_approval.timed_out_card_body */
static const char RR_CARD_FMT[] = "%s\n```\n%s\n```";
/* gateway.exec_approval.reason_label */
static const char RR_REASON_LABEL[] = "Why it was flagged";
/* The label the hook hands to runner._schedule(). */
static const char RR_SCHEDULE_LABEL[] = "Approval timeout notice scheduling error";
static const char RR_ELLIPSIS[] = "...";

/* --- tiny helpers -------------------------------------------------------- */

static char *rr_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

typedef struct {
    char *p;
    size_t n;
    size_t cap;
} rr_buf;

static int rr_reserve(rr_buf *b, size_t need) {
    char *np;
    size_t cap;
    if (b->n + need + 1 <= b->cap) return 1;
    cap = b->cap ? b->cap : 64;
    while (cap < b->n + need + 1) cap *= 2;
    np = (char *)realloc(b->p, cap);
    if (!np) return 0;
    b->p = np;
    b->cap = cap;
    return 1;
}

static void rr_addn(rr_buf *b, const char *s, size_t n) {
    if (!s || !n) return;
    if (!rr_reserve(b, n)) return;
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
}

static void rr_adds(rr_buf *b, const char *s) {
    if (s) rr_addn(b, s, strlen(s));
}

static char *rr_take(rr_buf *b) {
    char *p;
    if (!b->p) return rr_dup("");
    p = b->p;
    b->p = NULL;
    b->n = b->cap = 0;
    return p;
}

/* Python truthiness; an absent node reads as None. */
static int rr_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

/* --- UTF-8 aware measurement (Python len / utf16_len) -------------------- */

static size_t rr_next(const char *s, size_t i) {
    unsigned char c = (unsigned char)s[i];
    if (c < 0x80) return i + 1;
    if ((c >> 5) == 0x6) return i + 2;
    if ((c >> 4) == 0xE) return i + 3;
    if ((c >> 3) == 0x1E) return i + 4;
    return i + 1;
}

/* Codepoint count, or UTF-16 unit count, of the first `nbytes` bytes. */
static long rr_measure_of(const char *s, size_t nbytes, int utf16) {
    size_t i = 0;
    long units = 0;
    while (i < nbytes && s[i]) {
        unsigned char c = (unsigned char)s[i];
        if (utf16 && c >= 0xF0) units += 2;
        else units += 1;
        i = rr_next(s, i);
    }
    return units;
}

/* Byte offset of codepoint index `cp` (clamped to the string's length). */
static size_t rr_offset_at(const char *s, long cp) {
    size_t i = 0;
    long seen = 0;
    while (s[i] && seen < cp) {
        i = rr_next(s, i);
        seen += 1;
    }
    return i;
}

/* gateway/platforms/base.py:fit_prefix -- the longest prefix within `budget`
 * measured units, plus the suffix outside the budget. */
static char *rr_fit_prefix(const char *text, long budget, int utf16) {
    long total, lo, hi;
    size_t off;
    rr_buf b;
    if (!text) text = "";
    total = rr_measure_of(text, strlen(text), utf16);
    if (total <= budget) return rr_dup(text);
    lo = 0;
    hi = rr_measure_of(text, strlen(text), 0);  /* codepoint length */
    while (lo < hi) {
        long mid = (lo + hi + 1) / 2;
        size_t mo = rr_offset_at(text, mid);
        if (rr_measure_of(text, mo, utf16) <= budget) lo = mid;
        else hi = mid - 1;
    }
    off = rr_offset_at(text, lo);
    b.p = NULL; b.n = 0; b.cap = 0;
    rr_addn(&b, text, off);
    rr_adds(&b, RR_ELLIPSIS);
    return rr_take(&b);
}

/* `re.sub(r"`{3,}", lambda run: "\u200b".join(run.group()), s)` */
static char *rr_split_backtick_runs(const char *s) {
    size_t i = 0;
    rr_buf b;
    static const char ZWSP[] = "\xe2\x80\x8b";
    b.p = NULL; b.n = 0; b.cap = 0;
    if (!s) s = "";
    while (s[i]) {
        if (s[i] == '`') {
            size_t start = i;
            size_t k, j;
            while (s[i] == '`') i++;
            k = i - start;
            if (k >= 3) {
                for (j = 0; j < k; j++) {
                    if (j) rr_addn(&b, ZWSP, sizeof(ZWSP) - 1);
                    rr_addn(&b, "`", 1);
                }
            } else {
                rr_addn(&b, s + start, k);
            }
            continue;
        }
        rr_addn(&b, s + i, 1);
        i++;
    }
    return rr_take(&b);
}

/* --- window wording ------------------------------------------------------ */

char *w21_runturnrun_bdbcf9_window(double seconds) {
    double raw = seconds == 0.0 ? 0.0 : seconds;
    long s = (long)raw;                 /* int(seconds or 0) */
    const char *unit;
    long count;
    char buf[64];
    if (s < 0) s = 0;                   /* max(..., 0) */
    if (s && s % 3600 == 0) { count = s / 3600; unit = "hour"; }
    else if (s && s % 60 == 0) { count = s / 60; unit = "minute"; }
    else { count = s; unit = "second"; }
    snprintf(buf, sizeof(buf), "%ld %s%s", count, unit, count == 1 ? "" : "s");
    return rr_dup(buf);
}

char *w21_runturnrun_bdbcf9_notice(double timeout_s) {
    char *window = w21_runturnrun_bdbcf9_window(timeout_s);
    char *out;
    size_t need;
    if (!window) return NULL;
    need = strlen(RR_NOTICE_FMT) + strlen(window) + 8;
    out = (char *)malloc(need);
    if (!out) { free(window); return NULL; }
    snprintf(out, need, RR_NOTICE_FMT, window);
    free(window);
    return out;
}

/* --- arming -------------------------------------------------------------- */

json_t *w21_runturnrun_bdbcf9_arm(const json_t *approval_data, const char *session_key,
                                  const char *fallback_reason, double fallback_timeout) {
    json_t *request_id = json_obj_get(approval_data, "request_id");
    json_t *timeout = json_obj_get(approval_data, "timeout_s");
    json_t *description = json_obj_get(approval_data, "description");
    json_t *out = json_object();

    if (!rr_truthy(request_id) || !session_key || !session_key[0]) {
        json_set(out, "armed", json_bool(0));
        return out;
    }
    json_set(out, "armed", json_bool(1));
    json_set(out, "request_id", request_id ? json_copy(request_id) : json_null());
    json_set(out, "session_key", json_string(session_key));
    if (timeout && timeout->type == JSON_NUMBER) {
        json_set(out, "timeout_s", json_number(timeout->num_val));
        json_set(out, "timeout_from", json_string("params"));
    } else {
        json_set(out, "timeout_s", json_number(fallback_timeout));
        json_set(out, "timeout_from", json_string("fallback"));
    }
    /* `approval_data.get("description") or ea_default_reason_text()` */
    if (description && rr_truthy(description))
        json_set(out, "flagged", json_copy(description));
    else
        json_set(out, "flagged", json_string(fallback_reason ? fallback_reason : ""));
    return out;
}

json_t *w21_runturnrun_bdbcf9_settle(const char *reason, const json_t *still_current,
                                     const json_t *arm, const char *command,
                                     const char *card_message_id) {
    json_t *out = json_object();
    json_t *args;

    if (!json_get_bool(arm, "armed", 0)) {
        json_set(out, "scheduled", json_bool(0));
        json_set(out, "why", json_string("unarmed"));
        return out;
    }
    if (!reason || strcmp(reason, "timeout")) {
        json_set(out, "scheduled", json_bool(0));
        json_set(out, "why", json_string("reason"));
        return out;
    }
    /* `callable(still_current) and not still_current()` */
    if (still_current && still_current->type == JSON_BOOL && !still_current->bool_val) {
        json_set(out, "scheduled", json_bool(0));
        json_set(out, "why", json_string("not_current"));
        return out;
    }
    json_set(out, "scheduled", json_bool(1));
    json_set(out, "label", json_string(RR_SCHEDULE_LABEL));
    args = json_object();
    json_set(args, "command", json_string(command ? command : ""));
    json_set(args, "card_message_id",
             card_message_id && card_message_id[0] ? json_string(card_message_id) : json_null());
    json_set(args, "timeout_s", json_copy(json_obj_get(arm, "timeout_s")));
    /* the hook passes `flagged` in the `reason` parameter of _post_timeout_notice */
    json_set(args, "reason", json_copy(json_obj_get(arm, "flagged")));
    json_set(out, "args", args);
    return out;
}

/* --- card body ----------------------------------------------------------- */

static char *rr_card_render(const char *notice, const char *command) {
    size_t need = strlen(notice ? notice : "") + strlen(command ? command : "") + 16;
    char *out = (char *)malloc(need);
    if (!out) return NULL;
    snprintf(out, need, RR_CARD_FMT, notice ? notice : "", command ? command : "");
    return out;
}

char *w21_runturnrun_bdbcf9_card_body(const char *notice, const char *command, const char *reason,
                                      const char *measure_mode, double max_len) {
    int utf16 = measure_mode && !strcmp(measure_mode, "utf16");
    char *safe_command = rr_split_backtick_runs(command);
    char *safe_reason = rr_split_backtick_runs(reason);
    char *body_notice;
    char *frame_text;
    char *fitting;
    char *out;
    long frame, budget;

    if (safe_reason && safe_reason[0]) {
        char *fitted_reason = rr_fit_prefix(safe_reason, 500, utf16);
        rr_buf b;
        b.p = NULL; b.n = 0; b.cap = 0;
        rr_adds(&b, notice ? notice : "");
        rr_adds(&b, "\n");
        rr_adds(&b, RR_REASON_LABEL);
        rr_adds(&b, ": ");
        rr_adds(&b, fitted_reason);
        body_notice = rr_take(&b);
        free(fitted_reason);
    } else {
        body_notice = rr_dup(notice ? notice : "");
    }
    frame_text = rr_card_render(body_notice, "");
    frame = rr_measure_of(frame_text, strlen(frame_text), utf16);
    budget = (long)max_len - frame - (long)strlen(RR_ELLIPSIS);
    if (budget < 0) budget = 0;
    fitting = rr_fit_prefix(safe_command, budget, utf16);
    out = rr_card_render(body_notice, fitting);
    free(fitting);
    free(frame_text);
    free(body_notice);
    free(safe_command);
    free(safe_reason);
    return out;
}

/* --- post notice --------------------------------------------------------- */

/* `getattr(source, "is_bot", False) is not True` */
static int rr_notices_allowed(const json_t *source) {
    json_t *is_bot = json_obj_get(source, "is_bot");
    if (is_bot && is_bot->type == JSON_BOOL && is_bot->bool_val) return 0;
    return 1;
}

/* `max(int(seconds or 0), 0)` for a numeric or numeric-string timeout. */
static int rr_timeout_seconds(const json_t *v, double *out_seconds, char **error) {
    if (!v || v->type == JSON_NULL) { *out_seconds = 0.0; return 1; }
    if (v->type == JSON_NUMBER) { *out_seconds = v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out_seconds = v->bool_val ? 1.0 : 0.0; return 1; }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val ? v->str_val : "";
        char *end = NULL;
        long long parsed;
        if (!s[0]) { *out_seconds = 0.0; return 1; }
        parsed = strtoll(s, &end, 10);
        while (end && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end++;
        if (!end || *end || end == s) {
            size_t need = strlen(s) + 64;
            char *msg = (char *)malloc(need);
            if (msg) snprintf(msg, need, "invalid literal for int() with base 10: '%s'", s);
            *error = msg;
            return 0;
        }
        *out_seconds = (double)parsed;
        return 1;
    }
    {
        const char *kind = v->type == JSON_BOOL ? "bool" : v->type == JSON_ARRAY ? "list" : "dict";
        size_t need = strlen(kind) + 64;
        char *msg = (char *)malloc(need);
        if (msg) snprintf(msg, need, "int() argument must be a string or a number, not '%s'", kind);
        *error = msg;
    }
    return 0;
}

static json_t *rr_raised(const char *type, const char *message) {
    json_t *out = json_object();
    json_set(out, "action", json_string("raised"));
    json_set(out, "type", json_string(type ? type : "Exception"));
    json_set(out, "message", json_string(message ? message : ""));
    return out;
}

json_t *w21_runturnrun_bdbcf9_post_notice(const json_t *req) {
    json_t *source = json_obj_get(req, "source");
    json_t *card_message_id = json_obj_get(req, "card_message_id");
    json_t *thread_metadata = json_obj_get(req, "thread_metadata");
    const char *command = json_get_str(req, "command", "");
    const char *reason = json_get_str(req, "reason", "");
    const char *measure = json_get_str(req, "measure", "len");
    const char *chat_id = json_get_str(req, "status_chat_id", "");
    double max_len = json_get_num(req, "max_len", 4096);
    double timeout_s = 0.0;
    char *error = NULL;
    char *notice;
    json_t *out;

    if (!rr_notices_allowed(source)) {
        out = json_object();
        json_set(out, "action", json_string("skipped"));
        return out;
    }
    if (!rr_timeout_seconds(json_obj_get(req, "timeout_s"), &timeout_s, &error))
        return rr_raised("ValueError", error ? error : "");
    notice = w21_runturnrun_bdbcf9_notice(timeout_s);
    out = json_object();
    if (json_get_bool(req, "has_edit_message", 0) && rr_truthy(card_message_id) &&
        json_get_bool(req, "edit_success", 0)) {
        char *body = w21_runturnrun_bdbcf9_card_body(notice, command, reason, measure, max_len);
        json_set(out, "action", json_string("edited"));
        json_set(out, "chat_id", json_string(chat_id));
        json_set(out, "message_id", json_copy(card_message_id));
        json_set(out, "body", json_string(body ? body : ""));
        free(body);
    } else {
        json_t *metadata = json_object();
        size_t i;
        for (i = 0; i < json_object_size(thread_metadata); i++) {
            const char *k = json_object_get_key_at(thread_metadata, i);
            if (!k) continue;
            json_set(metadata, k, json_copy(json_object_get_at(thread_metadata, i)));
        }
        json_set(metadata, "_interim_send", json_bool(1));  /* _interim_metadata() */
        json_set(out, "action", json_string("sent"));
        json_set(out, "chat_id", json_string(chat_id));
        json_set(out, "text", json_string(notice ? notice : ""));
        json_set(out, "metadata", metadata);
    }
    free(notice);
    return out;
}
