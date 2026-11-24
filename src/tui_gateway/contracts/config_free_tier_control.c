/*
 * config_free_tier_control.c — C port of upstream Hermes
 * tui_gateway/contracts/config_free_tier_control.py (gate w33_configfree_3f224a).
 *
 * The contracts module fixes the wire shape; the values behind it are decided by
 * the pure bodies this file ports:
 *
 * PoP: w33_configfree_3f224a_status_payload @ tui_gateway/methods_free_tier.py:free_tier.status
 * PoP: w33_configfree_3f224a_guest_state @ hermes_cli/anon_auth.py:is_guest_state
 * PoP: w33_configfree_3f224a_free_inference @ hermes_cli/anon_auth.py:free_inference_active
 * PoP: w33_configfree_3f224a_guest_without_tier @ hermes_cli/anon_auth.py:guest_without_free_tier
 * PoP: w33_configfree_3f224a_notice_pending @ hermes_cli/anon_auth.py:guest_notice_pending
 * PoP: w33_configfree_3f224a_failure_copy @ hermes_cli/anon_auth.py:anon_failure_copy
 * PoP: w33_configfree_3f224a_mint_payload @ hermes_cli/anon_auth.py:MintFailure.as_payload
 * PoP: w33_configfree_3f224a_challenge_parse @ hermes_cli/anon_challenge.py:parse_browser_challenge
 * PoP: w33_configfree_3f224a_challenge_payload @ hermes_cli/anon_challenge.py:pending_challenge
 * PoP: w33_configfree_3f224a_challenge_outcome @ hermes_cli/anon_challenge.py:record_host_outcome
 * PoP: w33_configfree_3f224a_claim_blocker @ hermes_cli/welcome_trial.py:claim_blocker
 *
 * Seams are fixture-fed: the auth-store state, the `nous.guest` switch, the mint
 * memo, the install probe, the monotonic clock and the offer schedule.  No file,
 * clock or network is read here.
 */
#include "noc_w33_configfree_3f224a.h"
#include "hermes_json.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CF_MAX_MESSAGE 300
#define CF_CHALLENGE_COPY "Nous needs to run a quick check before starting your free session."

/* ------------------------------------------------------------------ helpers */

/* Python truthiness over a JSON value. */
static int truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
        case JSON_NULL: return 0;
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0];
        default: return v->c.count > 0;
    }
}

/* `v is True` — boolean true only (never 1, never "true"). */
static int is_true(const json_t *v) {
    return v && v->type == JSON_BOOL && v->bool_val;
}

/* Strict string compare: 1 when v is a string equal to s. */
static int str_is(const json_t *v, const char *s) {
    return v && v->type == JSON_STRING && v->str_val && strcmp(v->str_val, s) == 0;
}

/* `value` as Python float(): number, bool and numeric string convert; anything
 * else (null, list, object, non-numeric string) is TypeError / ValueError. */
static double as_py_float(const json_t *v, int *ok) {
    *ok = 0;
    if (!v) return 0.0;
    if (v->type == JSON_NUMBER) { *ok = 1; return v->num_val; }
    if (v->type == JSON_BOOL) { *ok = 1; return v->bool_val ? 1.0 : 0.0; }
    if (v->type == JSON_STRING && v->str_val) {
        char *end = NULL;
        double d = strtod(v->str_val, &end);
        if (end == v->str_val) return 0.0;
        while (end && *end && isspace((unsigned char)*end)) end++;
        if (end && *end != '\0') return 0.0;
        *ok = 1;
        return d;
    }
    return 0.0;
}

/* hermes_cli/anon_auth.py::friendly_wait — a rounded spoken duration. */
static void friendly_wait_num(double s, char *out, size_t n) {
    if (!(s > 0.0)) s = 0.0;               /* max(0.0, s); NaN reads as 0.0 */
    if (s <= 15.0) { snprintf(out, n, "a few seconds"); return; }
    if (s < 90.0) { snprintf(out, n, "about a minute"); return; }
    if (s < 3600.0) {
        long minutes = (long)rint(s / 60.0);   /* Python round(): ties to even */
        snprintf(out, n, "about %ld minutes", minutes);
        return;
    }
    long hours = (long)rint(s / 3600.0);
    if (hours <= 1) snprintf(out, n, "about an hour");
    else snprintf(out, n, "about %ld hours", hours);
}

/* ------------------------------------------------------------------- codes */

#define CF_GATE_CLOSED "anon_gate_closed"
#define CF_GATE_PAUSED "anon_gate_paused"
#define CF_RATE_LIMITED "anon_rate_limited"
#define CF_POW_REQUIRED "anon_pow_required"
#define CF_ACCOUNT_LOCKED "anon_account_locked"
#define CF_CREDENTIAL_DEAD "anon_credential_dead"
#define CF_UNREACHABLE "anon_unreachable"
#define CF_SERVER_ERROR "anon_server_error"
#define CF_CHALLENGE_REQUIRED "anon_challenge_required"
#define CF_SIGNIN_REQUIRED "anon_signin_required"

/* ANON_FAILURE_COPY, verbatim. */
static const char *failure_template(const char *code) {
    if (!code) return NULL;
    if (!strcmp(code, CF_GATE_CLOSED))
        return "This version can't be used without a Nous account. Signing in is free.";
    if (!strcmp(code, CF_GATE_PAUSED))
        return "Using Hermes without signing in is paused for a moment. Signing in is free.";
    if (!strcmp(code, CF_RATE_LIMITED))
        return "Lots of people are getting started right now. Try again in {wait}. "
               "Signing in is free and skips the wait.";
    if (!strcmp(code, CF_POW_REQUIRED))
        return "The Nous server asked for a proof of work, but that isn't implemented in your "
               "Agent yet. Sign in with a Nous account to continue.";
    if (!strcmp(code, CF_CHALLENGE_REQUIRED))
        return "Finish the quick check in your browser, then try again.";
    if (!strcmp(code, CF_SIGNIN_REQUIRED))
        return "Free guest access isn't available here. Sign in with a Nous account to continue.";
    if (!strcmp(code, CF_ACCOUNT_LOCKED))
        return "This session can't continue without signing in. Signing in is free.";
    if (!strcmp(code, CF_CREDENTIAL_DEAD))
        return "Your session ended. A new one starts on its own.";
    if (!strcmp(code, CF_UNREACHABLE))
        return "The Nous service couldn't be reached. Check your internet connection and try again.";
    if (!strcmp(code, CF_SERVER_ERROR))
        return "The Nous service had a hiccup. Try again in a moment.";
    return NULL;
}

/* ------------------------------------------------------- entitlement predicates */

int w33_configfree_3f224a_guest_state(const json_t *state) {
    if (!state || state->type != JSON_OBJECT) return 0;
    return str_is(json_obj_get(state, "auth_method"), W33_CONFIGFREE_3F224A_AUTH_METHOD);
}

int w33_configfree_3f224a_free_inference(const json_t *state, int guest_allowed) {
    if (!w33_configfree_3f224a_guest_state(state)) return 0;
    if (!is_true(json_obj_get(state, W33_CONFIGFREE_3F224A_WELCOME_FLAG))) return 0;
    return guest_allowed ? 1 : 0;
}

int w33_configfree_3f224a_guest_without_tier(const json_t *state, int guest_allowed) {
    if (!w33_configfree_3f224a_guest_state(state)) return 0;
    return w33_configfree_3f224a_free_inference(state, guest_allowed) ? 0 : 1;
}

int w33_configfree_3f224a_notice_pending(const json_t *state, int guest_allowed) {
    if (!w33_configfree_3f224a_free_inference(state, guest_allowed)) return 0;
    return truthy(json_obj_get(state, W33_CONFIGFREE_3F224A_NOTICE_FLAG)) ? 0 : 1;
}

/* ------------------------------------------------------------- failure copy */

json_t *w33_configfree_3f224a_failure_copy(const char *code, const json_t *retry_after) {
    const char *tpl = failure_template(code);
    if (!tpl) tpl = failure_template(CF_SERVER_ERROR);
    char wait[64];
    const json_t *ra = truthy(retry_after) ? retry_after : NULL;
    if (ra) {
        int ok = 0;
        double s = as_py_float(ra, &ok);
        friendly_wait_num(ok ? s : 0.0, wait, sizeof wait);
    } else {
        friendly_wait_num(60.0, wait, sizeof wait);
    }
    const char *hit = strstr(tpl, "{wait}");
    if (!hit) return json_string(tpl);
    char buf[1024];
    size_t pre = (size_t)(hit - tpl);
    if (pre >= sizeof buf) pre = sizeof buf - 1;
    memcpy(buf, tpl, pre);
    size_t o = pre;
    size_t wl = strlen(wait);
    if (o + wl >= sizeof buf) wl = sizeof buf - 1 - o;
    memcpy(buf + o, wait, wl);
    o += wl;
    const char *rest = hit + 6;   /* strlen("{wait}") */
    while (*rest && o + 1 < sizeof buf) buf[o++] = *rest++;
    buf[o] = '\0';
    return json_string(buf);
}

/* ------------------------------------------------------------ mint failure */

/* Python round(x, 3): decimal rounding half-to-even of the exact binary value. */
static double round3(double x) {
    char b[64];
    snprintf(b, sizeof b, "%.3f", x);
    return strtod(b, NULL);
}

json_t *w33_configfree_3f224a_mint_payload(const json_t *failure, double now) {
    if (!failure || failure->type != JSON_OBJECT) return json_null();
    int retryable = is_true(json_obj_get(failure, "retryable"));
    double not_before = 0.0;
    const json_t *nb = json_obj_get(failure, "not_before");
    if (nb && nb->type == JSON_NUMBER) not_before = nb->num_val;
    double remaining = 0.0;
    if (retryable) {
        remaining = round3(not_before - now);
        if (!(remaining > 0.0)) remaining = 0.0;   /* max(0.0, remaining) */
    }
    json_t *o = json_object();
    json_set(o, "error_code", json_string(json_get_str(failure, "code", "")));
    json_set(o, "error", json_string(json_get_str(failure, "message", "")));
    json_set(o, "retryable", json_bool(retryable ? true : false));
    json_set(o, "retry_after", json_number(retryable ? ceil(remaining) : 0.0));
    return o;
}

/* --------------------------------------------------------------- URL origin */

typedef struct {
    char scheme[16];
    char netloc[512];
} url_parts;

/* urlsplit, to the extent _same_origin reads it: lowercased scheme and the
 * authority up to the first '/', '?' or '#'.  An unmatched '[' in the netloc is
 * the ValueError urlparse raises. */
static int url_split(const char *s, url_parts *p) {
    p->scheme[0] = '\0';
    p->netloc[0] = '\0';
    if (!s) return 0;
    const char *colon = strchr(s, ':');
    if (!colon) return 0;
    for (const char *q = s; q < colon; q++)
        if (*q == '/' || *q == '?' || *q == '#') return 0;
    size_t sl = (size_t)(colon - s);
    if (sl == 0 || sl >= sizeof p->scheme) return 0;
    for (size_t i = 0; i < sl; i++) {
        unsigned char c = (unsigned char)s[i];
        int alpha = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
        int okc = alpha || (i > 0 && ((c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.'));
        if (!okc) return 0;
    }
    for (size_t i = 0; i < sl; i++) p->scheme[i] = (char)tolower((unsigned char)s[i]);
    p->scheme[sl] = '\0';
    const char *rest = colon + 1;
    if (rest[0] == '/' && rest[1] == '/') {
        const char *a = rest + 2;
        const char *b = a;
        while (*b && *b != '/' && *b != '?' && *b != '#') b++;
        size_t nl = (size_t)(b - a);
        if (nl >= sizeof p->netloc) nl = sizeof p->netloc - 1;
        memcpy(p->netloc, a, nl);
        p->netloc[nl] = '\0';
    }
    if (strchr(p->netloc, '[') && !strchr(p->netloc, ']')) return 0;  /* ValueError */
    return 1;
}

static int netloc_eq_ci(const char *a, const char *b) {
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static int same_origin(const char *url, const char *portal_base_url) {
    url_parts t, p;
    if (!url_split(url, &t) || !url_split(portal_base_url, &p)) return 0;
    if (strcmp(t.scheme, "https") != 0 && strcmp(t.scheme, "http") != 0) return 0;
    if (strcmp(t.scheme, p.scheme) != 0) return 0;
    if (t.netloc[0] == '\0') return 0;
    return netloc_eq_ci(t.netloc, p.netloc);
}

/* ------------------------------------------------------------ server copy */

/* anon_challenge.py::server_message — printable single-paragraph text only,
 * whitespace collapsed, capped at 300 characters; anything else reads "". */
static size_t server_message(const json_t *v, char *out, size_t n) {
    out[0] = '\0';
    if (!v || v->type != JSON_STRING || !v->str_val) return 0;
    char tmp[2048];
    size_t t = 0;
    for (const unsigned char *p = (const unsigned char *)v->str_val; *p && t + 1 < sizeof tmp; p++) {
        unsigned char c = *p;
        int printable = (c >= 0x20 && c != 0x7f) || c >= 0x80;
        tmp[t++] = printable ? (char)c : ' ';
    }
    tmp[t] = '\0';
    size_t o = 0;
    int pending_space = 0;
    for (size_t i = 0; i < t; i++) {
        if (tmp[i] == ' ') { pending_space = 1; continue; }
        if (pending_space && o > 0 && o + 1 < n) out[o++] = ' ';
        pending_space = 0;
        if (o + 1 < n) out[o++] = tmp[i];
    }
    out[o] = '\0';
    if (o == 0 || o > CF_MAX_MESSAGE) { out[0] = '\0'; return 0; }
    return o;
}

/* --------------------------------------------------------------- challenge */

/* anon_challenge.py::_number — numbers only, booleans and non-finite excluded. */
static double challenge_number(const json_t *v, double def) {
    if (!v || v->type != JSON_NUMBER) return def;
    if (!isfinite(v->num_val)) return def;
    return v->num_val;
}

static int string_is_printable(const char *s) {
    if (!s || !s[0]) return 0;   /* "".isprintable() is False */
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if (!((*p >= 0x20 && *p != 0x7f) || *p >= 0x80)) return 0;
    return 1;
}

json_t *w33_configfree_3f224a_challenge_parse(const json_t *payload, const char *portal_base_url) {
    if (!payload || payload->type != JSON_OBJECT) return json_null();
    const json_t *entries = json_obj_get(payload, "challenges");
    if (!entries || entries->type != JSON_ARRAY) return json_null();
    char message[2048];
    if (server_message(json_obj_get(payload, "message"), message, sizeof message) == 0)
        snprintf(message, sizeof message, "%s", CF_CHALLENGE_COPY);
    const char *portal = portal_base_url ? portal_base_url : "";
    for (size_t i = 0; i < json_len(entries); i++) {
        const json_t *entry = json_get(entries, i);
        if (!entry || entry->type != JSON_OBJECT) continue;
        if (!str_is(json_obj_get(entry, "type"), "browser")) continue;
        const json_t *url = json_obj_get(entry, "url");
        if (!url || url->type != JSON_STRING || !url->str_val) continue;
        if (!string_is_printable(url->str_val)) continue;
        if (!same_origin(url->str_val, portal)) continue;
        double expires = challenge_number(json_obj_get(entry, "expires_in"), 600.0);
        if (expires < 30.0) expires = 30.0;
        if (expires > 900.0) expires = 900.0;
        double interval = challenge_number(json_obj_get(entry, "interval"), 2.0);
        if (interval < 1.0) interval = 1.0;
        if (interval > 5.0) interval = 5.0;
        const json_t *req = json_obj_get(entry, "required");
        int required = (req && req->type == JSON_BOOL && !req->bool_val) ? 0 : 1;
        json_t *o = json_object();
        json_set(o, "url", json_string(url->str_val));
        json_set(o, "required", json_bool(required ? true : false));
        json_set(o, "expires_in", json_number(expires));
        json_set(o, "interval", json_number(interval));
        json_set(o, "message", json_string(message));
        json_set(o, "attempt", json_number(0));
        return o;
    }
    return json_null();
}

/* anon_challenge.py::_live — an expired record is cleared before it is read. */
static const json_t *live_pending(const json_t *rec, double now, const json_t **outcome) {
    *outcome = NULL;
    if (!rec || rec->type != JSON_OBJECT) return NULL;
    const json_t *pending = json_obj_get(rec, "pending");
    if (!truthy(pending)) return NULL;
    const json_t *dl = json_obj_get(rec, "deadline");
    double deadline = (dl && dl->type == JSON_NUMBER) ? dl->num_val : 0.0;
    if (deadline <= now) return NULL;
    *outcome = json_obj_get(rec, "outcome");
    return pending;
}

json_t *w33_configfree_3f224a_challenge_payload(const json_t *rec, double now) {
    const json_t *outcome = NULL;
    const json_t *pending = live_pending(rec, now, &outcome);
    if (!pending) return json_null();
    /* A window the host said how it ended must not come back (a timeout / error
     * still leaves the ticket workable). */
    if (outcome && outcome->type != JSON_NULL
        && !str_is(outcome, "timeout") && !str_is(outcome, "error"))
        return json_null();
    const json_t *dl = json_obj_get(rec, "deadline");
    double deadline = (dl && dl->type == JSON_NUMBER) ? dl->num_val : 0.0;
    const json_t *req = json_obj_get(pending, "required");
    int required = (req && req->type == JSON_BOOL && !req->bool_val) ? 0 : 1;
    const json_t *attempt = json_obj_get(pending, "attempt");
    long att = (attempt && attempt->type == JSON_NUMBER) ? (long)attempt->num_val : 0;
    json_t *o = json_object();
    json_set(o, "type", json_string("browser"));
    json_set(o, "url", json_string(json_get_str(pending, "url", "")));
    json_set(o, "required", json_bool(required ? true : false));
    json_set(o, "expires_in", json_number(ceil(deadline - now)));
    json_set(o, "message", json_string(json_get_str(pending, "message", "")));
    json_set(o, "attempt", json_number((double)att));
    return o;
}

int w33_configfree_3f224a_challenge_outcome(const json_t *rec, const char *url, long attempt,
                                            double now) {
    const json_t *outcome = NULL;
    const json_t *pending = live_pending(rec, now, &outcome);
    (void)outcome;
    if (!pending) return 0;
    const json_t *purl = json_obj_get(pending, "url");
    if (!purl || purl->type != JSON_STRING || !purl->str_val || !url) return 0;
    if (strcmp(purl->str_val, url) != 0) return 0;
    const json_t *patt = json_obj_get(pending, "attempt");
    long patt_v = (patt && patt->type == JSON_NUMBER) ? (long)patt->num_val : 0;
    return patt_v == attempt ? 1 : 0;
}

/* ------------------------------------------------------------- claim ladder */

json_t *w33_configfree_3f224a_claim_blocker(const json_t *ask) {
    int assume_fresh = truthy(json_obj_get(ask, "assume_fresh"));
    if (!truthy(json_obj_get(ask, "on_local_desktop_backend")))
        return json_string("not the desktop's local backend");
    if (!truthy(json_obj_get(ask, "guest_allowed")))
        return json_string("guests are switched off");
    if (truthy(json_obj_get(ask, "has_nous_identity")))
        return json_string("a Nous identity exists");
    const json_t *configured = json_obj_get(ask, "configured");
    int is_configured = (configured && configured->type != JSON_NULL)
                            ? truthy(configured)
                            : truthy(json_obj_get(ask, "provider_configured"));
    if (is_configured) return json_string("a provider is configured");
    if (!assume_fresh && truthy(json_obj_get(ask, "machine_used"))
        && !truthy(json_obj_get(ask, "trial_reserved")))
        return json_string("Hermes was used on this machine");
    return json_null();
}

/* ---------------------------------------------------------- status payload */

json_t *w33_configfree_3f224a_status_payload(const json_t *identity) {
    const json_t *state = json_obj_get(identity, "state");
    int guest_allowed = truthy(json_obj_get(identity, "guest_allowed")) ? 1 : 0;
    int has_guest = w33_configfree_3f224a_guest_state(state);
    int available = w33_configfree_3f224a_free_inference(state, guest_allowed);
    const json_t *blocker = json_obj_get(identity, "trial_blocker");
    int trial_claimable = (blocker == NULL || blocker->type == JSON_NULL) ? 1 : 0;
    int enabled = available || trial_claimable;
    int notice_pending = available && w33_configfree_3f224a_notice_pending(state, guest_allowed);

    json_t *o = json_object();
    json_set(o, "has_guest", json_bool(has_guest ? true : false));
    json_set(o, "enabled", json_bool(enabled ? true : false));
    json_set(o, "available", json_bool(available ? true : false));
    json_set(o, "notice_pending", json_bool(notice_pending ? true : false));
    json_set(o, "model", json_string(W33_CONFIGFREE_3F224A_GUEST_MODEL));
    json_set(o, "label", json_string(W33_CONFIGFREE_3F224A_LABEL));

    const json_t *nudge = json_obj_get(identity, "nudge_due_in");
    if (nudge && nudge->type != JSON_NULL) json_set(o, "nudge_due_in", json_copy(nudge));
    const json_t *cnudge = json_obj_get(identity, "connector_nudge_due_in");
    if (cnudge && cnudge->type != JSON_NULL)
        json_set(o, "connector_nudge_due_in", json_copy(cnudge));

    if (enabled && !has_guest) {
        const json_t *failure = json_obj_get(identity, "mint_failure");
        if (failure && failure->type == JSON_OBJECT) {
            for (size_t i = 0; i < json_object_size(failure); i++) {
                const char *k = json_object_get_key_at(failure, i);
                if (!k) continue;
                json_set(o, k, json_copy(json_object_get_at(failure, i)));
            }
        }
    }
    const json_t *challenge = json_obj_get(identity, "challenge");
    if (truthy(challenge)) json_set(o, "challenge", json_copy(challenge));
    return o;
}
