#ifndef NOC_W6_EVENTS_756CAD_H
#define NOC_W6_EVENTS_756CAD_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w6_events_756cad_plan_status @ acp_adapter/events.py:_PLAN_STATUS
 * raw is the status string; mirrors str(raw or "pending").strip() followed by
 * _PLAN_STATUS.get(..., "pending") (cancelled -> completed). */
struct json_t *w6_events_756cad_plan_status(const char *raw);

/* PoP: w6_events_756cad_plan_update @ acp_adapter/events.py:_build_plan_update_from_todo_result
 * result is the todo tool's JSON string. Returns a new
 * {"session_update":"plan","entries":[...]} json object or JSON null. */
struct json_t *w6_events_756cad_plan_update(const char *result);

/* PoP: w6_events_756cad_upgrade_queue @ acp_adapter/events.py:_upgrade_queue
 * ids is a JSON object name -> (string | array); a legacy bare string is
 * upgraded in place to a one-element FIFO. Returns the resulting queue as a
 * new JSON array of ids, or JSON null when no queue is present. */
struct json_t *w6_events_756cad_upgrade_queue(const struct json_t *ids, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W6_EVENTS_756CAD_H */
