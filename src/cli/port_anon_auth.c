/*
 * port_anon_auth.c — Port of hermes_cli/anon_auth.py.
 *
 * The free-tier (anonymous Nous) policy cluster: spoken durations, guest-state
 * predicates, welcome-host routing, and the per-code failure copy. Pure except
 * for the two config/env seams, which are taken as parameters / getenv so the
 * C caller owns them.
 */

#include "port_anon_auth.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ANON_AUTH_METHOD "anonymous"
#define ANON_ACCOUNT_TIER "anonymous"
#define GUEST_MODEL "nous/welcome"
#define WELCOME_CLAIM_FLAG "welcome_claim"
#define WELCOME_HOST "welcome-api.nousresearch.com"
#define EXTRA_WELCOME_HOSTS_ENV "HERMES_EXTRA_WELCOME_HOSTS"

/* Python 3 round(): banker's rounding (half to even). */
static double py_round(double x)
{
    double f = floor(x);
    double diff = x - f;
    if (diff < 0.5)
        return f;
    if (diff > 0.5)
        return f + 1.0;
    return fmod(f, 2.0) == 0.0 ? f : f + 1.0;
}

/* PoP: friendly_wait @ hermes_cli/anon_auth.py:friendly_wait */
void anon_friendly_wait(double seconds, char *out, size_t outsz)
{
    if (!out || outsz == 0)
        return;
    double s = seconds > 0.0 ? seconds : 0.0;
    if (s <= 15.0) {
        snprintf(out, outsz, "a few seconds");
    } else if (s < 90.0) {
        snprintf(out, outsz, "about a minute");
    } else if (s < 3600.0) {
        snprintf(out, outsz, "about %.0f minutes", py_round(s / 60.0));
    } else {
        double hours = py_round(s / 3600.0);
        if (hours <= 1.0)
            snprintf(out, outsz, "about an hour");
        else
            snprintf(out, outsz, "about %.0f hours", hours);
    }
}

/* PoP: is_guest_state @ hermes_cli/anon_auth.py:is_guest_state */
int anon_is_guest_state(json_t *state)
{
    if (!json_is_object(state))
        return 0;
    json_t *m = json_obj_get(state, "auth_method");
    return json_is_string(m) && strcmp(json_string_value(m), ANON_AUTH_METHOD) == 0;
}

/* PoP: welcome_hosts @ hermes_cli/anon_auth.py:welcome_hosts */
json_t *anon_welcome_hosts(void)
{
    json_t *out = json_new_array();
    json_array_append(out, json_new_string(WELCOME_HOST));
    const char *raw = getenv(EXTRA_WELCOME_HOSTS_ENV);
    if (raw) {
        char *copy = strdup(raw);
        if (copy) {
            for (char *part = strtok(copy, ","); part;
                 part = strtok(NULL, ",")) {
                while (isspace((unsigned char)*part))
                    part++;
                char *end = part + strlen(part);
                while (end > part && isspace((unsigned char)end[-1]))
                    *--end = '\0';
                if (!*part)
                    continue;
                for (char *p = part; *p; p++)
                    *p = (char)tolower((unsigned char)*p);
                json_array_append(out, json_new_string(part));
            }
            free(copy);
        }
    }
    return out;
}

/* urllib.parse.urlsplit(...).hostname, faithful to the cases the routing
 * predicate meets: a scheme is only recognised when followed by "://", and a
 * bare host (no scheme) yields no hostname. */
static char *url_hostname(const char *base_url)
{
    if (!base_url)
        return NULL;
    const char *p = base_url;
    const char *sep = strstr(p, "://");
    const char *authority;
    if (sep) {
        authority = sep + 3;
    } else {
        /* no "://": Python's urlparse finds a scheme only when the string
         * starts with scheme: — and then netloc stays empty. */
        return NULL;
    }
    size_t n = 0;
    while (authority[n] && authority[n] != '/' && authority[n] != '?' &&
           authority[n] != '#')
        n++;
    /* strip userinfo */
    size_t at = n;
    for (size_t i = 0; i < n; i++)
        if (authority[i] == '@')
            at = i;
    const char *host = authority + (at < n ? at + 1 : 0);
    size_t hlen = n - (at < n ? at + 1 : 0);
    /* strip :port (not inside IPv6 brackets) */
    int depth = 0;
    for (size_t i = 0; i < hlen; i++) {
        if (host[i] == '[')
            depth++;
        else if (host[i] == ']')
            depth--;
        else if (host[i] == ':' && depth == 0) {
            hlen = i;
            break;
        }
    }
    char *out = (char *)malloc(hlen + 1);
    if (!out)
        return NULL;
    for (size_t i = 0; i < hlen; i++)
        out[i] = (char)tolower((unsigned char)host[i]);
    out[hlen] = '\0';
    return out;
}

/* PoP: route_is_welcome_host @ hermes_cli/anon_auth.py:route_is_welcome_host */
int anon_route_is_welcome_host(const char *base_url)
{
    char *host = url_hostname(base_url);
    if (!host || !*host) {
        free(host);
        return 0;
    }
    json_t *hosts = anon_welcome_hosts();
    int found = 0;
    if (json_is_array(hosts)) {
        size_t n = json_len(hosts);
        for (size_t i = 0; i < n; i++) {
            json_t *h = json_get(hosts, i);
            if (json_is_string(h) && strcmp(json_string_value(h), host) == 0) {
                found = 1;
                break;
            }
        }
    }
    json_free(hosts);
    free(host);
    return found;
}

/* PoP: route_can_serve_model @ hermes_cli/anon_auth.py:route_can_serve_model */
int anon_route_can_serve_model(const char *provider, const char *base_url,
                               const char *model)
{
    if (!provider || strcmp(provider, "nous") != 0 ||
        !anon_route_is_welcome_host(base_url))
        return 1;
    return !model || !*model || strcmp(model, GUEST_MODEL) == 0;
}

/* PoP: pin_model_for_route @ hermes_cli/anon_auth.py:pin_model_for_route */
void anon_pin_model_for_route(const char *provider, const char *base_url,
                              const char *model, char *out, size_t outsz)
{
    if (!out || outsz == 0)
        return;
    if (provider && strcmp(provider, "nous") == 0 &&
        anon_route_is_welcome_host(base_url)) {
        snprintf(out, outsz, "%s", GUEST_MODEL);
        return;
    }
    snprintf(out, outsz, "%s", model ? model : "");
}

/* PoP: free_inference_active @ hermes_cli/anon_auth.py:free_inference_active */
int anon_free_inference_active(json_t *state, int guest_allowed)
{
    if (!anon_is_guest_state(state) || !guest_allowed)
        return 0;
    json_t *flag = json_obj_get(state, WELCOME_CLAIM_FLAG);
    return json_is_bool(flag) && json_bool_value(flag);
}

/* PoP: guest_without_free_tier @ hermes_cli/anon_auth.py:guest_without_free_tier */
int anon_guest_without_free_tier(json_t *state, int guest_allowed)
{
    return anon_is_guest_state(state) && !anon_free_inference_active(state, guest_allowed);
}

/* PoP: has_free_tier_account @ hermes_cli/anon_auth.py:has_free_tier_account */
int anon_has_free_tier_account(json_t *state, int guest_allowed)
{
    return anon_free_inference_active(state, guest_allowed);
}

/* PoP: trial_model @ hermes_cli/anon_auth.py:trial_model */
void anon_trial_model(json_t *state, int guest_allowed, char *out, size_t outsz)
{
    if (!out || outsz == 0)
        return;
    snprintf(out, outsz, "%s",
             anon_free_inference_active(state, guest_allowed) ? GUEST_MODEL : "");
}

/* ANON_FAILURE_COPY, verbatim from the Python module. */
typedef struct {
    const char *code;
    const char *copy;
} anon_copy_row;

static const anon_copy_row ANON_FAILURE_COPY[] = {
    {"anon_gate_closed",
     "This version can't be used without a Nous account. Signing in is free."},
    {"anon_gate_paused",
     "Using Hermes without signing in is paused for a moment. Signing in is free."},
    {"anon_rate_limited",
     "Lots of people are getting started right now. Try again in {wait}. "
     "Signing in is free and skips the wait."},
    {"anon_pow_required",
     "The Nous server asked for a proof of work, but that isn't implemented in your "
     "Agent yet. Sign in with a Nous account to continue."},
    {"anon_challenge_required",
     "Finish the quick check in your browser, then try again."},
    {"anon_signin_required",
     "Free guest access isn't available here. Sign in with a Nous account to continue."},
    {"anon_account_locked",
     "This session can't continue without signing in. Signing in is free."},
    {"anon_credential_dead", "Your session ended. A new one starts on its own."},
    {"anon_unreachable",
     "The Nous service couldn't be reached. Check your internet connection and try again."},
    {"anon_server_error", "The Nous service had a hiccup. Try again in a moment."},
};

/* PoP: anon_failure_copy @ hermes_cli/anon_auth.py:anon_failure_copy */
void anon_failure_copy(const char *code, double retry_after, char *out,
                       size_t outsz)
{
    if (!out || outsz == 0)
        return;
    const char *tmpl = NULL;
    size_t n = sizeof(ANON_FAILURE_COPY) / sizeof(ANON_FAILURE_COPY[0]);
    for (size_t i = 0; i < n; i++)
        if (code && strcmp(code, ANON_FAILURE_COPY[i].code) == 0)
            tmpl = ANON_FAILURE_COPY[i].copy;
    if (!tmpl)
        tmpl = ANON_FAILURE_COPY[n - 1].copy;      /* ANON_SERVER_ERROR */

    char wait[64];
    anon_friendly_wait(retry_after > 0.0 ? retry_after : 60.0, wait, sizeof(wait));

    /* str.format(wait=...) — the only placeholder in the table is {wait}. */
    size_t i = 0, o = 0;
    while (tmpl[i] && o + 1 < outsz) {
        if (tmpl[i] == '{' && strncmp(tmpl + i, "{wait}", 6) == 0) {
            for (const char *w = wait; *w && o + 1 < outsz; w++)
                out[o++] = *w;
            i += 6;
            continue;
        }
        out[o++] = tmpl[i++];
    }
    out[o] = '\0';
}
