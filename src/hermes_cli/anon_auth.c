/*
 * anon_auth.c — faithful C11 port of upstream Hermes hermes_cli/anon_auth.py.
 *
 * PoP: w7_anonauth_c583c5_friendly_wait @ hermes_cli/anon_auth.py:friendly_wait
 * PoP: w7_anonauth_c583c5_is_guest_state @ hermes_cli/anon_auth.py:is_guest_state
 * PoP: w7_anonauth_c583c5_parse_welcome_refusal @ hermes_cli/anon_auth.py:parse_welcome_refusal
 * PoP: w7_anonauth_c583c5_anon_failure_copy @ hermes_cli/anon_auth.py:anon_failure_copy
 * PoP: w7_anonauth_c583c5_route_is_welcome_host @ hermes_cli/anon_auth.py:route_is_welcome_host
 * PoP: w7_anonauth_c583c5_pin_model_for_route @ hermes_cli/anon_auth.py:pin_model_for_route
 */
#include "hermes_json.h"
#include "noc_w7_anonauth_c583c5.h"
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AA_GUEST_MODEL "nous/welcome"
#define AA_WELCOME_HOST "welcome-api.nousresearch.com"

/* Python round() is banker's rounding (ties to even); seconds are >= 0 here. */
static long long aa_round_even(double x) {
    double f = floor(x);
    double frac = x - f;
    long long n = (long long)f;
    if (frac > 0.5) return n + 1;
    if (frac < 0.5) return n;
    return (n % 2 == 0) ? n : n + 1;
}

static double aa_seconds(const char *raw) {
    if (!raw || !*raw) return 0.0;
    errno = 0;
    char *end = NULL;
    double s = strtod(raw, &end);
    if (errno == ERANGE || end == raw) return 0.0;  /* float() raised -> 0.0 */
    while (*end && isspace((unsigned char)*end)) end++;
    if (*end != '\0') return 0.0;
    if (s < 0.0) return 0.0;                       /* max(0.0, s) */
    return s;
}

struct json_t *w7_anonauth_c583c5_friendly_wait(const char *seconds) {
    double s = aa_seconds(seconds);
    char buf[96];
    if (s <= 15)
        snprintf(buf, sizeof(buf), "a few seconds");
    else if (s < 90)
        snprintf(buf, sizeof(buf), "about a minute");
    else if (s < 3600)
        snprintf(buf, sizeof(buf), "about %lld minutes", aa_round_even(s / 60.0));
    else {
        long long hours = aa_round_even(s / 3600.0);
        if (hours <= 1)
            snprintf(buf, sizeof(buf), "about an hour");
        else
            snprintf(buf, sizeof(buf), "about %lld hours", hours);
    }
    return json_string(buf);
}

int w7_anonauth_c583c5_is_guest_state(const struct json_t *state) {
    if (!state || state->type != JSON_OBJECT) return 0;
    struct json_t *m = json_obj_get((struct json_t *)state, "auth_method");
    return (m && m->type == JSON_STRING && m->str_val &&
            !strcmp(m->str_val, "anonymous")) ? 1 : 0;
}

static int aa_reason_allowed(const char *reason) {
    static const char *const reasons[] = {
        "model_not_free", "feature_not_free", "at_capacity", "admission_closed", "rate_limited"};
    for (size_t i = 0; i < sizeof(reasons) / sizeof(reasons[0]); i++)
        if (!strcmp(reason, reasons[i])) return 1;
    return 0;
}

static int aa_retry_value(const struct json_t *v) {
    if (!v) return 0;
    double d = 0.0;
    if (v->type == JSON_NUMBER) d = v->num_val;
    else if (v->type == JSON_STRING && v->str_val && *v->str_val) d = aa_seconds(v->str_val);
    else return 0;
    if (d < 0) d = 0;
    return (int)d;
}

struct json_t *w7_anonauth_c583c5_parse_welcome_refusal(const struct json_t *body) {
    if (!body || body->type != JSON_OBJECT) return json_null();
    struct json_t *rv = json_obj_get((struct json_t *)body, "reason");
    if (!rv || rv->type != JSON_STRING || !rv->str_val || !aa_reason_allowed(rv->str_val))
        return json_null();
    struct json_t *out = json_object();
    json_set(out, "reason", json_string(rv->str_val));
    json_set(out, "retry_after", json_int(aa_retry_value(json_obj_get((struct json_t *)body, "retry_after"))));
    struct json_t *alts = json_array();
    struct json_t *raw_alts = json_obj_get((struct json_t *)body, "alternates");
    if (raw_alts && raw_alts->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(raw_alts); i++) {
            struct json_t *a = json_get((struct json_t *)raw_alts, i);
            if (a && a->type == JSON_STRING && a->str_val && *a->str_val)
                json_append(alts, json_string(a->str_val));
        }
    }
    json_set(out, "alternates", alts);
    struct json_t *upgrade = json_obj_get((struct json_t *)body, "upgrade_url");
    json_set(out, "upgrade_url", json_string(
        (upgrade && upgrade->type == JSON_STRING && upgrade->str_val) ? upgrade->str_val : ""));
    return out;
}

struct json_t *w7_anonauth_c583c5_anon_failure_copy(const char *code, const char *retry_after) {
    static const char *const SIGNIN_FREE = "Signing in is free.";
    const char *tmpl = NULL;
    if (!code) code = "";
    if (!strcmp(code, "anon_gate_closed"))
        tmpl = "This version can't be used without a Nous account. Signing in is free.";
    else if (!strcmp(code, "anon_gate_paused"))
        tmpl = "Using Hermes without signing in is paused for a moment. Signing in is free.";
    else if (!strcmp(code, "anon_rate_limited"))
        tmpl = "Lots of people are getting started right now. Try again in {wait}. Signing in is free and skips the wait.";
    else if (!strcmp(code, "anon_pow_required"))
        tmpl = "The Nous server asked for a proof of work, but that isn't implemented in your Agent yet. Sign in with a Nous account to continue.";
    else if (!strcmp(code, "anon_challenge_required"))
        tmpl = "Finish the quick check in your browser, then try again.";
    else if (!strcmp(code, "anon_signin_required"))
        tmpl = "Free guest access isn't available here. Sign in with a Nous account to continue.";
    else if (!strcmp(code, "anon_account_locked"))
        tmpl = "This session can't continue without signing in. Signing in is free.";
    else if (!strcmp(code, "anon_credential_dead"))
        tmpl = "Your session ended. A new one starts on its own.";
    else if (!strcmp(code, "anon_unreachable"))
        tmpl = "The Nous service couldn't be reached. Check your internet connection and try again.";
    else
        tmpl = "The Nous service had a hiccup. Try again in a moment.";
    (void)SIGNIN_FREE;

    const char *wait = NULL;
    if (strstr(tmpl, "{wait}") != NULL) {
        struct json_t *w = w7_anonauth_c583c5_friendly_wait(
            (retry_after && *retry_after) ? retry_after : "60");
        wait = strdup(w && w->str_val ? w->str_val : "a few seconds");
        json_free(w);
    }
    char buf[1024];
    const char *pos = strstr(tmpl, "{wait}");
    if (pos && wait) {
        size_t head = (size_t)(pos - tmpl);
        snprintf(buf, sizeof(buf), "%.*s%s%s", (int)head, tmpl, wait,
                 pos + strlen("{wait}"));
    } else {
        snprintf(buf, sizeof(buf), "%s", tmpl);
    }
    free((void *)wait);
    return json_string(buf);
}

/* urlparse(str(base_url or "")).hostname, lowercased. */
static char *aa_host(const char *base_url) {
    const char *p = base_url ? base_url : "";
    const char *auth = strstr(p, "://");
    if (auth) p = auth + 3;
    const char *end = p + strlen(p);
    for (const char *q = p; q < end && *q; q++)
        if (*q == '/' || *q == '?' || *q == '#') { end = q; break; }
    const char *at = memchr(p, '@', (size_t)(end - p));
    if (at) p = at + 1;
    const char *colon = memchr(p, ':', (size_t)(end - p));
    if (colon) end = colon;
    size_t n = (size_t)(end - p);
    char *host = (char *)malloc(n + 1);
    if (!host) return NULL;
    for (size_t i = 0; i < n; i++) host[i] = (char)tolower((unsigned char)p[i]);
    host[n] = '\0';
    return host;
}

static int aa_extra_has(const char *extra_hosts, const char *host) {
    if (!extra_hosts || !*extra_hosts || !host || !*host) return 0;
    char *copy = strdup(extra_hosts);
    if (!copy) return 0;
    char *save = NULL;
    int found = 0;
    for (char *tok = strtok_r(copy, ",", &save); tok; tok = strtok_r(NULL, ",", &save)) {
        while (*tok && isspace((unsigned char)*tok)) tok++;
        char *e = tok + strlen(tok);
        while (e > tok && isspace((unsigned char)e[-1])) e--;
        size_t n = (size_t)(e - tok);
        if (n == strlen(host) && !strncasecmp(tok, host, n)) { found = 1; break; }
    }
    free(copy);
    return found;
}

int w7_anonauth_c583c5_route_is_welcome_host(const char *base_url, const char *extra_hosts) {
    char *host = aa_host(base_url);
    if (!host) return 0;
    int yes = !strcmp(host, AA_WELCOME_HOST) || aa_extra_has(extra_hosts, host);
    free(host);
    return yes ? 1 : 0;
}

struct json_t *w7_anonauth_c583c5_pin_model_for_route(const char *provider, const char *base_url,
                                                       const char *model, const char *extra_hosts) {
    if (provider && !strcmp(provider, "nous") &&
        w7_anonauth_c583c5_route_is_welcome_host(base_url, extra_hosts))
        return json_string(AA_GUEST_MODEL);
    return json_string(model ? model : "");
}
