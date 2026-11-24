/*
 * config_value_checks.c - C home for upstream Hermes hermes_cli/config_value_checks.py.
 *
 * PoP: w18_configvalu_4986b6_validate_voice @ hermes_cli/config_value_checks.py:_validate_voice
 * PoP: w18_configvalu_4986b6_validate_timezone @ hermes_cli/config_value_checks.py:_validate_timezone
 *
 * Issues are emitted as JSON objects carrying the ConfigIssue dataclass fields
 * (severity, message, hint). The {value!r} parts use a Python-repr helper so
 * str/None/bool/int values render exactly as the live f-strings do.
 * zoneinfo availability and the valid zone set are explicit injected seams.
 */
#include "noc_w18_configvalu_4986b6.h"

#include "hermes_json.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *cv_dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    if (n) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

/* Python str.strip() over the ASCII whitespace encountered in config values. */
static char *cv_strip(const char *s) {
    if (!s) return cv_dupstr("");
    const char *a = s;
    while (*a && isspace((unsigned char)*a)) a++;
    const char *b = a + strlen(a);
    while (b > a && isspace((unsigned char)b[-1])) b--;
    size_t n = (size_t)(b - a);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, a, n);
    out[n] = '\0';
    return out;
}

/* strip().lower() into a fixed buffer (ASCII folding). */
static void cv_strip_lower(const char *s, char *out, size_t cap) {
    char *t = cv_strip(s);
    size_t n = t ? strlen(t) : 0;
    if (n > cap - 1) n = cap - 1;
    for (size_t i = 0; i < n; ++i) out[i] = (char)tolower((unsigned char)t[i]);
    out[n] = '\0';
    free(t);
}

static void cv_repr_cstr(const char *s, char *out, size_t cap) {
    if (!s) s = "";
    char q = (strchr(s, '\'') && !strchr(s, '"')) ? '"' : '\'';
    size_t n = 0, lim = cap ? cap - 3 : 0;
    if (n < lim) out[n++] = q;
    for (const char *p = s; *p && n < lim; ++p) {
        unsigned char c = (unsigned char)*p;
        if (c == '\\' || c == (unsigned char)q) {
            if (n + 1 < lim) { out[n++] = '\\'; out[n++] = (char)c; }
        } else if (c == '\n') { if (n + 1 < lim) { out[n++] = '\\'; out[n++] = 'n'; } }
        else if (c == '\t') { if (n + 1 < lim) { out[n++] = '\\'; out[n++] = 't'; } }
        else if (c == '\r') { if (n + 1 < lim) { out[n++] = '\\'; out[n++] = 'r'; } }
        else out[n++] = (char)c;
    }
    if (n < lim + 2) out[n++] = q;
    out[n] = '\0';
}

/* Python repr() for the scalar values the checks report. */
static void cv_py_repr(const json_t *v, char *out, size_t cap) {
    if (!v || v->type == JSON_NULL) { snprintf(out, cap, "None"); return; }
    if (v->type == JSON_BOOL) { snprintf(out, cap, "%s", v->bool_val ? "True" : "False"); return; }
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == floor(d) && fabs(d) < 9.0e18) snprintf(out, cap, "%lld", (long long)d);
        else snprintf(out, cap, "%g", d);
        return;
    }
    if (v->type == JSON_STRING) { cv_repr_cstr(v->str_val, out, cap); return; }
    snprintf(out, cap, "None"); /* containers are not used as reported values */
}

static void cv_add_issue(json_t *out, const char *sev, const char *msg, const char *hint) {
    json_t *iss = json_object();
    json_set(iss, "severity", json_string(sev));
    json_set(iss, "message", json_string(msg));
    json_set(iss, "hint", json_string(hint));
    json_append(out, iss);
}

struct json_t *w18_configvalu_4986b6_validate_voice(const struct json_t *config) {
    json_t *out = json_array();
    const json_t *voice = (config && config->type == JSON_OBJECT)
        ? json_obj_get(config, "voice") : NULL;

    if (voice && voice->type == JSON_OBJECT && json_has(voice, "auto_tts_mode")) {
        const json_t *raw = json_obj_get(voice, "auto_tts_mode");
        char norm[1024];
        if (raw && raw->type == JSON_STRING) cv_strip_lower(raw->str_val, norm, sizeof(norm));
        else norm[0] = '\0';
        if (strcmp(norm, "all") && strcmp(norm, "voice_only")) {
            char repr[2048], msg[4096];
            cv_py_repr(raw, repr, sizeof(repr));
            snprintf(msg, sizeof(msg),
                     "voice.auto_tts_mode must be 'all' or 'voice_only', got %s", repr);
            cv_add_issue(out, "error", msg,
                         "Set voice.auto_tts_mode to all (audio with every reply) or "
                         "voice_only (audio only after voice input)");
        }
    }

    if (!(voice && voice->type == JSON_OBJECT && json_has(voice, "submit_mode"))) return out;
    const json_t *raw = json_obj_get(voice, "submit_mode");
    char norm[1024];
    if (raw && raw->type == JSON_STRING) cv_strip_lower(raw->str_val, norm, sizeof(norm));
    else norm[0] = '\0';
    if (strcmp(norm, "direct") && strcmp(norm, "draft")) {
        char repr[2048], msg[4096];
        cv_py_repr(raw, repr, sizeof(repr));
        snprintf(msg, sizeof(msg), "voice.submit_mode must be 'direct' or 'draft', got %s", repr);
        cv_add_issue(out, "error", msg,
                     "Set voice.submit_mode to direct (submit immediately) or draft (edit before sending)");
    }
    return out;
}

static const char *CV_TZ_HINT =
    "Use an IANA zone name such as America/New_York or Asia/Tokyo (see "
    "`timedatectl list-timezones`). With an invalid value the agent clock and cron "
    "schedules silently fall back to server-local time. HERMES_TIMEZONE overrides "
    "this key when set.";

struct json_t *w18_configvalu_4986b6_validate_timezone(const struct json_t *config,
                                                       int tz_available,
                                                       const struct json_t *valid_zones) {
    json_t *out = json_array();
    if (!config || config->type != JSON_OBJECT || !json_has(config, "timezone")) return out;
    const json_t *tz = json_obj_get(config, "timezone");

    if (tz && tz->type != JSON_NULL && tz->type != JSON_STRING) {
        char repr[2048], msg[4096];
        cv_py_repr(tz, repr, sizeof(repr));
        snprintf(msg, sizeof(msg), "timezone must be an IANA zone name string, got %s", repr);
        cv_add_issue(out, "error", msg, CV_TZ_HINT);
        return out;
    }

    char *name = cv_strip(tz && tz->type == JSON_STRING ? tz->str_val : "");
    if (!name || !name[0]) { free(name); return out; }
    if (!tz_available) { free(name); return out; }

    int valid = 0;
    if (valid_zones && valid_zones->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(valid_zones); ++i) {
            const json_t *z = json_get(valid_zones, i);
            if (z && z->type == JSON_STRING && z->str_val && !strcmp(z->str_val, name)) {
                valid = 1;
                break;
            }
        }
    }
    if (!valid) {
        char bound[2048], repr[256], msg[4096];
        cv_repr_cstr(name, bound, sizeof(bound));
        snprintf(repr, sizeof(repr), "%s", bound);
        snprintf(msg, sizeof(msg), "timezone %s is not a valid IANA zone name", repr);
        cv_add_issue(out, "error", msg, CV_TZ_HINT);
    }
    free(name);
    return out;
}
