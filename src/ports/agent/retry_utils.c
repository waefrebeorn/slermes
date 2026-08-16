#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
/*
 * ports/agent/retry_utils.c — C11 port of agent/retry_utils.py
 * Merged from 3 source file(s) by reorganization.
 */

/* ── from src/agent/port_agent_retry_utils.c ── */
/* port_agent_retry_utils.c — Thin adapter for legacy agent_retry_utils_* callers.
 *
 * These symbols appear in the Python C-port mapping layer; this file
 * preserves that surface while the canonical implementations live in
 * src/agent/retry_utils.c.
 */


#include "hermes_retry_utils.h"
#include <stdlib.h>
#include <string.h>

/* Legacy wrapper -> canonical retry_utils_* namespace. */
bool agent_retry_utils_is_zai_coding_overload_error(const char *base_url,
                                                    const char *model,
                                                    const retry_utils_err_t *err)
{
    return retry_utils_is_zai_coding_overload_error(base_url, model, err);
}

char *agent_retry_utils__error_text(int status_code,
                                    const char *msg,
                                    const char *body,
                                    const char *response)
{
    retry_utils_err_t err;
    memset(&err, 0, sizeof(err));

    /* Rebuild a flat text from the three fragments, tracking length. */
    char buf[16384];
    size_t n = 0;
    const char *parts[] = { msg, body, response };
    for (int i = 0; i < 3 && parts[i]; i++) {
        if (!parts[i] || !parts[i][0]) continue;
        if (n) { buf[n++] = ' '; if (n >= sizeof(buf) - 1) break; }
        for (const char *p = parts[i]; *p && n + 1 < sizeof(buf); p++) buf[n++] = *p;
    }
    buf[n] = '\0';
    memcpy(err.text, buf, sizeof(err.text) - 1);
    err.text[sizeof(err.text) - 1] = '\0';

    return retry_utils_error_text(&err);
}

char *agent_retry_utils_adaptive_rate_limit_backoff(int attempt,
                                                     const char *base_url,
                                                     const char *model,
                                                     const retry_utils_err_t *err,
                                                     double default_wait,
                                                     int short_attempts)
{
    (void)default_wait;

    char *label = retry_utils_adaptive_rate_limit_backoff(attempt,
                                                           base_url,
                                                           model,
                                                           err,
                                                           default_wait,
                                                           short_attempts);
    if (!label) return NULL;

    /* Normalize "SHORT"/"W|LONG" -> caller-safe labels. */
    if (strcmp(label, "SHORT") == 0) {
        free(label);
        return strdup("SHORT");
    }
    return label;
}

double agent_retry_utils_parse_retry_after_seconds(const char *value, int *ok)
{
    return retry_utils_parse_retry_after_seconds(value, ok);
}


/* ── from src/agent/port_retry_utils_ports.c ── */
/*
 * port_retry_utils_remaining.c — Port of agent/retry_utils.py backoff
 * surface. Jittered exponential backoff, error flattening, zai overload
 * classification, adaptive backoff, retry ceilings.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include <time.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: rtu_jittered_backoff @ agent/retry_utils.py:jittered_backoff */
double rtu_jittered_backoff(long attempt, double base_delay, double max_delay) {
    /* Python: exponential + jitter. */
    if (attempt < 1) attempt = 1;
    if (base_delay <= 0) base_delay = 1.0;
    if (max_delay <= 0) max_delay = 60.0;
    double exp = base_delay * pow(2.0, (double)(attempt - 1));
    if (exp > max_delay) exp = max_delay;
    double jitter = ((double)(rand() % 1000) / 1000.0) * exp * 0.5;
    return exp + jitter;
}

/* PoP: rtu_error_text @ agent/retry_utils.py:_error_text */
char *rtu_error_text(const char *error_json) {
    /* Python: flattened provider error text. */
    if (!error_json) return strdup("");
    const char *m = strstr(error_json, "\"message\"");
    if (m) {
        const char *colon = strchr(m, ':');
        if (colon) {
            const char *v = colon + 1;
            while (*v == ' ' || *v == '"') v++;
            const char *e = v;
            while (*e && *e != '"') e++;
            if (e > v) return strndup(v, (size_t)(e - v));
        }
    }
    return strdup(error_json);
}

/* PoP: rtu_is_zai_coding_overload_error @ agent/retry_utils.py:is_zai_coding_overload_error */
bool rtu_is_zai_coding_overload_error(const char *error) {
    /* Python: Z.AI coding plan transient overload 429. */
    if (!error) return false;
    char *l = lowerdup(error);
    if (!l) return false;
    bool r = strstr(l, "overload") && (strstr(l, "coding") || strstr(l, "429"));
    free(l);
    return r;
}

/* PoP: rtu_adaptive_rate_limit_backoff @ agent/retry_utils.py:adaptive_rate_limit_backoff */
double rtu_adaptive_rate_limit_backoff(const char *provider, const char *error) {
    /* Python: provider-aware default backoff. */
    if (rtu_is_zai_coding_overload_error(error)) return 30.0;
    return 5.0;
}

/* PoP: defaults @ agent/retry_utils.py:zai_coding_overload_retry_ceiling */
/* Python: ceiling = short_attempts + len(LONG_BACKOFF) + 1.
 * With defaults (short_attempts=3, len(LONG_BACKOFF)=4) => 8.
 * The +1 accounts for the retry-count>=ceiling check running before the
 * backoff is computed, so every long-tier entry is reachable. */
long rtu_zai_coding_overload_retry_ceiling(int short_attempts) {
    if (short_attempts < 0) short_attempts = 3;
    static const double LONG_BACKOFF[] = {30.0, 60.0, 90.0, 120.0};
    return short_attempts + (int)(sizeof(LONG_BACKOFF) / sizeof(LONG_BACKOFF[0])) + 1;
}

/* PoP: rtu_parse_retry_after_seconds @ agent/retry_utils.py:parse_retry_after_seconds */
double rtu_parse_retry_after_seconds(const char *value)
{
    /* Parse a "Retry-After" header value into non-negative seconds.
     * Accepts a numeric string / number / HTTP-date (RFC 7231). Returns -1
     * (sentinel for None) when absent or unparseable. Negative deltas are
     * clamped to 0.0. */
    if (!value) return -1.0;
    const char *raw = value;
    /* bool in Python → None; skip. */
    if (strcmp(raw, "true") == 0 || strcmp(raw, "false") == 0) return -1.0;
    /* trim */
    while (*raw == ' ' || *raw == '\t') raw++;
    const char *end = raw + strlen(raw);
    while (end > raw && (end[-1] == ' ' || end[-1] == '\t')) end--;
    if (raw == end) return -1.0;
    /* numeric string */
    char tmp[64];
    size_t len = (size_t)(end - raw);
    if (len >= sizeof(tmp)) len = sizeof(tmp) - 1;
    memcpy(tmp, raw, len); tmp[len] = '\0';
    char *nend = NULL;
    double num = strtod(tmp, &nend);
    if (nend && *nend == '\0') {
        return num < 0.0 ? 0.0 : num;
    }
    /* HTTP-date form (RFC 7231): seconds until that instant, clamped at 0.
     * Formats: IMF-fixdate "Sun, 06 Nov 1994 08:49:37 GMT",
     * RFC 850 "Sunday, 06-Nov-94 08:49:37 GMT",
     * asctime "Sun Nov  6 08:49:37 1994". Dates are GMT. */
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    const char *fmts[] = {
        "%a, %d %b %Y %H:%M:%S GMT",   /* IMF-fixdate */
        "%A, %d-%b-%y %H:%M:%S GMT",   /* RFC 850 */
        "%a %b %e %H:%M:%S %Y",        /* asctime */
        NULL
    };
    for (int i = 0; fmts[i]; i++) {
        memset(&tm, 0, sizeof(tm));
        char *r = strptime(tmp, fmts[i], &tm);
        if (r && *r == '\0') {
            time_t when = timegm(&tm);
            time_t now = time(NULL);
            double secs = (double)(when - now);
            return secs < 0.0 ? 0.0 : secs;
        }
    }
    return -1.0;
}


/* ── from src/agent/port_small_mods_gaps.c ── */
/* Slermes C port — small-module DA-4 gaps (pure functions) */
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "hermes_json.h"
#include "hermes_logger.h"
#include "libcrypto/crypto.h"

/* ── agent/retry_utils.py:parse_retry_after_seconds ── */
/* PoP: retry_parse_retry_after_seconds @ agent/retry_utils.py:parse_retry_after_seconds */
/* PoP: retry_parse_retry_after_seconds @ hermes_cli/auth.py:_parse_retry_after_seconds */
double retry_parse_retry_after_seconds(const char *raw, bool *ok) {
    if (!raw || !*raw) { if (ok) *ok = false; return 0.0; }
    char *end = NULL;
    double val = strtod(raw, &end);
    if (end && *end == '\0') {
        if (ok) *ok = true;
        return val > 0.0 ? val : 0.0;
    }
    /* TODO: HTTP-date form (RFC 7231) — parse with libdatetime */
    if (ok) *ok = false;
    return 0.0;
}

/* ── gateway/slash_commands.py:_redact_matrix_session_key ── */
/* PoP: slash_redact_matrix_session_key @ gateway/slash_commands.py:_redact_matrix_session_key */
void slash_redact_matrix_session_key(const char *session_key, char *out, size_t out_sz) {
    if (!session_key) session_key = "";
    unsigned char hash[CRYPTO_SHA256_LEN];
    crypto_sha256((const unsigned char *)session_key, strlen(session_key), hash);
    snprintf(out, out_sz, "sha256:%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
             hash[0], hash[1], hash[2], hash[3], hash[4], hash[5],
             hash[6], hash[7], hash[8], hash[9], hash[10], hash[11]);
}

/* ── agent/turn_summary.py:_ordered_verbs ── */
/* PoP: ts_ordered_verbs @ agent/turn_summary.py:_ordered_verbs */
json_t *ts_ordered_verbs(json_t *verbs_tally) {
    json_t *result = json_array();
    if (!verbs_tally || verbs_tally->type != JSON_OBJECT) return result;
    const char *priority[] = {"tool", "file", "web", "thinking", NULL};
    for (int i = 0; priority[i]; i++) {
        if (json_obj_get(verbs_tally, priority[i])) {
            json_append(result, json_string(priority[i]));
        }
    }
    /* Append remaining keys (first-seen order) */
    size_t n = json_len(verbs_tally);
    for (size_t idx = 0; idx < n; idx++) {
        const char *key = verbs_tally->c.keys[idx];
        bool already = false;
        for (int i = 0; priority[i]; i++) {
            if (strcmp(key, priority[i]) == 0) { already = true; break; }
        }
        if (!already) {
            json_append(result, json_string(key));
        }
    }
    return result;
}

/* ── agent/credits_tracker.py:new_credits_latch ── */
/* PoP: credits_new_latch @ agent/credits_tracker.py:new_credits_latch */
json_t *credits_new_latch(void) {
    json_t *latch = json_object();
    json_set(latch, "active", json_array());
    json_set(latch, "seen_below_90", json_bool(false));
    json_set(latch, "usage_band", json_null());
    json_set(latch, "seen_grant_unspent", json_bool(false));
    return latch;
}

/* ── agent/learning_graph_render.py:build_summary ── */
/* PoP: learn_graph_build_summary @ agent/learning_graph_render.py:build_summary */
json_t *learn_graph_build_summary(json_t *payload) {
    if (!payload) return json_array();
    json_t *stats = json_obj_get(payload, "stats");
    if (!stats || stats->type != JSON_OBJECT) stats = json_object();
    json_t *lines = json_array();

    int learned = (int)json_get_num(stats, "learned_skills", (int)json_get_num(stats, "nodes", 0));
    int mem = (int)json_get_num(stats, "memory_nodes", 0);
    int edges = (int)json_get_num(stats, "related_edges", 0);

    char buf[256];
    snprintf(buf, sizeof(buf), "%d learned skills · %d memories · %d skill links", learned, mem, edges);
    json_append(lines, json_string(buf));

    return lines;
}

