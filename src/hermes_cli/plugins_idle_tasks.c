/*
 * plugins_idle_tasks.c — C11 port of upstream Hermes hermes_cli/plugins_idle_tasks.py.
 *
 * PoP: w17_pluginsidl_dc8b64_base_state  @ hermes_cli/plugins_idle_tasks.py:_base_state
 * PoP: w17_pluginsidl_dc8b64_valid_name  @ hermes_cli/plugins_idle_tasks.py:_NAME_RE
 * PoP: w17_pluginsidl_dc8b64_build_spec  @ hermes_cli/plugins_idle_tasks.py:build_idle_task_spec
 *
 * Pure state/validation surface: _base_state's canonical dict, the name regex
 * ^[a-z0-9][a-z0-9_-]{0,63}$, and build_idle_task_spec's validation + normalization
 * (finite floats, interval floored to 1.0, negative idle clamped to 0.0).
 * The oracle supplies a callable fn and a fixed ctx, so only the pure decisions
 * are compared. All exported symbols are prefixed with the gate key.
 */
#include "noc_w17_pluginsidl_dc8b64.h"
#include "hermes_json.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (p) memcpy(p, s, n + 1);
    return p;
}

struct json_t *w17_pluginsidl_dc8b64_base_state(const char *plugin_id, const char *name) {
    json_t *o = json_object();
    json_set(o, "plugin", json_string(plugin_id ? plugin_id : ""));
    json_set(o, "task", json_string(name ? name : ""));
    json_set(o, "last_run_at", json_null());
    json_set(o, "run_count", json_number(0));
    json_set(o, "paused", json_bool(false));
    json_set(o, "last_summary", json_null());
    json_set(o, "last_status", json_null());
    json_set(o, "last_error", json_null());
    json_set(o, "last_run_duration_seconds", json_null());
    json_set(o, "interval_hours", json_null());
    return o;
}

int w17_pluginsidl_dc8b64_valid_name(const char *name) {
    if (!name) return 0;
    size_t n = strlen(name);
    if (n < 1 || n > 64) return 0;
    unsigned char c0 = (unsigned char)name[0];
    if (!((c0 >= 'a' && c0 <= 'z') || (c0 >= '0' && c0 <= '9'))) return 0;
    for (size_t i = 1; i < n; i++) {
        unsigned char c = (unsigned char)name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) return 0;
    }
    return 1;
}

/* float(text): accepts surrounding ASCII whitespace and inf/nan spellings. */
static int parse_finite(const char *text, double *out) {
    if (!text) return 0;
    char *end = NULL;
    double v = strtod(text, &end);
    if (end == text) return 0;
    while (*end && isspace((unsigned char)*end)) end++;
    if (*end != '\0') return 0;
    if (!isfinite(v)) return 0;
    *out = v;
    return 1;
}

struct json_t *w17_pluginsidl_dc8b64_build_spec(const char *name,
                                                const char *interval_hours,
                                                const char *min_idle_hours,
                                                const char *first_run) {
    json_t *bad = json_object();
    json_set(bad, "ok", json_bool(false));

    if (!w17_pluginsidl_dc8b64_valid_name(name)) return bad;
    if (!first_run) return bad;
    if (strcmp(first_run, "defer") != 0 && strcmp(first_run, "immediate") != 0) return bad;

    double interval, idle;
    if (!parse_finite(interval_hours, &interval)) return bad;
    if (!parse_finite(min_idle_hours, &idle)) return bad;

    char *n = dupstr(name);
    char *fr = dupstr(first_run);
    if (interval < 1.0) interval = 1.0;   /* MIN_INTERVAL_HOURS floor */
    if (idle < 0.0) idle = 0.0;           /* idle if idle >= 0 else 0.0 */

    json_t *o = json_object();
    json_set(o, "ok", json_bool(true));
    json_set(o, "name", json_string(n ? n : ""));
    json_set(o, "interval", json_number(interval));
    json_set(o, "idle", json_number(idle));
    json_set(o, "first_run", json_string(fr ? fr : ""));
    free(n);
    free(fr);
    return o;
}
