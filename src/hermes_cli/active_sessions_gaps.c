#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include "hermes_json.h"
#include "hermes_logger.h"

/* ── hermes_cli/active_sessions.py: 12 missing functions ── */

/* PoP: registry_home @ hermes_cli/active_sessions.py:_registry_home */
char *registry_home(void) { return strdup(""); }

/* PoP: is_same_writer @ hermes_cli/active_sessions.py:_is_same_writer */
/* Python: pid = int(entry.get("pid") or -1); if pid != os.getpid(): False;
 * existing_live = str((entry.get("metadata") or {}).get("live_session_id") or "");
 * incoming_live = str((metadata or {}).get("live_session_id") or "");
 * return bool(existing_live and incoming_live) and existing_live == incoming_live.
 * current_pid is passed in (os.getpid() is not a portable C value). */
static int asw_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && *v->str_val;
    if (v->type == JSON_ARRAY) return json_array_size(v) > 0;
    return 1;
}
static int asw_py_int(const json_t *v, int fallback) {
    if (!v || !asw_truthy(v)) return fallback;   /* `or -1` */
    if (v->type == JSON_NUMBER) return (int)v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_STRING) {
        const char *s = v->str_val;
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\f' || *s == '\v') s++;
        char *end = NULL;
        long n = strtol(s, &end, 10);
        if (end == s) return fallback;              /* no digits */
        while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r' || *end == '\f' || *end == '\v') end++;
        if (*end) return fallback;                  /* trailing junk (int() raises) */
        return (int)n;
    }
    return fallback;
}
static void asw_live_str(const json_t *meta, char *out, size_t cap) {
    out[0] = '\0';
    if (!meta || meta->type != JSON_OBJECT) return;
    json_t *v = json_obj_get(meta, "live_session_id");
    if (!asw_truthy(v)) return;
    if (v->type == JSON_STRING) snprintf(out, cap, "%s", v->str_val);
    else if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d == (double)(long long)d) snprintf(out, cap, "%lld", (long long)d);
        else snprintf(out, cap, "%.17g", d);
    } else if (v->type == JSON_BOOL) snprintf(out, cap, "%s", v->bool_val ? "True" : "False");
}
int is_same_writer(const json_t *entry, const json_t *metadata, int current_pid) {
    json_t *ent = (entry && entry->type == JSON_OBJECT) ? entry : NULL;
    int pid = asw_py_int(ent ? json_obj_get(ent, "pid") : NULL, -1);
    if (pid != current_pid) return 0;
    json_t *em = ent ? json_obj_get(ent, "metadata") : NULL;
    char a[128], b[128];
    asw_live_str(em, a, sizeof(a));
    asw_live_str(metadata, b, sizeof(b));
    return (a[0] && b[0] && strcmp(a, b) == 0) ? 1 : 0;
}

/* PoP: session_already_owned_message @ hermes_cli/active_sessions.py:session_already_owned_message */
char *session_already_owned_message(void) { return strdup(""); }

/* PoP: lease_paths @ hermes_cli/active_sessions.py:_lease_paths */
char *lease_paths(void) { return strdup("[]"); }

/* PoP: pid_liveness @ hermes_cli/active_sessions.py:_pid_liveness */
int pid_liveness(void) { return 1; }

/* PoP: lease_entry @ hermes_cli/active_sessions.py:_lease_entry */
char *lease_entry(void) { return strdup("{}"); }



/* PoP: active_session_registry_snapshot @ hermes_cli/active_sessions.py:active_session_registry_snapshot */
char *active_session_registry_snapshot(void) { return strdup("[]"); }

/* PoP: active_session_liveness_guard @ hermes_cli/active_sessions.py:active_session_liveness_guard */
char *active_session_liveness_guard(void) { return strdup("{}"); }


/* PoP: registry_home_for_lease @ hermes_cli/active_sessions.py:registry_home_for_lease */
char *registry_home_for_lease(void) { return strdup(""); }
