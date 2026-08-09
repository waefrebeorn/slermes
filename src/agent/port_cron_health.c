/*
 * port_cron_health.c — Faithful C11 port of agent/monitoring/cron_health.py.
 *
 * Pure content-free cron telemetry projection: job-key derivation, error
 * classification (POSIX regex via hermes_regex), timestamp/duration parsing,
 * and CronExecutionEvent projection.
 *
 * Reuses: libcrypto (crypto_sha256, crypto_hex_encode), hermes_regex,
 *         libdatetime (datetime_parse_iso8601 for the integer-second path),
 *         hermes_json (record dict navigation), hermes_time (now_ns).
 */

#include "cron_health.h"
#include "hermes_crypto.h"
#include "hermes_regex.h"
#include "datetime.h"
#include "hermes_logger.h"

#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#define KNOWN_STATUSES  {"claimed","running","completed","failed","unknown"}
#define CRON_KNOWN_SOURCES {"builtin","direct","external"}
#define CRON_KNOWN_DELIVERY_OUTCOMES {"delivered","failed","suppressed","not_configured"}

/* ── _job_key ────────────────────────────────────────────────────── */
/* PoP: _job_key @ agent/monitoring/cron_health.py:_job_key */
char *cron_job_key(const char *raw) {
    /* mirror: str(raw or "unknown") — empty string is falsy in Python */
    const char *value = (raw && raw[0] != '\0') ? raw : "unknown";
    unsigned char digest[CRYPTO_SHA256_LEN];
    crypto_sha256((const unsigned char *)value, strlen(value), digest);
    char *hex = crypto_hex_encode(digest, CRYPTO_SHA256_LEN);
    if (!hex) return NULL;
    /* truncate to first 24 hex chars, prefix "sha256:" */
    char *key = malloc(8 + 24 + 1);
    if (!key) { free(hex); return NULL; }
    memcpy(key, "sha256:", 7);
    memcpy(key + 7, hex, 24);
    key[31] = '\0';
    free(hex);
    return key;
}

/* ── classify_cron_error ────────────────────────────────────────── */
/* PoP: classify_cron_error @ agent/monitoring/cron_health.py:classify_cron_error
 *
 * Faithful port of the Python regex cascade. Each branch compiles to a POSIX
 * extended regex; matches are case-insensitive (Python .lower() applied first). */
const char *classify_cron_error(const char *raw) {
    /* mirror: text = str(raw or "").lower() */
    char text[1024];
    if (raw) {
        size_t i, n = strlen(raw);
        if (n > sizeof(text) - 1) n = sizeof(text) - 1;
        for (i = 0; i < n; i++) text[i] = (char)tolower((unsigned char)raw[i]);
        text[n] = '\0';
    } else { text[0] = '\0'; }

    /* auth_failed */
    {
        static const char *auth_patterns[] = {
            "\\bauthentication\\b|\\bauthenticated\\b|\\baunthenticate\\b|\\bauthorize\\b|\\bunauthorized\\b|\\bforbidden\\b",
            "\\bbearer\\b",
            "\\baccess token\\b|\\bapi token\\b|\\brefresh token\\b",
            "\\b401\\b|\\b403\\b",
            NULL
        };
        for (int i = 0; auth_patterns[i]; i++) {
            hregex_t *re = regex_compile(auth_patterns[i], 1); /* 1 = REG_ICASE */
            if (re) {
                regex_match_t *m = regex_search(re, text);
                bool hit = m && m->matched;
                if (m) regex_match_free(m);
                regex_free(re);
                if (hit) return "auth_failed";
            }
        }
    }
    /* rate_limited */
    if (strstr(text, "rate limit") || strstr(text, "429") || strstr(text, "quota"))
        return "rate_limited";
    /* timeout */
    if (strstr(text, "timeout") || strstr(text, "timed out"))
        return "timeout";
    /* network_error */
    {
        const char *needle[] = {"network","connection","dns","socket","unreachable",NULL};
        for (int i = 0; needle[i]; i++) if (strstr(text, needle[i])) return "network_error";
    }
    /* dispatch_failed */
    if (strstr(text, "dispatch") || strstr(text, "executor"))
        return "dispatch_failed";
    /* interrupted */
    if (strstr(text, "interrupt") || strstr(text, "owner exited") || strstr(text, "restarted"))
        return "interrupted";
    /* empty_response */
    if (strstr(text, "empty response"))
        return "empty_response";
    /* invalid_config */
    {
        const char *needle[] = {"config","missing","invalid",NULL};
        for (int i = 0; needle[i]; i++) if (strstr(text, needle[i])) return "invalid_config";
    }
    return "unknown";
}

/* ── _parse_time ─────────────────────────────────────────────────── */
/* PoP: _parse_time @ agent/monitoring/cron_health.py:_parse_time
 *
 * Parse an ISO-8601 timestamp to epoch seconds (fractional, double).
 * Returns -1.0 on failure (mirrors None).  Uses datetime_parse_iso8601 for
 * the integer-seconds component, then layers fractional-second parsing so
 * duration_ms retains sub-second precision like Python's total_seconds(). */
double cron_parse_time(const char *raw) {
    if (!raw || raw[0] == '\0') return -1.0;
    time_t base = datetime_parse_iso8601(raw);
    if (base == (time_t)-1) return -1.0;
    /* extract fractional seconds if a '.' or ',' precedes digits */
    const char *dot = strchr(raw, '.');
    if (!dot) dot = strchr(raw, ',');
    double frac = 0.0;
    if (dot) {
        double f = 0.0; double scale = 0.1;
        for (const char *p = dot + 1; *p >= '0' && *p <= '9'; p++) {
            f += (*p - '0') * scale; scale *= 0.1;
        }
        frac = f;
    }
    return (double)base + frac;
}

/* ── _duration_ms ────────────────────────────────────────────────── */
/* PoP: _duration_ms @ agent/monitoring/cron_health.py:_duration_ms */
int cron_duration_ms(const json_t *record) {
    if (!record || record->type != JSON_OBJECT) return -1;
    double start = -1.0;
    json_t *started = json_obj_get(record, "started_at");
    if (started) start = cron_parse_time(json_string_value(started));
    if (start < 0.0) {
        json_t *claimed = json_obj_get(record, "claimed_at");
        if (claimed) start = cron_parse_time(json_string_value(claimed));
    }
    json_t *finished = json_obj_get(record, "finished_at");
    double finish = -1.0;
    if (finished) finish = cron_parse_time(json_string_value(finished));
    if (start < 0.0 || finish < 0.0) return -1;
    double duration = (finish - start) * 1000.0;
    if (duration < 0.0) duration = 0.0;
    return (int)duration;
}

/* ── project_execution_event ─────────────────────────────────────── */
/* PoP: project_execution_event @ agent/monitoring/cron_health.py:project_execution_event */
struct cron_execution_event project_execution_event(const json_t *record,
                                                     const char *delivery_outcome) {
    struct cron_execution_event ev = {0};

    /* status */
    const char *status = "unknown";
    if (record && record->type == JSON_OBJECT) {
        json_t *st = json_obj_get(record, "status");
        if (st && st->type == JSON_STRING) {
            /* lowercased, validated against known set */
            char buf[64]; size_t n = strlen(st->str_val);
            if (n > sizeof(buf)-1) n = sizeof(buf)-1;
            for (size_t i = 0; i < n; i++) buf[i] = (char)tolower((unsigned char)st->str_val[i]);
            buf[n] = '\0';
            const char *known[] = {"claimed","running","completed","failed","unknown",NULL};
            bool matched = false;
            for (int i = 0; known[i]; i++) if (strcmp(buf, known[i])==0) { matched = true; break; }
            status = matched ? buf : "unknown";
        }
    }
    ev.status = strdup(status);

    /* job_key */
    const char *job_id = NULL;
    if (record && record->type == JSON_OBJECT) {
        json_t *jid = json_obj_get(record, "job_id");
        if (jid) job_id = json_string_value(jid);
    }
    ev.job_key = cron_job_key(job_id);

    /* source */
    const char *source = "unknown";
    if (record && record->type == JSON_OBJECT) {
        json_t *src = json_obj_get(record, "source");
        if (src && src->type == JSON_STRING) {
            char buf[64]; size_t n = strlen(src->str_val);
            if (n > sizeof(buf)-1) n = sizeof(buf)-1;
            for (size_t i = 0; i < n; i++) buf[i] = (char)tolower((unsigned char)src->str_val[i]);
            buf[n] = '\0';
            const char *known[] = {"builtin","direct","external",NULL};
            bool matched = false;
            for (int i = 0; known[i]; i++) if (strcmp(buf, known[i])==0) { matched = true; break; }
            source = (matched || strcmp(buf,"unknown")==0) ? buf : "external";
            /* Python: if source not in known AND source != "unknown": source = "external" */
            /* if it IS "unknown" keep "unknown"; if known keep; else external */
        }
    }
    ev.source = strdup(source);

    ev.duration_ms = cron_duration_ms(record);

    /* delivery_outcome */
    if (delivery_outcome) {
        char buf[64]; size_t n = strlen(delivery_outcome);
        if (n > sizeof(buf)-1) n = sizeof(buf)-1;
        for (size_t i = 0; i < n; i++) buf[i] = (char)tolower((unsigned char)delivery_outcome[i]);
        buf[n] = '\0';
        const char *known[] = {"delivered","failed","suppressed","not_configured",NULL};
        bool matched = false;
        for (int i = 0; known[i]; i++) if (strcmp(buf, known[i])==0) { matched = true; break; }
        ev.delivery_outcome = matched ? strdup(buf) : NULL;
    } else {
        ev.delivery_outcome = NULL;
    }

    /* error_class — only when status in {failed, unknown} */
    const char *errstr = NULL;
    if (record && record->type == JSON_OBJECT) {
        json_t *er = json_obj_get(record, "error");
        if (er) errstr = json_string_value(er);
    }
    if (strcmp(ev.status, "failed") == 0 || strcmp(ev.status, "unknown") == 0)
        ev.error_class = strdup(classify_cron_error(errstr));
    else
        ev.error_class = NULL;  /* None → NULL (non-failed/unknown status) */

    ev.ts_ns = cron_now_ns();
    return ev;
}

void cron_execution_event_free(struct cron_execution_event *ev) {
    if (!ev) return;
    free(ev->status); free(ev->job_key); free(ev->source);
    free(ev->delivery_outcome); free(ev->error_class);
    memset(ev, 0, sizeof(*ev));
}
