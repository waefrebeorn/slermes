/*
 * session_local_close.c — faithful C11 port of upstream
 * gateway/session_local_close.py.
 *
 * The picker's view of a canonical local session its last viewer closed, as
 * main listed it. A live row reads as closed when it is idle and unviewed and
 * (detached / a history mount / its target's ended_at is stamped). _local_target
 * swallows AuthorityRefusal/KeyError/TypeError -> None, so a malformed or API
 * binding reads as "nothing to close". Runtime collaborators are injected as
 * data through the seam parameters.
 *
 * PoP: sessclose_closed_idle_unviewed @ gateway/session_local_close.py:closed_idle_unviewed
 * PoP: sessclose_closed_and_unviewed  @ gateway/session_local_close.py:closed_and_unviewed
 */
#include "hermes_json.h"
#include <string.h>
#include "noc_sessclose.h"

/* Python truthiness for the scalar/container JSON values used by this module. */
static int truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0] ? 1 : 0;
    return v->c.count ? 1 : 0;
}

static int is_idle(const char *state) { return state && !strcmp(state, "idle"); }

struct json_t *sessclose_closed_idle_unviewed(const json_t *live,
                                              const char *execution_state,
                                              const json_t *target_ended_at,
                                              int history_mount) {
    /* if live is None or live.subscribers or execution_state != 'idle': False */
    if (!live || live->type == JSON_NULL) return json_bool(false);
    if (truthy(json_obj_get(live, "subscribers"))) return json_bool(false);
    if (!is_idle(execution_state)) return json_bool(false);

    /* if live.detached or is_history_mount(live): True */
    if (truthy(json_obj_get(live, "detached")) || history_mount) return json_bool(true);

    /* return target_ended_at is not None */
    return json_bool(target_ended_at && target_ended_at->type != JSON_NULL);
}

struct json_t *sessclose_closed_and_unviewed(const json_t *live,
                                             const char *execution_state,
                                             int history_mount,
                                             const char *target,
                                             int receipt_raises, int no_row,
                                             const json_t *row_ended_at) {
    /* live = authority.sessions.get(session_id)
     * if live is None or live.subscribers or handle.execution_state != 'idle': False */
    if (!live || live->type == JSON_NULL) return json_bool(false);
    if (truthy(json_obj_get(live, "subscribers"))) return json_bool(false);
    if (!is_idle(execution_state)) return json_bool(false);

    /* if live.detached or is_history_mount(live): True */
    if (truthy(json_obj_get(live, "detached")) || history_mount) return json_bool(true);

    /* target = _local_target(...) -> None on AuthorityRefusal/KeyError/TypeError */
    const char *local_target = receipt_raises ? NULL : target;
    /* row = ...list_row(target) if target else None */
    const json_t *ended = (local_target && local_target[0] && !no_row) ? row_ended_at : NULL;
    /* return closed_idle_unviewed(live, handle.execution_state, row and row.ended_at) */
    return sessclose_closed_idle_unviewed(live, execution_state, ended, history_mount);
}
