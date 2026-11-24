#ifndef NOC_W18_PROCESSREG_E1509D_H
#define NOC_W18_PROCESSREG_E1509D_H

#include "hermes_json.h"

/* C home for upstream Hermes tools/process_registry_access.py.
 * `owns_table` is a JSON array of [a, b] pairs for which _owns_result(a,b) is true. */
int     w18_processreg_e1509d_session_action_allowed(const char *owner_task_id,
            const char *sess_task_id, const char *sess_session_key,
            const char *task_id, const char *session_key, const json_t *owns_table);
json_t *w18_processreg_e1509d_resolve_actionable(const char *session_id, const char *task_id,
            int exists, const char *owner_task_id, const char *sess_task_id,
            const char *sess_session_key, const char *full_id, const char *caller_key,
            const json_t *owns_table);

#endif /* NOC_W18_PROCESSREG_E1509D_H */
