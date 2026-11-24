#ifndef NOC_W18_KANBANRESU_D42CF6_H
#define NOC_W18_KANBANRESU_D42CF6_H

#include "hermes_json.h"

/* Faithful C11 port of hermes_cli/kanban_resume_guard.py
 * (gate w18_kanbanresu_d42cf6). `task_value` injects get_session_env(
 * "HERMES_KANBAN_TASK", "") and `mode` selects the classification outcome:
 *   no_probe | probe_true | probe_false | probe_other | probe_none |
 *   probe_raises | persistence_true | persistence_false | persistence_raises
 * The refused reason uses the deterministic seam t(key, session_id=id)
 * -> "BLOCKED:<id>". */

json_t *w18_kanbanresu_d42cf6_dispatcher_owned_run(const char *task_value);
json_t *w18_kanbanresu_d42cf6_kanban_resume_refusal(const char *session_id,
                                                    const char *task_value,
                                                    const char *mode);

#endif /* NOC_W18_KANBANRESU_D42CF6_H */
