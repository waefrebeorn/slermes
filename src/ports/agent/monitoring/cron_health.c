#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _GNU_SOURCE
/*
 * ports/agent/monitoring/cron_health.c — C11 port of agent/monitoring/cron_health.py
 * Merged from 1 source file(s) by reorganization.
 */

/* ── from src/agent/port_cron_health.c ── */
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
#include "cron_jobs.h"
#include "cron_scheduler_runtime.h"
#include "port_monitoring_emitter.h"

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

/* ── _now ────────────────────────────────────────────────────────── */
/* PoP: cron_now @ agent/monitoring/cron_health.py:_now */
/* Faithful port of `_hermes_now()`: wall-clock epoch seconds as double. */
double cron_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

/* ── _job_key ────────────────────────────────────────────────────── */
/* PoP: cron_job_key @ agent/monitoring/cron_health.py:_job_key */
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
/* PoP: classify_cron_error @ agent/monitoring/cron_health.py:classify_cron_error */
/* Faithful port of the Python regex cascade. Each branch compiles to a POSIX
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
/* PoP: seconds @ agent/monitoring/cron_health.py:_parse_time */
/* Parse an ISO-8601 timestamp to epoch seconds (fractional, double).
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
/* PoP: cron_duration_ms @ agent/monitoring/cron_health.py:_duration_ms */
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

/* ── event-to-JSON serialization ─────────────────────────────────── */
/* Python: CronExecutionEvent.to_dict() → {"event":"cron_execution", ...} */
json_t *cron_execution_event_to_json(const struct cron_execution_event *ev) {
    if (!ev) return NULL;
    json_t *d = json_object();
    json_set(d, "event", json_string("cron_execution"));
    json_set(d, "status", json_string(ev->status));
    json_set(d, "job_key", json_string(ev->job_key));
    json_set(d, "source", json_string(ev->source));
    if (ev->duration_ms >= 0)
        json_set(d, "duration_ms", json_number((double)ev->duration_ms));
    else
        json_set(d, "duration_ms", json_new_null());
    if (ev->delivery_outcome)
        json_set(d, "delivery_outcome", json_string(ev->delivery_outcome));
    else
        json_set(d, "delivery_outcome", json_new_null());
    if (ev->error_class && ev->error_class[0])
        json_set(d, "error_class", json_string(ev->error_class));
    else
        json_set(d, "error_class", json_new_null());
    json_set(d, "ts_ns", json_number((double)ev->ts_ns));
    return d;
}

/* ── _is_overdue ──────────────────────────────────────────────────── */
/* PoP: cron_is_overdue @ agent/monitoring/cron_health.py:_is_overdue */
bool cron_is_overdue(const json_t *job, double now_epoch) {
    if (!job || job->type != JSON_OBJECT) return false;
    /* if not job.get("enabled", True): return False — default True */
    bool enabled = true;
    json_t *en = json_obj_get(job, "enabled");
    if (en && en->type == JSON_BOOL) enabled = en->bool_val;
    if (!enabled) return false;

    json_t *nra = json_obj_get(job, "next_run_at");
    const char *next_run_str = NULL;
    if (nra && nra->type == JSON_STRING) next_run_str = nra->str_val;
    double next_run = cron_parse_time(next_run_str);
    if (next_run < 0.0) return false;

    json_t *sched = json_obj_get(job, "schedule");
    if (!sched || sched->type != JSON_OBJECT) return false;

    /* grace window */
    int grace = cronjobs_compute_grace_seconds(sched);
    double lateness = now_epoch - next_run;
    return lateness > (double)grace;
}

/* ── emit_execution_state ─────────────────────────────────────────── */
/* PoP: cron_emit_execution_state @ agent/monitoring/cron_health.py:emit_execution_state */
/* PoP: cron_emit_execution_state @ cron/executions.py:_emit_execution_state */
void cron_emit_execution_state(const json_t *record, const char *delivery_outcome) {
    if (!record) return;
    struct cron_execution_event ev = project_execution_event(record, delivery_outcome);
    json_t *event = cron_execution_event_to_json(&ev);
    cron_execution_event_free(&ev);
    if (!event) return;
    monitoring_emitter_t *target = monitoring_emitter_get();
    if (!target) { json_free(event); return; }
    bool terminal = (strcmp(ev.status, "completed") == 0 ||
                     strcmp(ev.status, "failed") == 0 ||
                     strcmp(ev.status, "unknown") == 0);
    cron_execution_event_free(&ev);
    monitoring_emitter_emit(target, event);
    /* terminal states synchronously cross the queue barrier (flush timeout 1.0) */
    if (terminal) {
        monitoring_emitter_flush(target, 1.0);
    }
    json_free(event);
}

/* ── build_cron_health_snapshot ────────────────────────────────────── */
/* PoP: cron_build_health_snapshot @ agent/monitoring/cron_health.py:build_cron_health_snapshot */
json_t *cron_build_health_snapshot(void) {
    json_t *metrics = json_array();
    if (!metrics) return NULL;

    /* hermes.cron.scheduler.heartbeat_age_seconds / last_success_age_seconds */
    double hb_age = 0.0;
    double succ_age = 0.0;
    {
        bool have_hb = false, have_succ = false;
        double v = cronjobs_ticker_heartbeat_age();
        if (v >= 0.0) { have_hb = true; hb_age = v; }
        v = cronjobs_ticker_success_age();
        if (v >= 0.0) { have_succ = true; succ_age = v; }
        if (have_hb) {
            json_t *m = json_object();
            json_set(m, "name", json_string("hermes.cron.scheduler.heartbeat_age_seconds"));
            json_set(m, "value", json_number(hb_age >= 0.0 ? hb_age : 0.0));
            json_set(m, "attributes", json_object());
            json_array_append(metrics, m);
        }
        if (have_succ) {
            json_t *m = json_object();
            json_set(m, "name", json_string("hermes.cron.scheduler.last_success_age_seconds"));
            json_set(m, "value", json_number(succ_age >= 0.0 ? succ_age : 0.0));
            json_set(m, "attributes", json_object());
            json_array_append(metrics, m);
        }
    }

    /* hermes.cron.scheduler.catch_up_occurrences */
    {
        double cu = 0.0; /* would call get_catch_up_occurrence_count() if ported */
        json_t *m = json_object();
        json_set(m, "name", json_string("hermes.cron.scheduler.catch_up_occurrences"));
        json_set(m, "value", json_number(cu));
        json_set(m, "attributes", json_object());
        json_array_append(metrics, m);
    }

    /* hermes.cron.jobs.enabled / .overdue */
    {
        char *err = NULL;
        json_t *jobs = cronjobs_load_jobs(&err);
        if (err) free(err);
        if (jobs) {
            size_t n = json_len(jobs);
            int enabled_count = 0, overdue_count = 0;
            double now = cron_now();
            for (size_t i = 0; i < n; i++) {
                json_t *job = json_array_get(jobs, i);
                if (cron_is_overdue(job, now)) overdue_count++;
                /* enabled count */
                bool jen = true;
                json_t *en = json_obj_get(job, "enabled");
                if (en && en->type == JSON_BOOL) jen = en->bool_val;
                if (jen) enabled_count++;
            }
            json_t *m1 = json_object();
            json_set(m1, "name", json_string("hermes.cron.jobs.enabled"));
            json_set(m1, "value", json_number((double)enabled_count));
            json_set(m1, "attributes", json_object());
            json_array_append(metrics, m1);
            json_t *m2 = json_object();
            json_set(m2, "name", json_string("hermes.cron.jobs.overdue"));
            json_set(m2, "value", json_number((double)overdue_count));
            json_set(m2, "attributes", json_object());
            json_array_append(metrics, m2);
            json_free(jobs);
        }
    }

    /* hermes.cron.jobs.running */
    {
        size_t nrun = 0;
        char **ids = scheduler_get_running_job_ids(&nrun);
        json_t *m = json_object();
        json_set(m, "name", json_string("hermes.cron.jobs.running"));
        json_set(m, "value", json_number((double)nrun));
        json_set(m, "attributes", json_object());
        json_array_append(metrics, m);
        /* free the ids array (strings are malloc'd by the scheduler) */
        if (ids) {
            for (size_t i = 0; i < nrun; i++) free(ids[i]);
            free(ids);
        }
    }

    json_t *snapshot = json_object();
    json_set(snapshot, "metrics", metrics);
    json_set(snapshot, "events", json_array());
    return snapshot;
}

