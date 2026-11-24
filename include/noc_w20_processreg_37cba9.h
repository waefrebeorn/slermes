/*
 * noc_w20_processreg_37cba9.h -- public C API for the port of upstream Hermes
 *   tools/process_registry_handoff.py (gate w20_processreg_37cba9).
 *
 * Ported: the whole decision ladder of handoff_process -- parent liveness, the
 * parent owner id, the per-child handoff cap, the note requirement, the
 * transfer outcome and every message it returns. The three imports it makes
 * (process_registry.transfer_ownership, delegate_tool_registry._active_subagents,
 * terminal_tool._resolve_container_task_id) are the seams and are supplied as
 * `world`.
 */
#ifndef NOC_W20_PROCESSREG_37CBA9_H
#define NOC_W20_PROCESSREG_37CBA9_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* _MAX_HANDOFFS_PER_CHILD (3). */
int w20_processreg_37cba9_max_handoffs_per_child(void);

/* The transfer decision. `world` mirrors the live spawn tree:
 *   has_child               -- str(task_id or "") is in _active_subagents
 *   parent_live             -- the child's _delegate_parent_ref() is not None
 *   parent_current_task_id  -- parent._current_task_id ("" for unset)
 *   parent_session_id       -- parent.session_id ("" for unset)
 *   handed_before           -- len(child._handed_off_processes)
 *   transfer_ok             -- process_registry.transfer_ownership(...) not None
 *   transfer_session_id     -- session.id   (used when transfer_ok)
 *   transfer_command        -- session.command
 * Returns exactly what the Python returns: {"error": "..."} or the handed_off
 * receipt. */
json_t *w20_processreg_37cba9_handoff(const char *session_id, const json_t *args,
                                      const char *task_id, const json_t *world);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W20_PROCESSREG_37CBA9_H */
