/*
 * port_session_activity.c — Port of Python: agent/session_activity.py (pure
 * activity-stamp core plus the AwakeIdleMeter sleep-removal math).
 */

#include "port_session_activity.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SA_MAXSIZE_U (9223372036854775807ULL)

static char *dup_(const char *s, size_t n)
{
    char *o = (char *)malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s ? s : "", n);
    o[n] = '\0';
    return o;
}

/* PoP: sa_normalize_activity_provenance @ agent/session_activity.py:normalize_activity_provenance */
char *sa_normalize_activity_provenance(const char *provenance)
{
    const char *raw = provenance ? provenance : "";
    size_t len = strlen(raw);
    size_t s = 0, e = len;
    while (s < e && (raw[s] == ' ' || raw[s] == '\t' || raw[s] == '\r' || raw[s] == '\n'))
        s++;
    while (e > s && (raw[e - 1] == ' ' || raw[e - 1] == '\t' || raw[e - 1] == '\r' || raw[e - 1] == '\n'))
        e--;
    size_t n = e - s;
    static const char *const known[] = {
        SA_PROV_UNKNOWN, SA_PROV_AGENT_COMPRESSION, SA_PROV_AGENT_COMPRESSION_TIMEOUT,
        SA_PROV_AGENT_COMPRESSION_COOLDOWN, SA_PROV_AGENT_COMPRESSION_TURNHOLD,
    };
    for (size_t i = 0; i < sizeof(known) / sizeof(known[0]); i++)
        if (strlen(known[i]) == n && strncmp(raw + s, known[i], n) == 0)
            return dup_(known[i], n);
    return dup_(SA_PROV_UNKNOWN, strlen(SA_PROV_UNKNOWN));
}

/* PoP: sa_is_terminal_compression_provenance @ agent/session_activity.py:is_terminal_compression_provenance */
int sa_is_terminal_compression_provenance(const char *provenance)
{
    char *n = sa_normalize_activity_provenance(provenance);
    int ret = strcmp(n, SA_PROV_AGENT_COMPRESSION_TIMEOUT) == 0 ||
              strcmp(n, SA_PROV_AGENT_COMPRESSION_COOLDOWN) == 0 ||
              strcmp(n, SA_PROV_AGENT_COMPRESSION_TURNHOLD) == 0;
    free(n);
    return ret;
}

/* PoP: sa_bound_activity_description @ agent/session_activity.py:bound_activity_description */
char *sa_bound_activity_description(const char *description)
{
    const char *raw = description ? description : "";
    size_t len = strlen(raw);
    size_t s = 0, e = len;
    while (s < e && (raw[s] == ' ' || raw[s] == '\t' || raw[s] == '\r' || raw[s] == '\n'))
        s++;
    while (e > s && (raw[e - 1] == ' ' || raw[e - 1] == '\t' || raw[e - 1] == '\r' || raw[e - 1] == '\n'))
        e--;
    size_t n = e - s;
    if (n <= SA_ACTIVITY_DESCRIPTION_MAX)
        return dup_(raw + s, n);
    /* text[:119] + "…" (U+2026, UTF-8 e2 80 a6) -> 120 chars total. */
    char *out = (char *)malloc(SA_ACTIVITY_DESCRIPTION_MAX + 1);
    if (!out) return NULL;
    memcpy(out, raw + s, SA_ACTIVITY_DESCRIPTION_MAX - 1);
    out[SA_ACTIVITY_DESCRIPTION_MAX - 1] = (char)0xE2;
    out[SA_ACTIVITY_DESCRIPTION_MAX] = (char)0x80;
    out[SA_ACTIVITY_DESCRIPTION_MAX + 1] = (char)0xA6;
    return out;
}

/* Strict decimal integer parse matching Python int(str). Rejects ".", "e",
 * whitespace, trailing junk. Returns 0 on parse failure (caller checks ok). */
static int parse_cap(const char *text, unsigned long long *out)
{
    if (!text)
        return 0;
    size_t i = 0, len = strlen(text);
    if (i < len && (text[i] == '-' || text[i] == '+'))
        i++;
    if (i >= len)
        return 0;
    unsigned long long v = 0;
    for (; i < len; i++) {
        if (text[i] < '0' || text[i] > '9')
            return 0;
        if (v > (SA_MAXSIZE_U - (unsigned long long)(text[i] - '0')) / 10)
            return 0; /* overflow */
        v = v * 10 + (unsigned long long)(text[i] - '0');
    }
    *out = v;
    return 1;
}

/* PoP: sa_format_iteration_progress @ agent/session_activity.py:format_iteration_progress */
char *sa_format_iteration_progress(const char *n_text, const char *max_iterations)
{
    unsigned long long cap = 0;
    int ok = parse_cap(max_iterations, &cap);
    const char *n = n_text ? n_text : "";
    if (!ok || cap >= SA_MAXSIZE_U) {
        /* "iteration {n}" */
        size_t need = strlen("iteration ") + strlen(n) + 1;
        char *out = (char *)malloc(need);
        if (!out) return NULL;
        snprintf(out, need, "iteration %s", n);
        return out;
    }
    /* "iteration {n}/{cap}" */
    char cap_buf[32];
    snprintf(cap_buf, sizeof(cap_buf), "%llu", cap);
    size_t need = strlen("iteration ") + strlen(n) + 1 + strlen(cap_buf) + 1;
    char *out = (char *)malloc(need);
    if (!out) return NULL;
    snprintf(out, need, "iteration %s/%s", n, cap_buf);
    return out;
}

/* PoP: sa_build_activity_snapshot @ agent/session_activity.py:build_activity_snapshot */
json_t *sa_build_activity_snapshot(const char *last_activity_at,
                                   const char *last_activity_description,
                                   const char *last_activity_provenance,
                                   double now, const char *extra_json,
                                   int has_extra)
{
    /* when = float(last_activity_at) if last_activity_at is not None else None */
    int when_is_null = (last_activity_at == NULL);
    double when = when_is_null ? 0.0 : strtod(last_activity_at, NULL);

    char *desc = sa_bound_activity_description(last_activity_description);
    char *prov = sa_normalize_activity_provenance(last_activity_provenance);

    json_t *snap = json_object();
    if (when_is_null) {
        json_set(snap, "last_activity_at", json_null());
        json_set(snap, "last_activity_ts", json_null());
    } else {
        json_set(snap, "last_activity_at", json_number(when));
        json_set(snap, "last_activity_ts", json_number(when));
    }
    json_set(snap, "last_activity_description", json_string(desc ? desc : ""));
    json_set(snap, "description", json_string(desc ? desc : ""));
    json_set(snap, "last_activity_desc", json_string(desc ? desc : ""));
    json_set(snap, "last_activity_provenance", json_string(prov ? prov : SA_PROV_UNKNOWN));
    json_set(snap, "provenance", json_string(prov ? prov : SA_PROV_UNKNOWN));

    if (when_is_null) {
        json_set(snap, "seconds_since_activity", json_null());
    } else {
        double seconds = nearbyint((now - when) * 10.0) / 10.0; /* FE_TONEAREST = round-half-even */
        json_set(snap, "seconds_since_activity", json_number(seconds));
    }

    if (has_extra && extra_json) {
        json_t *extra = json_parse(extra_json, NULL);
        if (extra && extra->type == JSON_OBJECT) {
            for (size_t i = 0; i < extra->c.count; i++) {
                /* steal a fresh copy of each extra value: serialize + reparse */
                char *ser = json_serialize(extra->c.items[i]);
                if (ser) {
                    json_t *val = json_parse(ser, NULL);
                    free(ser);
                    if (val)
                        json_set(snap, extra->c.keys[i], val);
                }
            }
        }
    }
    free(desc);
    free(prov);
    return snap;
}

/* PoP: sa_idle_meter_init @ agent/session_activity.py:AwakeIdleMeter.__init__ */
void sa_idle_meter_init(sa_idle_meter_t *m, double wall, double mono)
{
    m->wall = wall;
    m->mono = mono;
    m->asleep_s = 0.0;
}

/* PoP: sa_measure_idle @ agent/session_activity.py:AwakeIdleMeter.measure */
double sa_measure_idle(sa_idle_meter_t *m, double idle_s, double wall, double mono)
{
    double slept_s = fmax(0.0, (wall - m->wall) - (mono - m->mono));
    if (idle_s < wall - m->wall)
        m->asleep_s = 0.0;
    m->asleep_s += slept_s;
    m->wall = wall;
    m->mono = mono;
    m->asleep_s = fmax(0.0, fmin(m->asleep_s, idle_s + slept_s));
    return fmax(0.0, idle_s - m->asleep_s);
}
