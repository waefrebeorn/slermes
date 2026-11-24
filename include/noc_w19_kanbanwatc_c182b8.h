/* noc_w19_kanbanwatc_c182b8.h - public C API for gateway/kanban_watchers_dispatcher.py. */
#ifndef NOC_W19_KANBANWATC_C182B8_H
#define NOC_W19_KANBANWATC_C182B8_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_kanbanwatc_c182b8_resolve_settings @ gateway/kanban_watchers_dispatcher.py:_resolve_dispatcher_settings
 * asdict() of the returned _DispatcherSettings (interval, max_spawn, max_in_progress, failure_limit,
 * stale_timeout_seconds, reconcile_orphans, default_assignee, max_in_progress_per_profile).
 * effective_max_in_progress models _kbd().resolve_max_in_progress(max_in_progress). */
json_t *w19_kanbanwatc_c182b8_resolve_settings(json_t *kanban_cfg,
                                               long long default_failure_limit,
                                               json_t *effective_max_in_progress);

/* PoP: w19_kanbanwatc_c182b8_is_corrupt_error @ gateway/kanban_watchers_dispatcher.py:_KanbanDispatcher.is_corrupt_board_db_error */
int w19_kanbanwatc_c182b8_is_corrupt_error(int kanban_corrupt, int sqlite_db_error,
                                           const char *msg);

/* PoP: w19_kanbanwatc_c182b8_quarantine_lifted @ gateway/kanban_watchers_dispatcher.py:_KanbanDispatcher._quarantine_lifted
 * -> {"lifted": bool, "log": "none"|"unchanged"|"changed", "entry_after": bool} */
json_t *w19_kanbanwatc_c182b8_quarantine_lifted(int has_entry, json_t *disabled_fp,
                                               double disabled_at, json_t *fingerprint,
                                               double now_mono);

/* PoP: w19_kanbanwatc_c182b8_log_spawn_results @ gateway/kanban_watchers_dispatcher.py:_log_spawn_results
 * -> {"any_spawned": bool} */
json_t *w19_kanbanwatc_c182b8_log_spawn_results(json_t *results);

/* PoP: w19_kanbanwatc_c182b8_board_slugs @ gateway/kanban_watchers_dispatcher.py:_KanbanDispatcher._board_slugs
 * (delegates to kanban_watchers_common._board_slugs; modeled from the enumerated boards). */
json_t *w19_kanbanwatc_c182b8_board_slugs(json_t *boards, const char *default_board);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_KANBANWATC_C182B8_H */
