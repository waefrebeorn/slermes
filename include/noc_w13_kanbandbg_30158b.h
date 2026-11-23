#ifndef NOC_W13_KANBANDDBG_30158B_H
#define NOC_W13_KANBANDDBG_30158B_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w13_kanbandbg_30158b_initial_state @ hermes_cli/kanban_db_graph.py:initial_task_state
 * Resolve [status, tenant] from parents/rows; {"error":msg} mirrors a ValueError. */
struct json_t *w13_kanbandbg_30158b_initial_state(const struct json_t *parents,
                                                  const struct json_t *rows,
                                                  const char *initial_status,
                                                  int triage,
                                                  const struct json_t *tenant);
/* PoP: w13_kanbandbg_30158b_validate_graph @ hermes_cli/kanban_db_graph.py:_validate_children_graph
 * DB-free shape + Kahn cycle check; {"ok":true} or {"error":msg}. */
struct json_t *w13_kanbandbg_30158b_validate_graph(const struct json_t *children);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_KANBANDDBG_30158B_H */
