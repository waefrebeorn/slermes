/*
 * process_registry_access.c — faithful C11 port of
 * tools/process_registry_access.py.
 *
 * PoP: w18_processreg_e1509d_session_action_allowed @ tools/process_registry_access.py:session_action_allowed
 * PoP: w18_processreg_e1509d_resolve_actionable @ tools/process_registry_access.py:resolve_actionable
 *
 * The two in-function imports (_owns_result, process_registry, caller_session_key)
 * are injected through `owns_table` / explicit arguments so behaviour is pure.
 */
#include "noc_w18_processreg_e1509d.h"
#include "hermes_json.h"

#include <string.h>

static int pra_nonempty(const char *s) { return s && *s ? 1 : 0; }

/* _owns_result(a, b) over the injected pair table. */
static int pra_owns(const json_t *owns_table, const char *a, const char *b) {
    if (!owns_table || owns_table->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < json_len(owns_table); ++i) {
        json_t *pair = json_get(owns_table, i);
        if (!pair || pair->type != JSON_ARRAY || json_len(pair) < 2) continue;
        json_t *ja = json_get(pair, 0), *jb = json_get(pair, 1);
        const char *sa = (ja && ja->type == JSON_STRING) ? ja->str_val : NULL;
        const char *sb = (jb && jb->type == JSON_STRING) ? jb->str_val : NULL;
        if (sa && sb && !strcmp(sa, a ? a : "") && !strcmp(sb, b ? b : "")) return 1;
    }
    return 0;
}

int w18_processreg_e1509d_session_action_allowed(const char *owner_task_id,
        const char *sess_task_id, const char *sess_session_key,
        const char *task_id, const char *session_key, const json_t *owns_table) {
    /* owner = session.owner_task_id or session.task_id */
    const char *owner = pra_nonempty(owner_task_id) ? owner_task_id : (sess_task_id ? sess_task_id : "");

    if (pra_nonempty(task_id) && !strcmp(task_id, owner)) return 1;
    if (pra_nonempty(session_key) && pra_nonempty(sess_session_key) &&
        !strcmp(session_key, sess_session_key)) return 1;
    if ((pra_nonempty(task_id) && pra_owns(owns_table, task_id, owner)) ||
        (pra_nonempty(session_key) && pra_owns(owns_table, session_key, sess_session_key)))
        return 1;
    return (!pra_nonempty(task_id) && !pra_nonempty(session_key)) ||
           (!pra_nonempty(owner) && !pra_nonempty(sess_session_key));
}

json_t *w18_processreg_e1509d_resolve_actionable(const char *session_id, const char *task_id,
        int exists, const char *owner_task_id, const char *sess_task_id,
        const char *sess_session_key, const char *full_id, const char *caller_key,
        const json_t *owns_table) {
    json_t *res = json_array();
    const char *sid = session_id ? session_id : "";
    const char *tid = task_id ? task_id : "";  /* str(task_id or "") */
    if (!exists) {
        json_append(res, json_bool(0));
        json_append(res, json_string(sid));
        return res;
    }
    int allowed = w18_processreg_e1509d_session_action_allowed(
        owner_task_id, sess_task_id, sess_session_key, tid, caller_key, owns_table);
    if (!allowed) {
        json_append(res, json_bool(0));
        json_append(res, json_string(sid));
        return res;
    }
    json_append(res, json_bool(1));
    json_append(res, json_string(full_id ? full_id : sid));
    return res;
}
