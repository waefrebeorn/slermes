/*
 * quiet_single_query.c — C11 port of upstream Hermes hermes_cli/quiet_single_query.py.
 *
 * PoP: w17_quietsingl_e79bf7_take_report_path  @ hermes_cli/quiet_single_query.py:take_turn_report_path
 * PoP: w17_quietsingl_e79bf7_exit_trailer      @ hermes_cli/quiet_single_query.py:exit_single_query
 * PoP: w17_quietsingl_e79bf7_diagnostic_muted  @ hermes_cli/quiet_single_query.py:_diagnostic_only_wake_muted
 *
 * All three are pure over their explicit inputs (no process env, no subprocess,
 * no filesystem). The oracle injects the two imported predicates as stubs and
 * calls the LIVE functions for comparison.
 */
#include "noc_w17_quietsingl_e79bf7.h"
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Module-level literal constants mirrored from the Python source. */
static const char *const KANBAN_WORKER_EXIT_TRAILER = "[kanban-worker-exit] rc=";
static const char *const TURN_REPORT_FILE_ENV = "HERMES_QUIET_TURN_REPORT_FILE";

struct json_t *w17_quietsingl_e79bf7_take_report_path(const struct json_t *envmap) {
    json_t *remaining = json_object();
    if (envmap && envmap->type == JSON_OBJECT) {
        for (size_t i = 0; i < json_object_size(envmap); i++) {
            const char *k = json_object_get_key_at(envmap, i);
            if (k) json_set(remaining, k, json_copy(json_object_get_at(envmap, i)));
        }
    }

    json_t *value = json_null();
    const json_t *found = remaining ? json_obj_get(remaining, TURN_REPORT_FILE_ENV) : NULL;
    if (found && found->type == JSON_STRING && found->str_val && found->str_val[0])
        value = json_copy(found);   /* environ.pop(...,None) or None */
    if (found) json_obj_del(remaining, TURN_REPORT_FILE_ENV);

    json_t *out = json_object();
    json_set(out, "value", value);
    json_set(out, "remaining", remaining);
    return out;
}

struct json_t *w17_quietsingl_e79bf7_exit_trailer(const char *code) {
    long long c = code ? strtoll(code, NULL, 10) : 0;
    char buf[128];
    snprintf(buf, sizeof(buf), "\n%s%lld", KANBAN_WORKER_EXIT_TRAILER, c);
    return json_string(buf);
}

int w17_quietsingl_e79bf7_diagnostic_muted(const struct json_t *events, int warnings_enabled) {
    if (!events || events->type != JSON_ARRAY || json_len(events) == 0) return 0;  /* not events -> False */

    int all_diag = 1;
    for (size_t i = 0; i < json_len(events); i++) {
        const json_t *e = json_get(events, i);
        if (!e || e->type != JSON_OBJECT) continue;   /* generator: only dicts are tested */
        const json_t *d = json_obj_get(e, "diagnostic");
        int diag = 0;
        if (d) {
            if (d->type == JSON_BOOL) diag = d->bool_val ? 1 : 0;
            else if (d->type == JSON_NUMBER) diag = d->num_val != 0.0;
            else if (d->type == JSON_STRING) diag = d->str_val && d->str_val[0] ? 1 : 0;
        }
        if (!diag) { all_diag = 0; break; }
    }
    if (!all_diag) return 0;
    return warnings_enabled ? 0 : 1;   /* return not warning_notifications_enabled("cli") */
}
