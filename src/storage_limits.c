/*
 * storage_limits.c — faithful C11 port of upstream Hermes storage_limits.py
 * (gate storlim).
 * PoP: storlim_configured_transcript_limit @ storage_limits.py:_configured_transcript_limit
 * PoP: storlim_resolved_max_resume_messages @ storage_limits.py:resolved_max_resume_messages
 * PoP: storlim_resolved_max_export_messages @ storage_limits.py:resolved_max_export_messages
 *
 * The live function reads config.yaml via hermes_cli.config.load_config_readonly()
 * (a lazy, circular-import-guarded I/O call). That external read is the caller's
 * responsibility: pass the already-loaded value of config["sessions"] in `sessions`.
 */
#include "noc_storlim.h"
#include "hermes_json.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

static int st_is_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* Python int(value) for the JSON value shapes config values take. 0 = error. */
static int st_py_int(const json_t *v, int64_t *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) {
        *out = (int64_t)v->num_val;   /* int(float) truncates toward zero */
        return 1;
    }
    if (v->type == JSON_BOOL) {
        *out = v->bool_val ? 1 : 0;
        return 1;
    }
    if (v->type == JSON_STRING) {
        const char *s = v->str_val;
        char buf[4096];
        size_t n = 0, i;
        int neg = 0;
        char *endp;
        long long parsed;
        if (!s) return 0;
        while (*s && st_is_ws((unsigned char)*s)) s++;
        if (*s == '+' || *s == '-') { neg = (*s == '-'); s++; }
        if (*s == '\0') return 0;
        for (i = 0; s[i]; i++) {
            char c = s[i];
            if (c >= '0' && c <= '9') {
                if (n + 1 >= sizeof(buf)) return 0;
                buf[n++] = c;
            } else if (c == '_') {
                /* Python only permits a single underscore between digits. */
                if (n == 0 || !(s[i + 1] >= '0' && s[i + 1] <= '9')) return 0;
            } else {
                /* trim trailing whitespace only */
                size_t k = i;
                while (s[k] && st_is_ws((unsigned char)s[k])) k++;
                if (s[k] != '\0') return 0;
                break;
            }
        }
        if (n == 0) return 0;
        buf[n] = '\0';
        errno = 0;
        parsed = strtoll(buf, &endp, 10);
        if (errno == ERANGE || !endp || *endp != '\0') return 0;
        *out = (int64_t)(neg ? -parsed : parsed);
        return 1;
    }
    /* None/list/dict: int() raises in Python. */
    return 0;
}

int64_t storlim_configured_transcript_limit(const char *key, const struct json_t *sessions,
                                            int64_t fallback) {
    json_t *value;
    int64_t limit;
    if (!key) return fallback;
    /* (load_config_readonly().get("sessions") or {}).get(key) — falsy or
     * non-object sessions yields no value; Python non-dict truthy sessions
     * raises AttributeError, also caught -> fallback. */
    if (!sessions || sessions->type != JSON_OBJECT) return fallback;
    value = json_obj_get(sessions, key);
    if (!value || value->type == JSON_NULL) return fallback;
    if (!st_py_int(value, &limit)) return fallback;
    return limit >= 0 ? limit : fallback;
}

int64_t storlim_resolved_max_resume_messages(const struct json_t *sessions) {
    return storlim_configured_transcript_limit("max_resume_messages", sessions,
                                               STORLIM_MAX_SAFE_MESSAGES);
}

int64_t storlim_resolved_max_export_messages(const struct json_t *sessions) {
    return storlim_configured_transcript_limit("max_export_messages", sessions,
                                               STORLIM_MAX_SAFE_MESSAGES);
}
