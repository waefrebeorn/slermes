/*
 * interrupt_control.c — C home for upstream Hermes agent/interrupt_control.py.
 *
 * Faithful C11 ports of the interrupt attribution reason helpers and the
 * steer() admission/cleaning logic. The lock/state side effects are modelled
 * by returning the observable outcome (accepted + cleaned text).
 *
 * PoP: w9_interruptco_7c8a77_interrupt_issuer @ agent/interrupt_control.py:interrupt_issuer
 * PoP: w9_interruptco_7c8a77_interrupt_skip_wording @ agent/interrupt_control.py:interrupt_skip_wording
 * PoP: w9_interruptco_7c8a77_interrupted_during_api_call_reason @ agent/interrupt_control.py:interrupted_during_api_call_reason
 * PoP: w9_interruptco_7c8a77_steer @ agent/interrupt_control.py:InterruptControlMixin.steer
 */
#include "noc_w9_interruptco_7c8a77.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REASON_HARD_STOP "explicit stop requested"
#define REASON_NEW_MESSAGE "user sent a new message"
#define REASON_USER_INTERRUPT "user interrupt"

static int is_ws(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static char *strip_dup(const char *s)
{
    if (!s) return strdup("");
    size_t a = 0, b = strlen(s);
    while (a < b && is_ws(s[a])) a++;
    while (b > a && is_ws(s[b - 1])) b--;
    char *o = (char *)malloc(b - a + 1);
    if (!o) return strdup("");
    memcpy(o, s + a, b - a);
    o[b - a] = '\0';
    return o;
}

static int is_user_reason(const char *s)
{
    if (!s) return 0;
    return strcmp(s, REASON_HARD_STOP) == 0 || strcmp(s, REASON_NEW_MESSAGE) == 0 ||
           strcmp(s, REASON_USER_INTERRUPT) == 0;
}

static char *replace_char(const char *s, char from, char to)
{
    char *o = strdup(s ? s : "");
    for (char *p = o; *p; p++) if (*p == from) *p = to;
    return o;
}

static char *capitalize_dup(const char *s)
{
    char *o = strdup(s ? s : "");
    if (o[0] >= 'a' && o[0] <= 'z') o[0] = (char)(o[0] - 32);
    for (char *p = o + 1; *p; p++) if (*p >= 'A' && *p <= 'Z') *p = (char)(*p + 32);
    return o;
}

/* interrupt_issuer(agent): reason = agent._tool_interrupt_reason */
static char *issuer_for(const char *reason)
{
    if (!reason || !*reason || is_user_reason(reason)) return NULL;
    char *stripped = strip_dup(reason);
    char *out = replace_char(stripped, ' ', '_');
    free(stripped);
    return out;
}

/* PoP: w9_interruptco_7c8a77_interrupt_issuer @ agent/interrupt_control.py:interrupt_issuer */
json_t *w9_interruptco_7c8a77_interrupt_issuer(json_t *a)
{
    const char *reason = json_get_str(a, "reason", "");
    char *issuer = issuer_for(reason);
    json_t *out = issuer ? json_string(issuer) : json_null();
    free(issuer);
    return out;
}

/* PoP: w9_interruptco_7c8a77_interrupt_skip_wording @ agent/interrupt_control.py:interrupt_skip_wording */
json_t *w9_interruptco_7c8a77_interrupt_skip_wording(json_t *a)
{
    const char *reason = json_get_str(a, "reason", "");
    if (!reason || !*reason) return json_string("Turn interrupted");
    if (is_user_reason(reason)) {
        char *cap = capitalize_dup(reason);
        json_t *r = json_string(cap);
        free(cap);
        return r;
    }
    char *spaced = replace_char(reason, '_', ' ');
    char *out = (char *)malloc(strlen(spaced) + 32);
    /* "Turn aborted \xe2\x80\x94 " (em dash U+2014) */
    snprintf(out, strlen(spaced) + 32, "Turn aborted \xe2\x80\x94 %s", spaced);
    json_t *r = json_string(out);
    free(spaced);
    free(out);
    return r;
}

/* PoP: w9_interruptco_7c8a77_interrupted_during_api_call_reason @ agent/interrupt_control.py:interrupted_during_api_call_reason */
json_t *w9_interruptco_7c8a77_interrupted_during_api_call_reason(json_t *a)
{
    char *issuer = issuer_for(json_get_str(a, "reason", ""));
    char *out;
    if (issuer) {
        out = (char *)malloc(strlen(issuer) + 64);
        snprintf(out, strlen(issuer) + 64, "interrupted_during_api_call(%s)", issuer);
    } else {
        out = strdup("interrupted_during_api_call");
    }
    json_t *r = json_string(out);
    free(issuer);
    free(out);
    return r;
}

/* PoP: w9_interruptco_7c8a77_steer @ agent/interrupt_control.py:InterruptControlMixin.steer */
json_t *w9_interruptco_7c8a77_steer(json_t *a)
{
    const char *text = json_get_str(a, "text", "");
    char *cleaned = strip_dup(text);
    json_t *out = json_object();
    if (!*cleaned) {
        json_set(out, "accepted", json_bool(0));
        json_set(out, "cleaned", json_null());
    } else {
        json_set(out, "accepted", json_bool(1));
        json_set(out, "cleaned", json_string(cleaned));
    }
    free(cleaned);
    return out;
}
