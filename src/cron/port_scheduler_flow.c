/* port_scheduler_flow.c — FLOW-core port of cron/scheduler.py.
 *
 * Ports the tick/dispatch ORCHESTRATION: due-job claim ordering, tick-lock
 * acquire/release, dead-owner reap throttle, stale-inflight sweep gating,
 * dispatch guard (shutdown / dedup / execution-creation / submit branches),
 * fire-claim heartbeat ownership, delivery composition and crash-failure
 * bookkeeping. All I/O (clock, file locks, store CAS, ledger writes, env,
 * config, logging) is INJECTED through the sf_flow_io_t seam dispatcher; the
 * decision logic around each seam matches the live Python exactly (early
 * returns, exception->fallback paths, mutation order).
 *
 * Seam model (mirrors context_compressor_flow): every live module-level call
 * or `from x import y; y(...)` the ported flow makes goes through
 * sf_flow_io_t.call(io, name, args). The dispatcher appends `name` to
 * io->trace (ordered decision sequence), returns the scripted result, applies
 * any "state" effects onto io->state, and models a raised exception with
 * {"__raise__": "<Type>", "message": "...", "errno": "<NAME>"}. io->state is a
 * json_t OBJECT mirroring the module's mutable globals (_running_job_ids,
 * _last_dead_owner_reap_at, _parallel_pools, ...). Errnos are carried as
 * SYMBOLIC names ("EMFILE", "EACCES", ...) so the C and the live Python agree
 * regardless of platform errno numbering.
 *
 * Ownership: seam results returned by call are owned by the caller (json_free).
 * io->trace and io->state are owned by the harness. Argument json_t* passed to
 * call are borrowed (the dispatcher copies what it records); the static seam()
 * wrapper frees them.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <math.h>
#include "hermes_json.h"

/* ── live module constants (byte-pinned to cron/scheduler.py) ──────────── */
#define SF_DEAD_OWNER_REAP_INTERVAL_SECONDS 300.0
#define SF_INFLIGHT_MIN_ALLOWANCE_MINUTES 30.0
#define SF_FORCED_RELEASE_HISTORY 20
#define SF_RUN_CLAIM_HEARTBEAT_SECONDS 60.0
#define SF_FIRE_CLAIM_HEARTBEAT_GRACE_SECONDS (SF_RUN_CLAIM_HEARTBEAT_SECONDS * 3)
#define SF_FIRE_CLAIM_MISS_CONFIRM_SECONDS 1.0
#define SF_DEFAULT_FAILURE_REPEAT_ALERT_HOURS 6.0
#define SF_SILENT_MARKER "[SILENT]"
#define SF_CRON_FAILURE_MARKER "[CRON_FAILURE]"
#define SF_OWNERSHIP_LOST_INTERRUPTED "Interrupted by shutdown before terminal completion."

/* ── generic seam dispatcher + trace + attribute state ─────────────────── */
typedef struct sf_flow_io_t {
    json_t *(*call)(struct sf_flow_io_t *io, const char *name, json_t *args);
    json_t *trace;   /* JSON array of strings: ordered decision/action sequence */
    json_t *state;   /* JSON object: module mutable globals */
    void   *ctx;     /* harness backend (scripted results) */
} sf_flow_io_t;

/* PoP: sf_flow_trace @ cron/scheduler.py:<seam-trace helper> */
void sf_flow_trace(sf_flow_io_t *io, const char *token) {
    if (!io || !io->trace || io->trace->type != JSON_ARRAY) return;
    json_append(io->trace, json_string(token));
}

/* PoP: sf_flow_call @ cron/scheduler.py:<seam dispatcher> */
json_t *sf_flow_call(sf_flow_io_t *io, const char *name, json_t *args) {
    if (io && io->call) return io->call(io, name, args);
    return json_null();
}

/* PoP: sf_flow_raised @ cron/scheduler.py:<exception sentinel> */
bool sf_flow_raised(const json_t *result) {
    return result && result->type == JSON_OBJECT && json_obj_get(result, "__raise__") != NULL;
}

/* Call a seam, record its name in the trace, free args, return owned result. */
static json_t *seam(sf_flow_io_t *io, const char *name, json_t *args) {
    sf_flow_trace(io, name);
    json_t *r = sf_flow_call(io, name, args);
    if (args) json_free(args);
    return r ? r : json_null();
}

/* ── Python-faithful helpers ───────────────────────────────────────────── */
static bool sf_truthy(const json_t *v) {
    if (!v) return false;
    switch (v->type) {
        case JSON_NULL: return false;
        case JSON_BOOL: return v->bool_val;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count > 0;
    }
    return false;
}

static int sf_is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* Python str.strip() (ASCII whitespace). Caller frees. */
static char *sf_strip(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p), a = 0, b = n;
    while (a < b && sf_is_ws((unsigned char)p[a])) a++;
    while (b > a && sf_is_ws((unsigned char)p[b - 1])) b--;
    char *out = malloc(b - a + 1);
    if (!out) return NULL;
    memcpy(out, p + a, b - a);
    out[b - a] = '\0';
    return out;
}

static char *sf_lower(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    for (size_t i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)p[i]);
    out[n] = '\0';
    return out;
}

static bool sf_startswith(const char *s, const char *pre) {
    if (!s || !pre) return false;
    return strncmp(s, pre, strlen(pre)) == 0;
}

/* str(v) for a json scalar (Python str of None/bool/number/string). Caller frees. */
static char *sf_pystr(const json_t *v) {
    if (!v || v->type == JSON_NULL) return strdup("None");
    if (v->type == JSON_BOOL) return strdup(v->bool_val ? "True" : "False");
    if (v->type == JSON_STRING) return strdup(v->str_val ? v->str_val : "");
    if (v->type == JSON_NUMBER) {
        char buf[64];
        double d = v->num_val;
        if (d == floor(d) && fabs(d) < 1e15) snprintf(buf, sizeof buf, "%.0f", d);
        else snprintf(buf, sizeof buf, "%g", d);
        return strdup(buf);
    }
    char *ser = json_serialize(v);
    return ser ? ser : strdup("");
}

/* dict.get(key) borrowed; NULL when absent. */
static json_t *sf_get(const json_t *obj, const char *key) {
    if (!obj || obj->type != JSON_OBJECT) return NULL;
    return json_obj_get(obj, key);
}

static const char *sf_get_str(const json_t *obj, const char *key, const char *def) {
    json_t *v = sf_get(obj, key);
    if (v && v->type == JSON_STRING) return v->str_val ? v->str_val : "";
    return def;
}

/* Python `x or default` for string-valued dict reads. */
static const char *sf_str_or(const json_t *obj, const char *key, const char *def) {
    json_t *v = sf_get(obj, key);
    if (sf_truthy(v) && v->type == JSON_STRING) return v->str_val;
    return def;
}

/* ── ISO-8601 -> epoch seconds (aware strings; deterministic subset) ───── */
static int64_t sf_days_from_civil(int64_t y, int m, int d) {
    y -= m <= 2;
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    int64_t yoe = y - era * 400;
    int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

static bool sf_read_digits(const char **p, int n, int *val) {
    int v = 0;
    for (int i = 0; i < n; i++) {
        char c = (*p)[i];
        if (c < '0' || c > '9') return false;
        v = v * 10 + (c - '0');
    }
    *p += n;
    *val = v;
    return true;
}

/* Parse YYYY-MM-DD[T ]HH:MM:SS[.frac][Z|±HH:MM] -> epoch seconds (double).
 * Returns false on malformed input. Mirrors datetime.fromisoformat(...).timestamp()
 * for the aware subset the harness feeds. */
static bool sf_parse_iso_epoch(const char *s, double *out) {
    if (!s) return false;
    const char *p = s;
    while (sf_is_ws((unsigned char)*p)) p++;
    int y, mo, d, h, mi, sec;
    if (!sf_read_digits(&p, 4, &y)) return false;
    if (*p != '-') return false; p++;
    if (!sf_read_digits(&p, 2, &mo)) return false;
    if (*p != '-') return false; p++;
    if (!sf_read_digits(&p, 2, &d)) return false;
    if (*p == 'T' || *p == 't' || *p == ' ') p++;
    else return false;
    if (!sf_read_digits(&p, 2, &h)) return false;
    if (*p != ':') return false; p++;
    if (!sf_read_digits(&p, 2, &mi)) return false;
    if (*p != ':') return false; p++;
    if (!sf_read_digits(&p, 2, &sec)) return false;
    double frac = 0.0;
    if (*p == '.') {
        p++;
        double scale = 0.1;
        int digits = 0;
        while (*p >= '0' && *p <= '9') {
            if (digits < 6) { frac += (*p - '0') * scale; scale /= 10.0; }
            digits++; p++;
        }
        if (digits == 0) return false;
    }
    int off_sec = 0;
    if (*p == 'Z' || *p == 'z') { p++; }
    else if (*p == '+' || *p == '-') {
        int sign = (*p == '-') ? -1 : 1;
        p++;
        int oh, om = 0;
        if (!sf_read_digits(&p, 2, &oh)) return false;
        if (*p == ':') p++;
        if (*p >= '0' && *p <= '9') { if (!sf_read_digits(&p, 2, &om)) return false; }
        off_sec = sign * (oh * 3600 + om * 60);
    }
    while (sf_is_ws((unsigned char)*p)) p++;
    if (*p != '\0') return false;
    int64_t days = sf_days_from_civil(y, mo, d);
    int64_t secs = days * 86400 + h * 3600 + mi * 60 + sec - off_sec;
    *out = (double)secs + frac;
    return true;
}

/* ── state accessors ───────────────────────────────────────────────────── */
static json_t *sf_state_get(sf_flow_io_t *io, const char *key) {
    return sf_get(io->state, key);
}
static void sf_state_set(sf_flow_io_t *io, const char *key, json_t *val) {
    if (io && io->state && io->state->type == JSON_OBJECT) json_set(io->state, key, val);
    else if (val) json_free(val);
}

/* logger.<level> seam (recorded; live logger is a recording stub in the oracle). */
static void sf_log(sf_flow_io_t *io, const char *level) {
    char name[32];
    snprintf(name, sizeof name, "logger.%s", level);
    json_t *r = seam(io, name, NULL);
    json_free(r);
}
/* ── pure helpers ──────────────────────────────────────────────────────── */

/* PoP: sf_job_route_pinned @ cron/scheduler.py:_job_route_pinned */
bool sf_job_route_pinned(const json_t *job) {
    static const char *keys[] = { "provider", "model", "base_url" };
    for (int i = 0; i < 3; i++) {
        json_t *v = sf_get(job, keys[i]);
        if (v && v->type == JSON_STRING) {
            char *t = sf_strip(v->str_val);
            bool nonempty = t && t[0] != '\0';
            free(t);
            if (nonempty) return true;
        }
    }
    return false;
}

/* PoP: sf_normalize_newlines @ cron/scheduler.py:_normalize_newlines */
char *sf_normalize_newlines(const char *text) {
    const char *p = text ? text : "";
    size_t n = strlen(p);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < n; i++) {
        if (p[i] == '\r') {
            out[j++] = '\n';
            if (i + 1 < n && p[i + 1] == '\n') i++;
        } else {
            out[j++] = p[i];
        }
    }
    out[j] = '\0';
    return out;
}

/* Python str.splitlines() over the ASCII line-boundary subset (\n \r \r\n \v \f).
 * Returns a json array of strings (caller frees). */
static json_t *sf_splitlines(const char *text) {
    json_t *arr = json_array();
    const char *p = text ? text : "";
    const char *start = p;
    while (*p) {
        char c = *p;
        if (c == '\n' || c == '\r' || c == '\v' || c == '\f') {
            size_t len = (size_t)(p - start);
            char *line = malloc(len + 1);
            memcpy(line, start, len);
            line[len] = '\0';
            json_append(arr, json_string(line));
            free(line);
            if (c == '\r' && p[1] == '\n') p++;
            p++;
            start = p;
        } else {
            p++;
        }
    }
    if (p != start) {
        json_append(arr, json_string(start));
    }
    return arr;
}

/* Python str.rstrip() (ASCII whitespace). Caller frees. */
static char *sf_rstrip(const char *s) {
    const char *p = s ? s : "";
    size_t n = strlen(p);
    while (n && sf_is_ws((unsigned char)p[n - 1])) n--;
    char *out = malloc(n + 1);
    memcpy(out, p, n);
    out[n] = '\0';
    return out;
}

/* PoP: sf_cron_failure_marker_error @ cron/scheduler.py:_cron_failure_marker_error */
char *sf_cron_failure_marker_error(const char *text) {
    json_t *lines = sf_splitlines(text ? text : "");
    size_t n = json_len(lines);
    if (n == 0) { json_free(lines); return NULL; }
    const char *first = json_get(lines, 0)->str_val;
    char *rf = sf_rstrip(first);
    bool match = strcmp(rf, SF_CRON_FAILURE_MARKER) == 0;
    free(rf);
    if (!match) { json_free(lines); return NULL; }
    /* evidence = "\n".join(lines[1:]).strip() */
    size_t cap = 1;
    for (size_t i = 1; i < n; i++) cap += strlen(json_get(lines, i)->str_val) + 1;
    char *joined = malloc(cap);
    joined[0] = '\0';
    for (size_t i = 1; i < n; i++) {
        if (i > 1) strcat(joined, "\n");
        strcat(joined, json_get(lines, i)->str_val);
    }
    json_free(lines);
    char *ev = sf_strip(joined);
    free(joined);
    if (ev && ev[0] != '\0') return ev;
    free(ev);
    return strdup("Cron agent reported failure.");
}

/* PoP: sf_apply_agent_failure_marker @ cron/scheduler.py:_apply_agent_failure_marker */
json_t *sf_apply_agent_failure_marker(const json_t *job, bool success, const json_t *error,
                                      const char *final_response) {
    json_t *out = json_array();
    if (success && !sf_truthy(sf_get(job, "no_agent"))) {
        char *marker = sf_cron_failure_marker_error(final_response);
        if (marker) {
            json_append(out, json_bool(false));
            json_append(out, json_string(marker));
            json_append(out, json_bool(true));
            free(marker);
            return out;
        }
    }
    json_append(out, json_bool(success));
    json_append(out, error ? json_copy(error) : json_null());
    json_append(out, json_bool(false));
    return out;
}

/* PoP: sf_is_lock_contention_errno @ cron/scheduler.py:_is_lock_contention_errno */
bool sf_is_lock_contention_errno(const char *errno_name, const char *platform) {
    if (!errno_name || !*errno_name) return false;  /* err.errno is None */
    if (strcmp(platform, "fcntl") == 0) {
        return strcmp(errno_name, "EWOULDBLOCK") == 0 ||
               strcmp(errno_name, "EAGAIN") == 0 ||
               strcmp(errno_name, "EACCES") == 0;
    }
    if (strcmp(platform, "msvcrt") == 0) {
        return strcmp(errno_name, "EACCES") == 0 ||
               strcmp(errno_name, "EDEADLK") == 0;
    }
    return false;
}

/* PoP: sf_is_fd_exhaustion_text @ cron/scheduler.py:_is_fd_exhaustion_text */
bool sf_is_fd_exhaustion_text(const char *text) {
    char *low = sf_lower(text ? text : "");
    bool r = strstr(low, "too many open files") != NULL || strstr(low, "emfile") != NULL;
    free(low);
    return r;
}

/* PoP: sf_is_fd_exhaustion @ cron/scheduler.py:_is_fd_exhaustion */
bool sf_is_fd_exhaustion(const json_t *exc) {
    const char *type = sf_get_str(exc, "type", "");
    const char *en = sf_get_str(exc, "errno", "");
    if (strcmp(type, "OSError") == 0 &&
        (strcmp(en, "EMFILE") == 0 || strcmp(en, "ENFILE") == 0)) {
        return true;
    }
    const char *msg = sf_get_str(exc, "message", "");
    return sf_is_fd_exhaustion_text(msg);
}

/* PoP: sf_stale_code_yield_labels @ cron/scheduler.py:stale_code_yield_labels */
json_t *sf_stale_code_yield_labels(const char *recorded_error) {
    if (!recorded_error || !*recorded_error ||
        !sf_startswith(recorded_error, "CronTickYielded")) {
        return json_null();
    }
    /* _STALE_YIELD_RE = r"stale code: booted on (\S+), disk is at (\S+)\)" */
    const char *anchor = strstr(recorded_error, "stale code: booted on ");
    if (!anchor) return json_null();
    const char *p = anchor + strlen("stale code: booted on ");
    const char *g1 = p;
    while (*p && !sf_is_ws((unsigned char)*p)) p++;
    if (p == g1) return json_null();
    size_t g1len = (size_t)(p - g1);
    if (strncmp(p, ", disk is at ", 13) != 0) return json_null();
    p += 13;
    const char *g2 = p;
    while (*p && !sf_is_ws((unsigned char)*p)) p++;
    if (p == g2) return json_null();
    size_t g2len = (size_t)(p - g2);
    if (*p != ')') return json_null();
    char *b = malloc(g1len + 1); memcpy(b, g1, g1len); b[g1len] = '\0';
    char *d = malloc(g2len + 1); memcpy(d, g2, g2len); d[g2len] = '\0';
    json_t *out = json_array();
    json_append(out, json_string(b));
    json_append(out, json_string(d));
    free(b); free(d);
    return out;
}

/* PoP: sf_inflight_key @ cron/scheduler.py:_inflight_key */
json_t *sf_inflight_key(sf_flow_io_t *io, const char *job_id, const json_t *home) {
    json_t *key;
    if (home && home->type != JSON_NULL) {
        json_t *args = json_object();
        json_set(args, "home", json_copy(home));
        key = seam(io, "hermes_home_key", args);
    } else {
        json_t *h = seam(io, "_get_hermes_home", NULL);
        json_t *args = json_object();
        json_set(args, "home", h);
        key = seam(io, "hermes_home_key", args);
    }
    json_t *out = json_array();
    json_append(out, key);
    json_append(out, json_string(job_id ? job_id : ""));
    return out;
}

/* membership of a [home,job] key in a state array of such pairs. */
static bool sf_key_in_array(const json_t *arr, const json_t *key) {
    if (!arr || arr->type != JSON_ARRAY || !key || json_len(key) < 2) return false;
    const char *kh = json_get(key, 0)->str_val;
    const char *kj = json_get(key, 1)->str_val;
    for (size_t i = 0; i < json_len(arr); i++) {
        json_t *e = json_get(arr, i);
        if (!e || e->type != JSON_ARRAY || json_len(e) < 2) continue;
        json_t *h = json_get(e, 0);
        json_t *j = json_get(e, 1);
        if (h && j && h->type == JSON_STRING && j->type == JSON_STRING &&
            strcmp(h->str_val, kh ? kh : "") == 0 &&
            strcmp(j->str_val, kj ? kj : "") == 0) {
            return true;
        }
    }
    return false;
}

/* PoP: sf_is_job_running @ cron/scheduler.py:is_job_running */
bool sf_is_job_running(sf_flow_io_t *io, const char *job_id, const json_t *home) {
    json_t *key = sf_inflight_key(io, job_id, home);
    bool r = sf_key_in_array(sf_state_get(io, "_running_job_ids"), key) ||
             sf_key_in_array(sf_state_get(io, "_running_fire_owners"), key);
    json_free(key);
    return r;
}

/* PoP: sf_job_interval_minutes @ cron/scheduler.py:_job_interval_minutes */
json_t *sf_job_interval_minutes(sf_flow_io_t *io, const json_t *job) {
    json_t *schedule = sf_get(job, "schedule");
    json_t *owned = NULL;
    if (schedule && schedule->type == JSON_STRING) {
        char *t = sf_strip(schedule->str_val);
        bool nonempty = t && t[0] != '\0';
        free(t);
        if (nonempty) {
            json_t *args = json_object();
            json_set(args, "schedule", json_copy(schedule));
            json_t *parsed = seam(io, "parse_schedule", args);
            /* parse_schedule(...) or {} */
            if (!sf_truthy(parsed)) { json_free(parsed); parsed = json_object(); }
            owned = parsed;
            schedule = parsed;
        }
    }
    json_t *ret = json_null();
    if (schedule && schedule->type == JSON_OBJECT) {
        const char *kind = sf_get_str(schedule, "kind", "");
        if (strcmp(kind, "interval") == 0) {
            json_t *minutes = sf_get(schedule, "minutes");
            if (sf_truthy(minutes)) {
                double m = minutes->type == JSON_NUMBER ? minutes->num_val : 0.0;
                ret = json_number(m);
            }
        } else if (strcmp(kind, "cron") == 0) {
            json_t *expr = sf_get(schedule, "expr");
            char *es = sf_pystr(sf_truthy(expr) ? expr : NULL);
            /* str(schedule.get("expr") or "") */
            json_t *args = json_object();
            json_set(args, "expr", json_string(sf_truthy(expr) ? (expr->type == JSON_STRING ? expr->str_val : es) : ""));
            free(es);
            ret = seam(io, "_cron_interval_minutes", args);
        }
    }
    if (owned) json_free(owned);
    return ret;
}

/* PoP: sf_failure_repeat_alert_hours @ cron/scheduler.py:_failure_repeat_alert_hours */
double sf_failure_repeat_alert_hours(sf_flow_io_t *io) {
    json_t *args = json_object();
    json_set(args, "key", json_string("failure_repeat_alert_hours"));
    json_set(args, "default", json_number(SF_DEFAULT_FAILURE_REPEAT_ALERT_HOURS));
    json_set(args, "cast", json_string("float"));
    json_t *r = seam(io, "_cron_config_number", args);
    double v = (r && r->type == JSON_NUMBER) ? r->num_val : SF_DEFAULT_FAILURE_REPEAT_ALERT_HOURS;
    json_free(r);
    return v;
}

/* PoP: sf_repeat_alert_withheld @ cron/scheduler.py:_repeat_alert_withheld */
bool sf_repeat_alert_withheld(sf_flow_io_t *io, const json_t *incident) {
    double hours = sf_failure_repeat_alert_hours(io);
    if (hours <= 0) return false;
    json_t *alerted_at = sf_get(incident, "alerted_at");
    if (!sf_truthy(alerted_at)) return false;
    char *as = sf_pystr(alerted_at);
    double last;
    bool ok = sf_parse_iso_epoch(as, &last);
    free(as);
    if (!ok) return false;  /* TypeError/ValueError -> False */
    json_t *now = seam(io, "_hermes_now", NULL);
    char *ns = sf_pystr(now);
    json_free(now);
    double now_e;
    ok = sf_parse_iso_epoch(ns, &now_e);
    free(ns);
    if (!ok) return false;
    return (now_e - last) < hours * 3600.0;
}

/* PoP: sf_failure_streak_nudge @ cron/scheduler.py:_failure_streak_nudge */
char *sf_failure_streak_nudge(sf_flow_io_t *io, const json_t *job) {
    json_t *schedule = sf_get(job, "schedule");
    json_t *sched_obj = (schedule && schedule->type == JSON_OBJECT) ? schedule : NULL;
    const char *kind = sf_get_str(sched_obj, "kind", "");
    if (strcmp(kind, "cron") != 0 && strcmp(kind, "interval") != 0) return strdup("");
    long threshold = 3;
    json_t *cfg = seam(io, "load_config", NULL);
    if (sf_flow_raised(cfg)) {
        json_free(cfg);
        threshold = 3;
    } else {
        json_t *c = sf_truthy(cfg) ? cfg : NULL;
        json_t *cron_cfg = sf_get(c, "cron");
        json_t *cc = sf_truthy(cron_cfg) ? cron_cfg : NULL;
        json_t *tv = sf_get(cc, "failure_nudge_threshold");
        if (tv) {
            if (tv->type == JSON_NUMBER) threshold = (long)tv->num_val;
            else if (tv->type == JSON_STRING) threshold = strtol(tv->str_val, NULL, 10);
        }
        json_free(cfg);
    }
    if (threshold <= 0) return strdup("");
    json_t *fs = sf_get(job, "failure_streak");
    long streak = (sf_truthy(fs) && fs->type == JSON_NUMBER ? (long)fs->num_val : 0) + 1;
    if (streak < threshold) return strdup("");
    const char *job_ref = sf_str_or(job, "name", NULL);
    if (!job_ref) job_ref = sf_str_or(job, "id", NULL);
    if (!job_ref) job_ref = "this job";
    char *out = NULL;
    if (asprintf(&out,
            "\nThis job has failed %ld runs in a row \xe2\x80\x94 worth a review. "
            "Fix its prompt/config, or pause it with `hermes cron pause %s` "
            "(resume/remove also available) to stop the noise.",
            streak, job_ref) < 0) {
        return strdup("");
    }
    return out;
}

/* PoP: sf_inflight_min_allowance_minutes @ cron/scheduler.py:_inflight_min_allowance_minutes */
double sf_inflight_min_allowance_minutes(sf_flow_io_t *io) {
    json_t *cfg = seam(io, "load_config", NULL);
    if (!sf_flow_raised(cfg)) {
        json_t *c = sf_truthy(cfg) ? cfg : NULL;
        json_t *cron_cfg = sf_get(c, "cron");
        json_t *cc = (cron_cfg && cron_cfg->type == JSON_OBJECT) ? cron_cfg : NULL;
        json_t *cv = sf_get(cc, "inflight_max_minutes");
        if (cv && cv->type != JSON_NULL) {
            double val = cv->type == JSON_NUMBER ? cv->num_val : strtod(sf_get_str(cc, "inflight_max_minutes", "0"), NULL);
            if (val > 0) { json_free(cfg); return val; }
        }
    }
    json_free(cfg);
    json_t *args = json_object();
    json_set(args, "name", json_string("HERMES_CRON_INFLIGHT_MAX_MINUTES"));
    json_t *rawj = seam(io, "cron_env_setting", args);
    char *raw = sf_strip(rawj && rawj->type == JSON_STRING ? rawj->str_val : "");
    json_free(rawj);
    if (raw && raw[0]) {
        char *end = NULL;
        double val = strtod(raw, &end);
        if (end != raw && *end == '\0') {
            if (val > 0) { free(raw); return val; }
        } else {
            sf_log(io, "warning");
        }
    }
    free(raw);
    return SF_INFLIGHT_MIN_ALLOWANCE_MINUTES;
}

/* PoP: sf_cron_cleanup_timeout_seconds @ cron/scheduler.py:_cron_cleanup_timeout_seconds */
double sf_cron_cleanup_timeout_seconds(sf_flow_io_t *io) {
    double def = 10.0;
    json_t *cfg = seam(io, "load_config", NULL);
    if (sf_flow_raised(cfg)) {
        sf_log(io, "debug");
        json_free(cfg);
        return def;
    }
    json_t *c = sf_truthy(cfg) ? cfg : NULL;
    json_t *cron_cfg = sf_get(c, "cron");
    json_t *cc = (cron_cfg && cron_cfg->type == JSON_OBJECT) ? cron_cfg : NULL;
    json_t *configured = sf_get(cc, "cleanup_timeout_seconds");
    double ret = def;
    if (configured && configured->type != JSON_NULL) {
        double timeout = configured->type == JSON_NUMBER ? configured->num_val
                       : strtod(sf_get_str(cc, "cleanup_timeout_seconds", "0"), NULL);
        if (timeout >= 0) ret = timeout;
    }
    json_free(cfg);
    return ret;
}

/* PoP: sf_resolve_job_workdir @ cron/scheduler.py:_resolve_job_workdir */
char *sf_resolve_job_workdir(sf_flow_io_t *io, const json_t *job, const char *job_id) {
    json_t *w = sf_get(job, "workdir");
    char *raw = sf_strip(sf_truthy(w) && w->type == JSON_STRING ? w->str_val : "");
    char *workdir = (raw && raw[0]) ? raw : NULL;
    if (!workdir) { free(raw); return NULL; }
    json_t *args = json_object();
    json_set(args, "path", json_string(workdir));
    json_t *isdir = seam(io, "is_dir", args);
    bool dir = sf_truthy(isdir);
    json_free(isdir);
    if (!dir) {
        sf_log(io, "warning");
        free(workdir);
        return NULL;
    }
    return workdir;
}

/* PoP: sf_resolve_job_reasoning_config @ cron/scheduler.py:_resolve_job_reasoning_config */
json_t *sf_resolve_job_reasoning_config(sf_flow_io_t *io, const json_t *job,
                                        const json_t *cfg, const char *model) {
    json_t *pinned = sf_get(job, "reasoning_effort");
    if (pinned && pinned->type != JSON_NULL) {
        json_t *args = json_object();
        json_set(args, "value", json_copy(pinned));
        json_t *parsed = seam(io, "parse_reasoning_effort", args);
        if (parsed && parsed->type != JSON_NULL) {
            sf_log(io, "info");
            return parsed;
        }
        json_free(parsed);
        sf_log(io, "warning");
    }
    json_t *args = json_object();
    json_set(args, "cfg", (cfg && cfg->type == JSON_OBJECT) ? json_copy(cfg) : json_object());
    json_set(args, "model", json_string(model ? model : ""));
    return seam(io, "resolve_reasoning_config", args);
}
/* ── flow: tick lock ───────────────────────────────────────────────────── */

static bool sf_errno_unwritable(const char *en) {
    return strcmp(en, "EROFS") == 0 || strcmp(en, "EACCES") == 0 ||
           strcmp(en, "EPERM") == 0 || strcmp(en, "ENOSPC") == 0 ||
           strcmp(en, "EDQUOT") == 0;
}

static const char *sf_platform(sf_flow_io_t *io) {
    const char *p = sf_get_str(io->state, "platform", "fcntl");
    return p;
}

/* PoP: sf_acquire_tick_lock @ cron/scheduler.py:_acquire_tick_lock */
json_t *sf_acquire_tick_lock(sf_flow_io_t *io, const char *lock_file) {
    const char *plat = sf_platform(io);
    json_t *lock_fd = json_null();
    json_t *args = json_object();
    json_set(args, "path", json_string(lock_file ? lock_file : ""));
    json_t *opened = seam(io, "open", args);
    if (sf_flow_raised(opened)) {
        /* open() raised: lock_fd stays None */
        goto except_path;
    }
    json_free(lock_fd);
    lock_fd = opened;  /* fd */
    if (strcmp(plat, "fcntl") == 0) {
        json_t *a = json_object();
        json_set(a, "fd", json_copy(lock_fd));
        json_set(a, "op", json_string("LOCK_EX_NB"));
        json_t *r = seam(io, "fcntl.flock", a);
        if (sf_flow_raised(r)) { opened = r; goto except_path; }
        json_free(r);
    } else if (strcmp(plat, "msvcrt") == 0) {
        json_t *a = json_object();
        json_set(a, "fd", json_copy(lock_fd));
        json_set(a, "mode", json_string("LK_NBLCK"));
        json_set(a, "nbytes", json_number(1));
        json_t *r = seam(io, "msvcrt.locking", a);
        if (sf_flow_raised(r)) { opened = r; goto except_path; }
        json_free(r);
    }
    return lock_fd;

except_path:
    /* `opened` holds the raised OSError descriptor here. */
    if (lock_fd && lock_fd->type != JSON_NULL) {
        json_t *a = json_object();
        json_set(a, "fd", json_copy(lock_fd));
        json_t *cr = seam(io, "close", a);  /* contextlib.suppress(OSError) */
        json_free(cr);
        const char *en = sf_get_str(opened, "errno", "");
        if (sf_is_lock_contention_errno(en, plat)) {
            sf_log(io, "info");
            json_free(opened);
            json_free(lock_fd);
            return json_null();
        }
    }
    if (sf_is_fd_exhaustion(opened)) {
        sf_log(io, "error");
    } else if (!sf_errno_unwritable(sf_get_str(opened, "errno", ""))) {
        sf_log(io, "error");
    }
    json_free(lock_fd);
    return opened;  /* re-raise */
}

/* PoP: sf_release_tick_lock @ cron/scheduler.py:_release_tick_lock */
json_t *sf_release_tick_lock(sf_flow_io_t *io, const json_t *lock_fd) {
    const char *plat = sf_platform(io);
    if (strcmp(plat, "fcntl") == 0) {
        json_t *a = json_object();
        json_set(a, "fd", json_copy(lock_fd));
        json_set(a, "op", json_string("LOCK_UN"));
        json_t *r = seam(io, "fcntl.flock", a);  /* suppressed */
        json_free(r);
    } else if (strcmp(plat, "msvcrt") == 0) {
        json_t *a = json_object();
        json_set(a, "fd", json_copy(lock_fd));
        json_set(a, "mode", json_string("LK_UNLCK"));
        json_set(a, "nbytes", json_number(1));
        json_t *r = seam(io, "msvcrt.locking", a);  /* suppressed */
        json_free(r);
    }
    json_t *a = json_object();
    json_set(a, "fd", json_copy(lock_fd));
    return seam(io, "close", a);  /* lock_fd.close() — not suppressed */
}

/* ── flow: dead-owner reap ─────────────────────────────────────────────── */

/* PoP: sf_maybe_reap_dead_owners @ cron/scheduler.py:_maybe_reap_dead_owners */
void sf_maybe_reap_dead_owners(sf_flow_io_t *io) {
    json_t *home = seam(io, "_get_hermes_home", NULL);
    json_t *ka = json_object();
    json_set(ka, "home", home);
    json_t *keyj = seam(io, "hermes_home_key", ka);
    char *reap_key = sf_pystr(keyj);
    json_free(keyj);
    json_t *nowj = seam(io, "time.monotonic", NULL);
    double reap_now = nowj && nowj->type == JSON_NUMBER ? nowj->num_val : 0.0;
    json_free(nowj);
    json_t *map = sf_state_get(io, "_last_dead_owner_reap_at");
    if (!map || map->type != JSON_OBJECT) {
        map = json_object();
        sf_state_set(io, "_last_dead_owner_reap_at", json_copy(map));
        json_free(map);
        map = sf_state_get(io, "_last_dead_owner_reap_at");
    }
    json_t *last = sf_get(map, reap_key);
    if (last && last->type != JSON_NULL &&
        reap_now - last->num_val < SF_DEAD_OWNER_REAP_INTERVAL_SECONDS) {
        free(reap_key);
        return;
    }
    json_set(map, reap_key, json_number(reap_now));
    free(reap_key);
    json_t *r = seam(io, "recover_interrupted_executions", NULL);
    if (sf_flow_raised(r)) {
        sf_log(io, "debug");
        json_free(r);
        return;
    }
    if (sf_truthy(r)) sf_log(io, "warning");
    json_free(r);
}

/* ── flow: stale-inflight sweep gate ───────────────────────────────────── */

/* PoP: sf_sweep_stale_inflight_for_tick @ cron/scheduler.py:_sweep_stale_inflight_for_tick */
void sf_sweep_stale_inflight_for_tick(sf_flow_io_t *io, const json_t *due_jobs) {
    json_t *home = seam(io, "_get_hermes_home", NULL);
    json_t *ka = json_object();
    json_set(ka, "home", home);
    json_t *keyj = seam(io, "hermes_home_key", ka);
    char *local_home = sf_pystr(keyj);
    json_free(keyj);
    json_t *running = sf_state_get(io, "_running_job_ids");
    bool any_local = false;
    if (running && running->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(running); i++) {
            json_t *e = json_get(running, i);
            if (e && e->type == JSON_ARRAY && json_len(e) >= 1) {
                json_t *h = json_get(e, 0);
                if (h && h->type == JSON_STRING && strcmp(h->str_val, local_home) == 0) {
                    any_local = true;
                    break;
                }
            }
        }
    }
    if (!any_local) { free(local_home); return; }
    /* contextlib.suppress(Exception): subset check + optional load_jobs */
    json_t *sweep_jobs = json_copy(due_jobs ? due_jobs : json_array());
    bool subset = true;
    if (running && running->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(running) && subset; i++) {
            json_t *e = json_get(running, i);
            if (!e || e->type != JSON_ARRAY || json_len(e) < 2) continue;
            json_t *h = json_get(e, 0);
            if (!h || h->type != JSON_STRING || strcmp(h->str_val, local_home) != 0) continue;
            const char *jid = json_get(e, 1)->str_val;
            bool found = false;
            if (due_jobs && due_jobs->type == JSON_ARRAY) {
                for (size_t k = 0; k < json_len(due_jobs); k++) {
                    json_t *j = json_get(due_jobs, k);
                    if (j && j->type == JSON_OBJECT) {
                        const char *did = sf_get_str(j, "id", NULL);
                        if (did && jid && strcmp(did, jid) == 0) { found = true; break; }
                    }
                }
            }
            if (!found) subset = false;
        }
    }
    if (!subset) {
        json_t *all = seam(io, "load_jobs", NULL);
        if (!sf_flow_raised(all) && sf_truthy(all)) {
            json_free(sweep_jobs);
            sweep_jobs = all;
        } else {
            json_free(all);  /* suppressed: keep due_jobs */
        }
    }
    free(local_home);
    json_t *a = json_object();
    json_set(a, "jobs", sweep_jobs);
    json_t *r = seam(io, "sweep_stale_inflight", a);
    if (sf_flow_raised(r)) sf_log(io, "warning");
    json_free(r);
}

/* ── flow: max parallel workers ────────────────────────────────────────── */

/* PoP: sf_resolve_max_parallel_workers @ cron/scheduler.py:_resolve_max_parallel_workers */
json_t *sf_resolve_max_parallel_workers(sf_flow_io_t *io) {
    json_t *a = json_object();
    json_set(a, "name", json_string("HERMES_CRON_MAX_PARALLEL"));
    json_t *envj = seam(io, "cron_env_setting", a);
    if (sf_flow_raised(envj)) {
        const char *t = sf_get_str(envj, "__raise__", "");
        if (strcmp(t, "ValueError") == 0 || strcmp(t, "TypeError") == 0) {
            sf_log(io, "warning");
            json_free(envj);
        } else {
            return envj;  /* propagate */
        }
    } else {
        char *env_par = sf_strip(envj && envj->type == JSON_STRING ? envj->str_val : "");
        json_free(envj);
        if (env_par && env_par[0]) {
            char *end = NULL;
            long v = strtol(env_par, &end, 10);
            if (end != env_par && *end == '\0') {
                free(env_par);
                return v ? json_number((double)v) : json_null();  /* int(x) or None */
            }
            free(env_par);
            sf_log(io, "warning");  /* int() ValueError */
        } else {
            free(env_par);
        }
    }
    /* with contextlib.suppress(Exception): config path */
    json_t *cfg = seam(io, "load_config", NULL);
    if (!sf_flow_raised(cfg)) {
        json_t *ucfg = sf_truthy(cfg) ? cfg : NULL;
        json_t *cron_cfg = (ucfg && ucfg->type == JSON_OBJECT) ? sf_get(ucfg, "cron") : NULL;
        json_t *cc = (cron_cfg && cron_cfg->type == JSON_OBJECT) ? cron_cfg : NULL;
        json_t *cfg_par = sf_get(cc, "max_parallel_jobs");
        if (cfg_par && cfg_par->type != JSON_NULL) {
            long v = cfg_par->type == JSON_NUMBER ? (long)cfg_par->num_val
                   : strtol(sf_get_str(cc, "max_parallel_jobs", "0"), NULL, 10);
            json_free(cfg);
            return v ? json_number((double)v) : json_null();
        }
    }
    json_free(cfg);
    return json_null();
}

/* ── flow: MCP orphan sweep ────────────────────────────────────────────── */

/* PoP: sf_sweep_mcp_orphans @ cron/scheduler.py:_sweep_mcp_orphans */
void sf_sweep_mcp_orphans(sf_flow_io_t *io) {
    json_t *r = seam(io, "_kill_orphaned_mcp_children", NULL);
    if (sf_flow_raised(r)) sf_log(io, "debug");
    json_free(r);
}

/* PoP: sf_sweep_mcp_orphans_when_all_done @ cron/scheduler.py:_sweep_mcp_orphans_when_all_done */
void sf_sweep_mcp_orphans_when_all_done(sf_flow_io_t *io, const json_t *futures) {
    size_t n = (futures && futures->type == JSON_ARRAY) ? json_len(futures) : 0;
    if (n == 0) {
        sf_sweep_mcp_orphans(io);
        return;
    }
    for (size_t i = 0; i < n; i++) {
        json_t *r = seam(io, "future.add_done_callback", NULL);
        json_free(r);
    }
    long remaining = (long)n;
    for (size_t i = 0; i < n; i++) {
        remaining--;
        json_t *f = json_get(futures, i);
        json_t *ex = sf_get(f, "exception");
        json_t *r = seam(io, "future.exception", NULL);
        json_free(r);
        if (ex && ex->type != JSON_NULL) sf_log(io, "error");
        if (remaining <= 0) sf_sweep_mcp_orphans(io);
    }
}

/* ── flow: due-job claim + dispatch ────────────────────────────────────── */

/* PoP: sf_process_due_job @ cron/scheduler.py:_process_due_job */
json_t *sf_process_due_job(sf_flow_io_t *io, const json_t *job) {
    const char *job_id = sf_get_str(job, "id", "");
    const char *execution_id = sf_get_str(job, "execution_id", "");
    json_t *a = json_object();
    json_set(a, "job_id", json_string(job_id));
    json_set(a, "return_job", json_bool(true));
    json_t *claimed = seam(io, "claim_job_for_fire", a);
    if (sf_flow_raised(claimed)) {
        const char *type = sf_get_str(claimed, "__raise__", "");
        const char *msg = sf_get_str(claimed, "message", "");
        if (strcmp(type, "OSError") == 0) {
            const char *name = sf_str_or(job, "name", job_id);
            char *label = NULL;
            asprintf(&label, "skipped job '%s'", name);
            json_t *na = json_object();
            json_set(na, "exc", json_copy(claimed));
            json_set(na, "label", json_string(label));
            json_set(na, "stage", json_string("claim"));
            json_t *jobs = json_array();
            json_append(jobs, json_copy(job));
            json_set(na, "jobs", jobs);
            json_t *nr = seam(io, "note_unwritable", na);
            json_free(nr);
            free(label);
            char *err = NULL;
            asprintf(&err, "Cron store unwritable; not started: %s", msg);
            json_t *sa = json_object();
            json_set(sa, "execution_id", json_string(execution_id));
            json_set(sa, "job_id", json_string(job_id));
            json_set(sa, "error", json_string(err));
            json_t *sr = seam(io, "settle_unstarted_execution", sa);
            json_free(sr);
            free(err);
            json_free(claimed);
            return json_bool(false);
        }
        char *err = NULL;
        asprintf(&err, "Fire claim failed: %s: %s", type, msg);
        json_t *sa = json_object();
        json_set(sa, "execution_id", json_string(execution_id));
        json_set(sa, "job_id", json_string(job_id));
        json_set(sa, "error", json_string(err));
        json_t *sr = seam(io, "settle_unstarted_execution", sa);
        json_free(sr);
        free(err);
        return claimed;  /* re-raise */
    }
    if (!sf_truthy(claimed)) {
        json_t *fa = json_object();
        json_set(fa, "execution_id", json_string(execution_id));
        json_set(fa, "success", json_bool(false));
        json_set(fa, "error", json_string("Fire claim lost; execution was not started."));
        json_t *fr = seam(io, "finish_execution", fa);
        json_free(fr);
        json_free(claimed);
        return json_bool(true);
    }
    json_t *claimed_job = (claimed->type == JSON_OBJECT) ? json_copy(claimed) : json_copy(job);
    json_set(claimed_job, "execution_id", json_string(execution_id));
    json_t *si = sf_get(job, "_scheduled_instant");
    json_set(claimed_job, "_scheduled_instant", si ? json_copy(si) : json_null());
    json_free(claimed);
    json_t *ra = json_object();
    json_set(ra, "job", claimed_job);
    return seam(io, "run_one_job", ra);
}

/* ── flow: fire-claim ownership loss bookkeeping ───────────────────────── */

/* PoP: sf_record_fire_ownership_lost @ cron/scheduler.py:_record_fire_ownership_lost */
void sf_record_fire_ownership_lost(sf_flow_io_t *io, const json_t *job,
                                   const char *fire_owner, const char *execution_id) {
    const char *job_id = sf_get_str(job, "id", "");
    bool owns = false;
    if (fire_owner) {
        json_t *a = json_object();
        json_set(a, "job_id", json_string(job_id));
        json_set(a, "expected_owner", json_string(fire_owner));
        json_t *r = seam(io, "heartbeat_fire_claim", a);
        owns = sf_truthy(r);
        json_free(r);
    }
    if (fire_owner && owns) {
        json_t *ra = json_object();
        json_set(ra, "job", json_copy(job));
        json_t *retry = seam(io, "is_retry_run", ra);
        bool ladder = sf_truthy(retry);
        json_free(retry);
        json_t *ma = json_object();
        json_set(ma, "job_id", json_string(job_id));
        json_set(ma, "success", json_bool(false));
        json_set(ma, "error", json_string(SF_OWNERSHIP_LOST_INTERRUPTED));
        json_set(ma, "expected_fire_owner", json_string(fire_owner));
        if (ladder) json_set(ma, "ladder_rung", json_bool(true));
        json_t *mr = seam(io, "mark_job_run", ma);
        json_free(mr);
        json_t *fa = json_object();
        json_set(fa, "execution_id", json_string(execution_id));
        json_set(fa, "success", json_bool(false));
        json_set(fa, "error", json_string(SF_OWNERSHIP_LOST_INTERRUPTED));
        json_t *fr = seam(io, "finish_execution", fa);
        json_free(fr);
    } else {
        json_t *fa = json_object();
        json_set(fa, "execution_id", json_string(execution_id));
        json_set(fa, "error", json_string("Fire claim ownership lost; stale result was discarded."));
        json_t *fr = seam(io, "finish_lost_execution", fa);
        json_free(fr);
    }
}

/* PoP: sf_fire_ownership_lost @ cron/scheduler.py:_FireOwnership.lost */
bool sf_fire_ownership_lost(sf_flow_io_t *io, const json_t *job, const json_t *ownership) {
    const char *job_id = sf_get_str(job, "id", "");
    json_t *tc = sf_get(ownership, "transport_cancel");
    bool transport_cancelled = tc && tc->type != JSON_NULL && sf_truthy(sf_get(tc, "set"));
    if (transport_cancelled) return true;
    json_t *ownerj = sf_get(ownership, "owner");
    const char *owner = (ownerj && ownerj->type == JSON_STRING) ? ownerj->str_val : NULL;
    if (!owner) return false;
    json_t *sa = json_object();
    json_set(sa, "job_id", json_string(job_id));
    json_t *sr = seam(io, "self_removal_delivery_allowed", sa);
    bool self_removed = sf_truthy(sr);
    json_free(sr);
    if (self_removed) return false;
    json_t *cl = sf_get(ownership, "claim_lost");
    bool cl_present = cl && cl->type != JSON_NULL;
    bool latched = cl_present && sf_truthy(sf_get(cl, "set"));
    json_t *ha = json_object();
    json_set(ha, "job_id", json_string(job_id));
    json_set(ha, "expected_owner", json_string(owner));
    json_t *hr = seam(io, "heartbeat_fire_claim", ha);
    if (sf_flow_raised(hr)) {
        sf_log(io, "debug");
        json_free(hr);
        return latched;
    }
    bool validated = sf_truthy(hr);
    json_free(hr);
    if (validated) return false;
    if (cl_present) {
        json_t *r = seam(io, "claim_lost.set", NULL);
        json_free(r);
    }
    return true;
}
/* ── flow: incident bookkeeping ────────────────────────────────────────── */

/* PoP: sf_mark_incident_alerted @ cron/scheduler.py:_mark_incident_alerted */
void sf_mark_incident_alerted(sf_flow_io_t *io, const char *incident_id) {
    if (!incident_id || !*incident_id) return;
    json_t *a = json_object();
    json_set(a, "incident_id", json_string(incident_id));
    json_set(a, "state", json_string("alerted"));
    json_t *r = seam(io, "set_incident_state", a);
    if (sf_flow_raised(r)) sf_log(io, "debug");
    json_free(r);
}

/* PoP: sf_resolve_incidents_for_recovered_job @ cron/scheduler.py:_resolve_incidents_for_recovered_job */
void sf_resolve_incidents_for_recovered_job(sf_flow_io_t *io, const json_t *job) {
    json_t *a = json_object();
    json_set(a, "job_id", json_string(sf_get_str(job, "id", "")));
    json_t *r = seam(io, "close_incidents_for_recovered_job", a);
    if (sf_flow_raised(r)) sf_log(io, "debug");
    json_free(r);
}

/* PoP: sf_upsert_incident_for_failure @ cron/scheduler.py:_upsert_incident_for_failure */
json_t *sf_upsert_incident_for_failure(sf_flow_io_t *io, const json_t *job,
                                       const char *error, const json_t *output_file) {
    json_t *out = json_array();
    json_t *a = json_object();
    json_set(a, "job_id", json_string(sf_get_str(job, "id", "")));
    json_set(a, "error", json_string(error ? error : ""));
    json_t *name = sf_get(job, "name");
    json_set(a, "job_name", name ? json_copy(name) : json_null());
    json_set(a, "output_file", output_file ? json_copy(output_file) : json_null());
    json_t *r = seam(io, "upsert_incident", a);
    if (sf_flow_raised(r)) {
        sf_log(io, "debug");
        json_free(r);
        json_append(out, json_bool(false));
        json_append(out, json_null());
        return out;
    }
    /* upsert_incident -> (incident_id, is_new) */
    json_t *idj = (r && r->type == JSON_ARRAY && json_len(r) >= 1) ? json_get(r, 0) : NULL;
    char *incident_id = idj ? sf_pystr(idj) : strdup("");
    json_free(r);
    json_t *ga = json_object();
    json_set(ga, "incident_id", json_string(incident_id));
    json_t *incident = seam(io, "get_incident", ga);
    if (sf_flow_raised(incident)) {
        sf_log(io, "debug");
        json_free(incident);
        free(incident_id);
        json_append(out, json_bool(false));
        json_append(out, json_null());
        return out;
    }
    const char *state = (incident && incident->type == JSON_OBJECT) ? sf_get_str(incident, "state", NULL) : NULL;
    bool withheld = false;
    if (state && strcmp(state, "closed") == 0) withheld = true;
    else if (state && strcmp(state, "alerted") == 0) withheld = sf_repeat_alert_withheld(io, incident);
    json_free(incident);
    json_append(out, json_bool(withheld));
    json_append(out, json_string(incident_id));
    free(incident_id);
    return out;
}

/* ── flow: stale forced-release record ─────────────────────────────────── */

/* PoP: sf_record_stale_release @ cron/scheduler.py:_record_stale_release */
void sf_record_stale_release(sf_flow_io_t *io, const json_t *job, const char *job_id,
                             double age, double allowance, const char *reason) {
    const char *name = sf_str_or(job, "name", job_id);
    sf_log(io, "warning");
    json_t *fa = json_object();
    json_set(fa, "job_id", json_string(job_id));
    json_set(fa, "name", json_string(name));
    json_set(fa, "age", json_number(age));
    json_set(fa, "allowance", json_number(allowance));
    json_t *fr = seam(io, "_record_forced_release", fa);
    json_free(fr);
    if (strcmp(reason, "ledger-terminal") == 0) return;
    json_t *repeat = sf_get(job, "repeat");
    json_t *rep = sf_truthy(repeat) ? repeat : NULL;
    if (rep && rep->type == JSON_OBJECT) {
        json_t *times = sf_get(rep, "times");
        if (times && times->type != JSON_NULL) {
            sf_log(io, "warning");
            return;
        }
    }
    char *err = NULL;
    asprintf(&err,
        "Stale in-flight claim force-released after %.1fm (allowance %.1fm); "
        "previous run never released the scheduler in-flight guard",
        age / 60.0, allowance / 60.0);
    json_t *ma = json_object();
    json_set(ma, "job_id", json_string(job_id));
    json_set(ma, "success", json_bool(false));
    json_set(ma, "error", json_string(err));
    json_t *mr = seam(io, "mark_job_run", ma);
    free(err);
    if (sf_flow_raised(mr)) sf_log(io, "warning");
    json_free(mr);
}

/* ── flow: parallel pool discard (shutdown) ────────────────────────────── */

/* PoP: sf_discard_parallel_pools @ cron/scheduler.py:discard_parallel_pools */
void sf_discard_parallel_pools(sf_flow_io_t *io, const json_t *home_keys) {
    json_t *pools = sf_state_get(io, "_parallel_pools");
    json_t *workers = sf_state_get(io, "_parallel_pool_max_workers");
    if (!home_keys || home_keys->type != JSON_ARRAY) return;
    for (size_t i = 0; i < json_len(home_keys); i++) {
        json_t *kj = json_get(home_keys, i);
        char *key = sf_pystr(kj);
        bool had = pools && pools->type == JSON_OBJECT && json_has(pools, key);
        if (pools && pools->type == JSON_OBJECT) json_obj_del(pools, key);
        if (workers && workers->type == JSON_OBJECT) json_obj_del(workers, key);
        if (had) {
            json_t *a = json_object();
            json_set(a, "key", json_string(key));
            json_set(a, "wait", json_bool(false));
            json_set(a, "cancel_futures", json_bool(false));
            json_t *r = seam(io, "pool.shutdown", a);
            json_free(r);
        }
        free(key);
    }
}

/* ── flow: per-run scope enter / exit ──────────────────────────────────── */

/* PoP: sf_cron_run_scope_enter @ cron/scheduler.py:_CronRunScope.enter */
void sf_cron_run_scope_enter(sf_flow_io_t *io, json_t *scope) {
    json_t *a = json_object();
    json_set(a, "value", json_string("1"));
    json_t *tok = seam(io, "cron_session_var.set", a);
    json_set(scope, "_cron_session_token", tok);
    json_t *nd = seam(io, "enter_non_dispatcher_owned_context", NULL);
    json_set(scope, "_non_dispatcher_token", nd);
    json_t *ba = json_object();
    json_set(ba, "task_id", json_copy(sf_get(scope, "task_id")));
    json_t *sp = seam(io, "bind_run", ba);
    json_set(scope, "_spend_token", sp);
}

/* PoP: sf_cron_run_scope_exit @ cron/scheduler.py:_CronRunScope.exit */
void sf_cron_run_scope_exit(sf_flow_io_t *io, json_t *scope) {
    json_t *ca = json_object();
    json_set(ca, "task_id", json_copy(sf_get(scope, "task_id")));
    json_t *r1 = seam(io, "clear_session_cwd", ca);
    json_free(r1);
    json_t *va = json_object();
    json_set(va, "tokens", json_copy(sf_get(scope, "_ctx_tokens")));
    json_t *r2 = seam(io, "clear_session_vars", va);
    json_free(r2);
    json_t *cst = sf_get(scope, "_cron_session_token");
    if (cst && cst->type != JSON_NULL) {
        json_t *a = json_object();
        json_set(a, "token", json_copy(cst));
        json_t *r = seam(io, "cron_session_var.reset", a);
        json_free(r);
    }
    json_t *ndt = sf_get(scope, "_non_dispatcher_token");
    if (ndt && ndt->type != JSON_NULL) {
        json_t *a = json_object();
        json_set(a, "token", json_copy(ndt));
        json_t *r = seam(io, "exit_non_dispatcher_owned_context", a);
        json_free(r);
    }
    json_t *spt = sf_get(scope, "_spend_token");
    if (spt && spt->type != JSON_NULL) {
        json_t *a = json_object();
        json_set(a, "token", json_copy(spt));
        json_t *r = seam(io, "unbind_run", a);
        json_free(r);
    }
    static const char *vars[] = {
        "HERMES_CRON_AUTO_DELIVER_PLATFORM",
        "HERMES_CRON_AUTO_DELIVER_CHAT_ID",
        "HERMES_CRON_AUTO_DELIVER_THREAD_ID",
    };
    for (int i = 0; i < 3; i++) {
        json_t *a = json_object();
        json_set(a, "name", json_string(vars[i]));
        json_set(a, "value", json_string(""));
        json_t *r = seam(io, "var_map.set", a);
        json_free(r);
    }
}

/* ── flow: fire-claim heartbeat wrapper ────────────────────────────────── */

static void sf_finish_unstarted(sf_flow_io_t *io, const json_t *job, const char *error) {
    json_t *eid = sf_get(job, "execution_id");
    if (!sf_truthy(eid)) return;
    json_t *a = json_object();
    json_set(a, "execution_id", json_copy(eid));
    json_set(a, "error", json_string(error));
    json_t *r = seam(io, "finish_lost_execution", a);
    if (sf_flow_raised(r)) sf_log(io, "warning");
    json_free(r);
}

/* PoP: sf_run_with_fire_claim_heartbeat @ cron/scheduler.py:_run_with_fire_claim_heartbeat */
json_t *sf_run_with_fire_claim_heartbeat(sf_flow_io_t *io, const json_t *job) {
    json_t *claim = sf_get(job, "fire_claim");
    char *owner = strdup("");
    if (claim && claim->type == JSON_OBJECT) {
        json_t *by = sf_get(claim, "by");
        if (sf_truthy(by)) { free(owner); owner = sf_pystr(by); }
    }
    if (!owner[0]) {
        free(owner);
        json_t *a = json_object();
        json_set(a, "lost", json_null());
        return seam(io, "run", a);
    }
    char *job_id = sf_pystr(sf_truthy(sf_get(job, "id")) ? sf_get(job, "id") : NULL);
    json_t *ha = json_object();
    json_set(ha, "job_id", json_string(job_id));
    json_set(ha, "expected_owner", json_string(owner));
    json_t *hb = seam(io, "heartbeat_fire_claim", ha);
    if (sf_flow_raised(hb)) {
        sf_log(io, "warning");
        sf_finish_unstarted(io, job, "Fire claim ownership could not be validated before execution started.");
        json_free(hb);
        free(owner);
        free(job_id);
        return json_bool(true);
    }
    bool owns_false = hb && hb->type == JSON_BOOL && !hb->bool_val;
    json_free(hb);
    if (owns_false) {
        sf_log(io, "warning");
        sf_finish_unstarted(io, job, "Fire claim ownership lost before execution started.");
        free(owner);
        free(job_id);
        return json_bool(true);
    }
    json_t *ta = json_object();
    json_set(ta, "name", json_string("cron-fire-claim-heartbeat"));
    json_t *thread = seam(io, "_start_heartbeat_thread", ta);
    if (!thread || thread->type == JSON_NULL) {
        sf_finish_unstarted(io, job, "Fire claim heartbeat could not be started; execution was not run.");
        json_free(thread);
        free(owner);
        free(job_id);
        return json_bool(true);
    }
    json_free(thread);
    free(owner);
    free(job_id);
    json_t *ra = json_object();
    json_set(ra, "lost", json_string("lost_ownership"));
    json_t *ret = seam(io, "run", ra);
    /* finally: stop.set(); heartbeat_thread.join(timeout=1.0) */
    json_t *s1 = seam(io, "stop.set", NULL);
    json_free(s1);
    json_t *ja = json_object();
    json_set(ja, "timeout", json_number(1.0));
    json_t *s2 = seam(io, "thread.join", ja);
    json_free(s2);
    return ret;
}

/* ── flow: delivery composition ────────────────────────────────────────── */

/* re.sub(r"\[blocked_config[^\]]*\]\s*", "", err). Caller frees. */
static char *sf_strip_blocked_config(const char *err) {
    size_t n = strlen(err);
    char *out = malloc(n + 1);
    size_t j = 0;
    const char *p = err;
    while (*p) {
        if (strncmp(p, "[blocked_config", 15) == 0) {
            const char *q = p + 15;
            while (*q && *q != ']') q++;
            if (*q == ']') {
                q++;
                while (*q && sf_is_ws((unsigned char)*q)) q++;
                p = q;
                continue;
            }
        }
        out[j++] = *p++;
    }
    out[j] = '\0';
    return out;
}

/* PoP: sf_compose_run_delivery @ cron/scheduler.py:_compose_run_delivery */
json_t *sf_compose_run_delivery(sf_flow_io_t *io, const json_t *job, bool success,
                                const json_t *error, const char *final_response,
                                const json_t *output_file, bool agent_declared) {
    char *err = error && sf_truthy(error) ? sf_pystr(error) : strdup("");
    bool blocked_config_silent = strstr(err, "[blocked_config:silent]") != NULL;
    bool blocked_config = blocked_config_silent || strstr(err, "[blocked_config]") != NULL;
    bool incident_acked = false;
    json_t *failure_incident_id = json_null();
    char *deliver_content = NULL;
    const char *job_name = sf_str_or(job, "name", sf_get_str(job, "id", ""));
    const char *job_id = sf_get_str(job, "id", "");
    if (blocked_config && !success) {
        char *stripped = sf_strip_blocked_config(err);
        char *pf_text = sf_strip(stripped);
        free(stripped);
        json_t *a = json_object();
        json_set(a, "name", json_string(job_name));
        json_set(a, "text", json_string(pf_text));
        json_t *r = seam(io, "blocked_config_notice", a);
        deliver_content = sf_pystr(r);
        json_free(r);
        free(pf_text);
    } else if (success) {
        deliver_content = strdup(final_response ? final_response : "");
        sf_resolve_incidents_for_recovered_job(io, job);
    } else {
        json_t *up = sf_upsert_incident_for_failure(io, job, err[0] ? err : "", output_file);
        incident_acked = sf_truthy(json_get(up, 0));
        json_free(failure_incident_id);
        failure_incident_id = json_copy(json_get(up, 1));
        json_free(up);
        if (incident_acked) {
            deliver_content = strdup("");
        } else if (agent_declared) {
            char *es = sf_strip(err);
            char *er = sf_rstrip(es);
            /* err.strip().rstrip(".") */
            size_t el = strlen(er);
            while (el && er[el - 1] == '.') er[--el] = '\0';
            json_t *a = json_object();
            json_set(a, "name", json_string(job_name));
            json_set(a, "job_id", json_string(job_id));
            json_set(a, "text", json_string(er));
            json_t *r = seam(io, "generic_failure_notice", a);
            char *notice = sf_pystr(r);
            json_free(r);
            free(es);
            free(er);
            char *nudge = sf_failure_streak_nudge(io, job);
            asprintf(&deliver_content, "%s%s", notice, nudge);
            free(notice);
            free(nudge);
        } else {
            json_t *sa = json_object();
            json_set(sa, "job", json_copy(job));
            json_set(sa, "error", json_copy(error ? error : json_null()));
            json_t *sr = seam(io, "_summarize_cron_failure_for_delivery", sa);
            char *summary = sf_pystr(sr);
            json_free(sr);
            char *nudge = sf_failure_streak_nudge(io, job);
            json_t *ha = json_object();
            json_set(ha, "job", json_copy(job));
            json_t *qs = sf_get(job, "_quota_hold_seconds");
            json_set(ha, "seconds", qs ? json_copy(qs) : json_null());
            json_t *hr = seam(io, "hold_notice", ha);
            char *hold = sf_pystr(hr);
            json_free(hr);
            asprintf(&deliver_content, "%s%s%s", summary, nudge, hold);
            free(summary);
            free(nudge);
            free(hold);
        }
    }
    free(err);
    json_t *out = json_array();
    json_append(out, json_string(deliver_content ? deliver_content : ""));
    json_append(out, json_bool(blocked_config));
    json_append(out, json_bool(blocked_config_silent));
    json_append(out, json_bool(incident_acked));
    json_append(out, failure_incident_id);
    free(deliver_content);
    return out;
}

/* PoP: sf_deliver_crash_failure @ cron/scheduler.py:_deliver_crash_failure */
json_t *sf_deliver_crash_failure(sf_flow_io_t *io, const json_t *job, const char *err_text) {
    json_t *la = json_object();
    json_set(la, "job", json_copy(job));
    json_set(la, "for_failure", json_bool(true));
    json_t *lane = seam(io, "_delivery_lane_value", la);
    json_t *na = json_object();
    json_set(na, "value", lane);
    json_t *norm = seam(io, "_normalize_deliver_value", na);
    char *normalized_deliver = sf_pystr(norm);
    json_free(norm);
    json_t *up = sf_upsert_incident_for_failure(io, job, err_text, NULL);
    bool incident_acked = sf_truthy(json_get(up, 0));
    char *failure_incident_id = sf_pystr(json_get(up, 1));
    json_free(up);
    json_t *out = json_array();
    if (incident_acked) {
        json_append(out, json_null());
        json_append(out, json_string("suppressed_acked"));
        free(normalized_deliver);
        free(failure_incident_id);
        return out;
    }
    char *delivery_error = NULL;
    /* try: _deliver_result(job, _summarize(...)+nudge, for_failure=True) */
    json_t *sa = json_object();
    json_set(sa, "job", json_copy(job));
    json_set(sa, "error", json_string(err_text ? err_text : ""));
    json_t *sr = seam(io, "_summarize_cron_failure_for_delivery", sa);
    if (sf_flow_raised(sr)) {
        delivery_error = sf_pystr(sr);
        sf_log(io, "error");
        json_free(sr);
    } else {
        char *summary = sf_pystr(sr);
        json_free(sr);
        char *nudge = sf_failure_streak_nudge(io, job);
        char *content = NULL;
        asprintf(&content, "%s%s", summary, nudge);
        free(summary);
        free(nudge);
        json_t *da = json_object();
        json_set(da, "job", json_copy(job));
        json_set(da, "content", json_string(content));
        json_set(da, "for_failure", json_bool(true));
        json_t *dr = seam(io, "_deliver_result", da);
        free(content);
        if (sf_flow_raised(dr)) {
            delivery_error = sf_pystr(dr);
            sf_log(io, "error");
        } else if (dr && dr->type == JSON_STRING) {
            delivery_error = strdup(dr->str_val);
        } else if (dr && dr->type != JSON_NULL) {
            delivery_error = sf_pystr(dr);
        }
        json_free(dr);
    }
    bool unresolved_origin = false;
    if ((!delivery_error || !*delivery_error) && strcmp(normalized_deliver, "origin") == 0) {
        json_t *ta = json_object();
        json_set(ta, "job", json_copy(job));
        json_set(ta, "for_failure", json_bool(true));
        json_t *targets = seam(io, "_resolve_delivery_targets", ta);
        unresolved_origin = !sf_truthy(targets);
        json_free(targets);
    }
    json_t *ca = json_object();
    json_set(ca, "delivery_error", delivery_error ? json_string(delivery_error) : json_null());
    json_set(ca, "should_deliver", json_bool(true));
    json_set(ca, "unresolved_origin", json_bool(unresolved_origin));
    json_set(ca, "normalized_deliver", json_string(normalized_deliver));
    json_set(ca, "incident_acked", json_bool(false));
    json_set(ca, "success", json_bool(false));
    json_t *dq = sf_get(job, "last_delivery_queued");
    json_set(ca, "delivery_queued", dq ? json_copy(dq) : json_null());
    json_set(ca, "notification_suppressed", json_bool(sf_truthy(sf_get(job, "_notification_all_targets_suppressed"))));
    json_t *outcome = seam(io, "_classify_delivery_outcome", ca);
    char *oc = sf_pystr(outcome);
    json_free(outcome);
    if (strcmp(oc, "delivered") == 0 || strcmp(oc, "not_configured") == 0) {
        sf_mark_incident_alerted(io, strcmp(failure_incident_id, "None") == 0 ? NULL : failure_incident_id);
    }
    json_append(out, delivery_error ? json_string(delivery_error) : json_null());
    json_append(out, json_string(oc));
    free(oc);
    free(delivery_error);
    free(normalized_deliver);
    free(failure_incident_id);
    return out;
}
