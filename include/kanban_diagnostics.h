#ifndef HERMES_KANBAN_DIAGNOSTICS_H
#define HERMES_KANBAN_DIAGNOSTICS_H

/*
 * kanban_diagnostics.h — C11 port of hermes_cli/kanban_diagnostics.py helpers.
 */

#include "json.h"

#ifdef __cplusplus
extern "C" {
#endif

int kanban_severity_at_or_above(const char *severity, const char *threshold);
const json_t *kanban_task_field(const json_t *task, const char *name);
const char *kanban_task_field_str(const json_t *task, const char *name, const char *default_val);
json_t *kanban_parse_payload(const json_t *ev);
const char *kanban_event_kind(const json_t *ev);
long kanban_event_ts(const json_t *ev);
json_t *kanban_active_hallucination_events(const json_t *events, const char *kind);
json_t *kanban_diagnostic_action_to_dict(const char *kind, const char *label, const json_t *payload, int suggested);
json_t *kanban_generic_recovery_actions(const json_t *task, int running);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_KANBAN_DIAGNOSTICS_H */
