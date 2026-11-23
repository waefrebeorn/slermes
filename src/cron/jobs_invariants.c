/*
 * jobs_invariants.c — real C11 port of upstream Hermes cron/jobs_invariants.py.
 *
 * PoP: jobsinvariants_script_is_command_line @ cron/jobs_invariants.py:_script_is_command_line
 * PoP: jobsinvariants_mode_error @ cron/jobs_invariants.py:_validate_job_mode_invariants
 * PoP: jobsinvariants_schedule_error @ cron/jobs_invariants.py:validate_schedule_knobs
 *
 * display_hermes_home, _resolve_script_path and the two jobs_time normalizers
 * are injected collaborators; the three functions above run the live module's
 * actual branch order and message construction in C.
 */
#include "noc_w7_jobsinvaria_ba9c41.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *jstr(const json_t *in, const char *key) {
    json_t *v = json_obj_get(in, key);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

int jobsinvariants_script_is_command_line(const char *script, const char *script_path,
                                           int script_is_file) {
    const char *p;
    if (!script) return 0;
    p = script;
    while (*p && isspace((unsigned char)*p)) p++;          /* script.strip() */
    {
        size_t n = strlen(p);
        while (n && isspace((unsigned char)p[n - 1])) n--;
        {
            size_t i;
            int has_ws = 0;
            for (i = 0; i < n; i++)
                if (isspace((unsigned char)p[i])) { has_ws = 1; break; }
            if (!has_ws) return 0;                          /* no whitespace -> not a command line */
        }
        /* _resolve_script_path returns None -> command line; a non-file path
         * is likewise a command line. */
        if (!script_path) return 1;
        return script_is_file ? 0 : 1;
    }
}

/* Python repr() for the ASCII script/kind strings the fixtures use. */
static char *py_repr(const char *s) {
    size_t n = s ? strlen(s) : 0, i, pos = 0;
    char *out = (char *)malloc(n * 2 + 3);
    if (!out) return NULL;
    out[pos++] = '\'';
    for (i = 0; i < n; i++) {
        char c = s[i];
        if (c == '\\' || c == '\'') { out[pos++] = '\\'; out[pos++] = c; }
        else if (c == '\t') { out[pos++] = '\\'; out[pos++] = 't'; }
        else if (c == '\n') { out[pos++] = '\\'; out[pos++] = 'n'; }
        else if (c == '\r') { out[pos++] = '\\'; out[pos++] = 'r'; }
        else if (c == '\b') { out[pos++] = '\\'; out[pos++] = 'b'; }
        else if (c == '\f') { out[pos++] = '\\'; out[pos++] = 'f'; }
        else out[pos++] = c;
    }
    out[pos++] = '\'';
    out[pos] = '\0';
    return out;
}

json_t *jobsinvariants_mode_error(const json_t *in) {
    const char *monitor_script = jstr(in, "monitor_script");
    const char *monitor_url = jstr(in, "monitor_url");
    const char *script = jstr(in, "script");
    int no_agent = json_get_bool(in, "no_agent", 0) ? 1 : 0;
    const char *home = jstr(in, "home");
    const char *script_path = jstr(in, "script_path");
    int script_is_file = json_get_bool(in, "script_is_file", 0) ? 1 : 0;
    char msg[1024];

    if (monitor_script && monitor_url)
        return json_string("monitor_script and monitor_url are mutually exclusive — a job "
                           "can only have one monitor source.");
    if ((monitor_script || monitor_url) && no_agent)
        return json_string("monitor_script/monitor_url cannot be combined with no_agent=True — "
                           "the whole point of a monitor job is to suppress or wake the AGENT "
                           "based on source changes. Use a plain no_agent script job instead.");
    if (no_agent && !(script && *script))
        return json_string("no_agent=True requires a script — with no agent and no script "
                           "there is nothing for the job to run.");
    if (script && *script && jobsinvariants_script_is_command_line(script, script_path, script_is_file)) {
        char *repr = py_repr(script);
        snprintf(msg, sizeof(msg),
                 "script must name a file under %s/scripts/ (e.g. 'watchdog.sh'), not a shell "
                 "command line: %s has no such file. Write the command into a script there and "
                 "pass its filename.",
                 home ? home : "/home/test/.hermes", repr ? repr : "''");
        free(repr);
        return json_string(msg);
    }
    return json_string("");
}

json_t *jobsinvariants_schedule_error(const json_t *in) {
    int tz_error = json_get_bool(in, "tz_error", 0) ? 1 : 0;
    int grace_error = json_get_bool(in, "grace_error", 0) ? 1 : 0;
    json_t *schedule = json_obj_get(in, "schedule");
    const char *kind;
    char msg[512];
    char *repr;

    if (tz_error) return json_string("timezone error");
    if (grace_error) return json_string("grace error");
    if (json_get_bool(in, "grace_none", 0)) return json_string("");  /* normalize_grace_minutes -> None */
    kind = (schedule && schedule->type == JSON_OBJECT) ? jstr(schedule, "kind") : NULL;
    if (!kind || strcmp(kind, "cron")) {
        repr = kind ? py_repr(kind) : strdup("None");
        snprintf(msg, sizeof(msg),
                 "grace_minutes only applies to cron schedules (a fixed wall clock that can be "
                 "missed); this job's schedule kind is %s.",
                 repr ? repr : "None");
        free(repr);
        return json_string(msg);
    }
    return json_string("");
}

