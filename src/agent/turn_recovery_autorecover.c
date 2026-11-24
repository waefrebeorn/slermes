/*
 * turn_recovery_autorecover.c -- real C11 port of the bounded post-exhaustion
 * auto-recovery ladder for a pre-delivery turn, upstream
 * agent/turn_recovery_autorecover.py.
 *
 * PoP: w21_turnrecove_8c1d4a_cycles @ agent/turn_recovery_autorecover.py:auto_recovery_cycles
 * PoP: w21_turnrecove_8c1d4a_wait_seconds @ agent/turn_recovery_autorecover.py:ladder_wait_seconds
 * PoP: w21_turnrecove_8c1d4a_eligible @ agent/turn_recovery_autorecover.py:ladder_eligible
 * PoP: w21_turnrecove_8c1d4a_notice @ agent/turn_recovery_autorecover.py:ladder_notice
 * PoP: w21_turnrecove_8c1d4a_recover @ agent/turn_recovery_autorecover.py:auto_recover_after_exhaustion
 *
 * The schedule constants, the transient-reason set, the stop hints, the cycle
 * counter and the returned action dict are all ported. The provider's
 * Retry-After parse, the jittered backoff's random draw, the agent's own
 * think-block content test and the interruptible sleep are seams (see the
 * header). The notice text is emitted as UTF-8 byte escapes so this source
 * stays ASCII.
 *
 * ASCII source: no non-ASCII bytes below.
 */
#include "hermes_json.h"
#include "noc_w21_turnrecove_8c1d4a.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TR_LADDER_BASE_DELAY_S 15.0
#define TR_LADDER_CAP_S        60.0
#define TR_RETRY_AFTER_CAP_S   120.0
#define TR_DEFAULT_CYCLES      5   /* documented `agent.auto_recovery_cycles` default */

/* "\xE2\x8F\xB3" hourglass, "\xE2\x80\x94" em dash. */
#define TR_HOURGLASS "\xE2\x8F\xB3"
#define TR_EMDASH    "\xE2\x80\x94"

static char *tr_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

/* int(value or 0): truncation toward zero, strings through the int grammar. */
static long long tr_int_or_zero(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) {
        if (!isfinite(v->num_val)) return 0;
        return (long long)v->num_val;
    }
    if (v->type == JSON_STRING) {
        const char *p = v->str_val ? v->str_val : "";
        char *endp = NULL;
        long long parsed;
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
        if (!*p) return 0;
        parsed = strtoll(p, &endp, 10);
        while (endp && (*endp == ' ' || *endp == '\t' || *endp == '\n' || *endp == '\r')) endp++;
        if (!endp || *endp) return 0;
        return parsed;
    }
    return 0;
}

static long long tr_cycles_value(const json_t *agent) {
    json_t *raw = (agent && agent->type == JSON_OBJECT)
                      ? json_obj_get(agent, "_auto_recovery_cycles") : NULL;
    long long cycles = tr_int_or_zero(raw);
    return cycles < 0 ? 0 : cycles;
}

json_t *w21_turnrecove_8c1d4a_cycles(const json_t *agent) {
    return json_number((double)tr_cycles_value(agent));
}

/* jittered_backoff(cycle, base=15, max=60, jitter_ratio=0.2) with the jitter
 * draw at zero: min(base * 2^(cycle-1), max), attempt is 1-based. */
static double tr_schedule_delay(int cycle) {
    int exponent = cycle - 1;
    if (exponent < 0) exponent = 0;
    if (exponent >= 3) return TR_LADDER_CAP_S;   /* 15*8 already exceeds the cap */
    {
        double delay = TR_LADDER_BASE_DELAY_S;
        int i;
        for (i = 0; i < exponent; i++) delay *= 2.0;
        return delay < TR_LADDER_CAP_S ? delay : TR_LADDER_CAP_S;
    }
}

json_t *w21_turnrecove_8c1d4a_wait_seconds(int cycle, const json_t *api_error) {
    json_t *retry_after = (api_error && api_error->type == JSON_OBJECT)
                              ? json_obj_get(api_error, "retry_after") : NULL;
    json_t *out = json_object();
    if (retry_after && retry_after->type == JSON_NUMBER) {
        double wait = retry_after->num_val;
        if (wait > TR_RETRY_AFTER_CAP_S) wait = TR_RETRY_AFTER_CAP_S;
        json_set(out, "wait", json_number(wait));
        json_set(out, "source", json_string("retry_after"));
        return out;
    }
    json_set(out, "wait", json_number(tr_schedule_delay(cycle)));
    json_set(out, "source", json_string("schedule"));
    return out;
}

json_t *w21_turnrecove_8c1d4a_eligible(const json_t *agent, const char *reason,
                                       const char *streamed, int has_content) {
    int cycles_ok = tr_cycles_value(agent) > 0;
    int transient = 0;
    (void)streamed;   /* only the injected `has_content` verdict is consulted */
    if (reason && (!strcmp(reason, "timeout") || !strcmp(reason, "overloaded") ||
                   !strcmp(reason, "server_error")))
        transient = 1;
    return json_bool(cycles_ok && transient && !has_content);
}

json_t *w21_turnrecove_8c1d4a_notice(const char *platform, double wait_s,
                                     int cycle, int total) {
    static const struct { const char *surface; const char *hint; } HINTS[] = {
        {"cli", "press Esc to stop"},
        {"tui", "press Esc to stop"},
        {"desktop", "press Esc to stop"},
        {"api_server", "cancel the request to stop"},
        {"cron", ""},
    };
    const char *hint = "send /stop to cancel";
    char *surface = tr_dup(platform ? platform : "");
    char text[512];
    char *cut;
    size_t i;

    for (cut = surface; *cut; cut++) *cut = (char)tolower((unsigned char)*cut);
    for (i = 0; i < sizeof(HINTS) / sizeof(HINTS[0]); i++) {
        if (!strcmp(surface, HINTS[i].surface)) { hint = HINTS[i].hint; break; }
    }
    free(surface);
    snprintf(text, sizeof(text),
             TR_HOURGLASS " Provider temporarily unavailable " TR_EMDASH
             " retrying automatically in %.0fs (cycle %d/%d)",
             wait_s, cycle, total);
    if (hint[0]) {
        char joined[640];
        snprintf(joined, sizeof(joined), "%s; %s", text, hint);
        return json_string(joined);
    }
    return json_string(text);
}

/* The give-up diagnostic the ladder emits when its cycles are spent. */
static char *tr_gave_up_text(long long total) {
    char *buf = (char *)malloc(256);
    if (!buf) return NULL;
    snprintf(buf, 256,
             TR_HOURGLASS " Automatic recovery gave up after %lld cycles " TR_EMDASH
             " the provider is still unavailable.", total);
    return buf;
}

json_t *w21_turnrecove_8c1d4a_recover(const json_t *case_) {
    const json_t *agent = (case_ && case_->type == JSON_OBJECT)
                              ? json_obj_get(case_, "agent") : NULL;
    const json_t *retry = (case_ && case_->type == JSON_OBJECT)
                              ? json_obj_get(case_, "retry") : NULL;
    const json_t *api_error = (case_ && case_->type == JSON_OBJECT)
                                  ? json_obj_get(case_, "api_error") : NULL;
    const json_t *interrupt = (case_ && case_->type == JSON_OBJECT)
                                  ? json_obj_get(case_, "interrupt") : NULL;
    const char *reason = (case_ && case_->type == JSON_OBJECT)
                             ? json_get_str(case_, "reason", "") : "";
    const char *streamed = (case_ && case_->type == JSON_OBJECT)
                               ? json_get_str(case_, "streamed", "") : "";
    const char *platform = (agent && agent->type == JSON_OBJECT)
                               ? json_get_str(agent, "platform", "") : "";
    int has_content = (case_ && case_->type == JSON_OBJECT)
                          ? (json_get_bool(case_, "has_content", 0) ? 1 : 0) : 0;
    long long total = tr_cycles_value(agent);
    long long used = (long long)tr_int_or_zero((retry && retry->type == JSON_OBJECT)
                                                   ? json_obj_get(retry, "auto_recovery_cycles_used")
                                                   : NULL);
    json_t *status = json_array();
    json_t *wait_lines = json_array();
    json_t *out = json_object();
    json_t *action = json_null();
    json_t *result = json_null();
    json_t *wait_node = json_null();
    json_t *notice_node = json_null();

    if (used < 0) used = 0;
    {
        json_t *eligible = w21_turnrecove_8c1d4a_eligible(agent, reason, streamed, has_content);
        int ok = eligible->type == JSON_BOOL && eligible->bool_val;
        json_free(eligible);
        if (!ok) goto done;
    }
    if (used >= total) {
        char *text = tr_gave_up_text(total);
        json_append(status, json_string(text ? text : ""));
        free(text);
        goto done;
    }
    {
        int cycle = (int)used + 1;
        json_t *wait = w21_turnrecove_8c1d4a_wait_seconds(cycle, api_error);
        double wait_s = json_get_num(wait, "wait", 0.0);
        json_t *notice;
        used = cycle;
        notice = w21_turnrecove_8c1d4a_notice(platform, wait_s, cycle, (int)total);
        json_free(wait_node);
        wait_node = json_number(wait_s);
        json_free(notice_node);
        notice_node = json_copy(notice);
        json_append(status, notice);          /* _emit_diagnostic_status(notice) */
        json_append(wait_lines, json_copy(notice));  /* _emit_diagnostic_wait(notice) */
        if (interrupt && interrupt->type != JSON_NULL) {
            json_free(result);
            result = json_copy(interrupt);
            action = json_string("return");
        } else if (retry && retry->type == JSON_OBJECT &&
                   json_get_bool(retry, "restart_with_redirected_messages", 0)) {
            action = json_string("break");
        } else {
            action = json_string("continue");
        }
    }

done:
    json_set(out, "action", action);
    json_set(out, "result", result);
    json_set(out, "used_cycles", json_number((double)used));
    json_set(out, "wait", wait_node);
    json_set(out, "notice", notice_node);
    json_set(out, "status", status);
    json_set(out, "wait_lines", wait_lines);
    return out;
}
