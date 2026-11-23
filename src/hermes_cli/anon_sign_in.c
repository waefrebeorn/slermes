/*
 * anon_sign_in.c — C port of upstream Hermes hermes_cli/anon_sign_in.py.
 *
 * PoP: w9_anonsignin_37dfd1_format_wait_line @ hermes_cli/anon_sign_in.py:format_wait_line
 * PoP: w9_anonsignin_37dfd1_completed_copy @ hermes_cli/anon_sign_in.py:Completed.copy / copy_terminal
 * PoP: w9_anonsignin_37dfd1_code_copy_with_wait @ hermes_cli/anon_sign_in.py:Code.copy_with_wait
 * PoP: w9_anonsignin_37dfd1_state_copy @ hermes_cli/anon_sign_in.py:<state>.copy
 *
 * Network/credential composition (run_sign_in) is intentionally not ported;
 * only the deterministic copy/wait rendering is modelled here.
 */
#include "noc_w9_anonsignin_37dfd1.h"
#include "hermes_json.h"
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NO_DEFAULT_CHAT      "No default model is set yet; run /model to pick one."
#define NO_DEFAULT_TERMINAL  "No default model is set yet; run `hermes model` to pick one."
#define DO_NOT_SHARE         "Do not share this code."

/* Python int(x): bool -> 0/1, number truncates, decimal string with optional
 * sign / whitespace / '_' separators. Returns 0 when int() would raise. */
static int py_int(const json_t *v, long long *out) {
    if (!v) return 0;
    if (v->type == JSON_NULL || v->type == JSON_ARRAY || v->type == JSON_OBJECT) return 0;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (!isfinite(d)) return 0;
        *out = (long long)d;
        return 1;
    }
    if (v->type == JSON_STRING) {
        const char *p = v->str_val ? v->str_val : "";
        while (*p && isspace((unsigned char)*p)) p++;
        const char *e = p + strlen(p);
        while (e > p && isspace((unsigned char)e[-1])) e--;
        if (e == p) return 0;
        int neg = 0;
        if (*p == '+' || *p == '-') { neg = (*p == '-'); p++; }
        char digs[256];
        size_t dn = 0;
        int prev_digit = 0, any = 0;
        for (; p < e && dn + 1 < sizeof(digs); p++) {
            if (*p == '_') {
                if (!prev_digit || p + 1 >= e || !isdigit((unsigned char)p[1])) return 0;
                prev_digit = 0;
                continue;
            }
            if (!isdigit((unsigned char)*p)) return 0;
            digs[dn++] = *p;
            prev_digit = 1;
            any = 1;
        }
        if (!any || p != e) return 0;
        digs[dn] = '\0';
        errno = 0;
        char *endp = NULL;
        long long val = strtoll(digs, &endp, 10);
        if (errno == ERANGE || (endp && *endp)) return 0;
        *out = neg ? -val : val;
        return 1;
    }
    return 0;
}

/* ceil(v/60) for integer v, with max(1, ...) applied afterwards. */
static long long wait_minutes(long long v) {
    if (v < 0) v = 0;
    long long n = v == 0 ? 0 : (v + 59) / 60;
    return n < 1 ? 1 : n;
}

struct json_t *w9_anonsignin_37dfd1_format_wait_line(const json_t *expires_in) {
    long long raw = 0;
    py_int(expires_in, &raw);
    long long n = wait_minutes(raw);
    char buf[160];
    snprintf(buf, sizeof(buf), "Waiting for sign-in, up to %lld minute%s.", n, n == 1 ? "" : "s");
    return json_string(buf);
}

struct json_t *w9_anonsignin_37dfd1_completed_copy(const json_t *email,
                                                  const json_t *model,
                                                  int model_changed,
                                                  const json_t *handoff,
                                                  int terminal) {
    const char *em = (email && email->type == JSON_STRING && email->str_val && email->str_val[0])
                     ? email->str_val : NULL;
    char out[16384];
    size_t n = 0;
    if (em) n += (size_t)snprintf(out + n, sizeof(out) - n, "Signed in as %s.", em);
    else n += (size_t)snprintf(out + n, sizeof(out) - n, "Signed in.");
    if (handoff && handoff->type == JSON_ARRAY) {
        for (size_t i = 0; i < handoff->c.count; i++) {
            json_t *line = json_get(handoff, i);
            const char *s = (line && line->type == JSON_STRING && line->str_val) ? line->str_val : "";
            n += (size_t)snprintf(out + n, sizeof(out) - n, "\n%s", s);
        }
    }
    if (model_changed) {
        const char *md = (model && model->type == JSON_STRING && model->str_val && model->str_val[0])
                         ? model->str_val : NULL;
        if (md) n += (size_t)snprintf(out + n, sizeof(out) - n, "\nDefault model is now %s.", md);
        else n += (size_t)snprintf(out + n, sizeof(out) - n, "\n%s",
                                   terminal ? NO_DEFAULT_TERMINAL : NO_DEFAULT_CHAT);
    }
    out[n] = '\0';
    return json_string(out);
}

struct json_t *w9_anonsignin_37dfd1_code_copy_with_wait(const json_t *expires_in) {
    long long raw = 0;
    py_int(expires_in, &raw);
    long long n = wait_minutes(raw);
    char buf[200];
    snprintf(buf, sizeof(buf), "%s Waiting for sign-in, up to %lld minute%s.",
             DO_NOT_SHARE, n, n == 1 ? "" : "s");
    return json_string(buf);
}

struct json_t *w9_anonsignin_37dfd1_state_copy(const char *kind, const struct json_t *detail) {
    (void)detail;
    if (!kind) return json_string("");
    if (!strcmp(kind, "code")) return json_string(DO_NOT_SHARE);
    if (!strcmp(kind, "waiting")) return json_string("Waiting for sign-in...");
    if (!strcmp(kind, "cancelled")) return json_string("Sign-in cancelled.");
    if (!strcmp(kind, "timed_out")) return json_string("That sign-in link has expired. Start again whenever you're ready.");
    if (!strcmp(kind, "already_signed_in")) return json_string("Already signed in.");
    return json_string("");
}
