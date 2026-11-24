/*
 * kanban_resume_guard.c — faithful C11 port of hermes_cli/kanban_resume_guard.py
 * (gate w18_kanbanresu_d42cf6).
 *
 * PoP: w18_kanbanresu_d42cf6_dispatcher_owned_run @ hermes_cli/kanban_resume_guard.py:_dispatcher_owned_run
 * PoP: w18_kanbanresu_d42cf6_kanban_resume_refusal @ hermes_cli/kanban_resume_guard.py:kanban_resume_refusal
 */
#include "noc_w18_kanbanresu_d42cf6.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* bool(str(get_session_env("HERMES_KANBAN_TASK", "") or "").strip()) */
static int dispatcher_owned(const char *task_value) {
    const char *p = task_value ? task_value : "";
    while (*p && isspace((unsigned char)*p)) p++;
    while (*p && !isspace((unsigned char)*p)) {
        if (*p) return 1;
        p++;
    }
    /* trailing-only whitespace: skip it and see if anything is left */
    while (*p && isspace((unsigned char)*p)) p++;
    return *p ? 1 : 0;
}

json_t *w18_kanbanresu_d42cf6_dispatcher_owned_run(const char *task_value) {
    return json_bool(dispatcher_owned(task_value) ? 1 : 0);
}

static json_t *blocked(const char *session_id) {
    size_t n = strlen(session_id ? session_id : "");
    char *buf = (char *)malloc(n + 9);
    json_t *r;
    if (!buf) return json_null();
    memcpy(buf, "BLOCKED:", 8);
    memcpy(buf + 8, session_id ? session_id : "", n);
    buf[8 + n] = '\0';
    r = json_string(buf);
    free(buf);
    return r;
}

/* The guard's fail-open decision tree. A positive REAL True is the only
 * refusal; every other classification (including probe errors) proceeds. */
json_t *w18_kanbanresu_d42cf6_kanban_resume_refusal(const char *session_id,
                                                    const char *task_value,
                                                    const char *mode) {
    if (!session_id || !*session_id) return json_null();
    if (dispatcher_owned(task_value)) return json_null();
    if (!mode) return json_null();
    if (!strcmp(mode, "probe_true") || !strcmp(mode, "persistence_true"))
        return blocked(session_id);
    /* probe_false / probe_other / probe_none / probe_raises / no_probe /
     * persistence_false / persistence_raises all classify as unclassified. */
    return json_null();
}

