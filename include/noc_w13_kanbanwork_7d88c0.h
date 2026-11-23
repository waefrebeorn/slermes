#ifndef NOC_W13_KANBANWORK_7D88C0_H
#define NOC_W13_KANBANWORK_7D88C0_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* All functions accept an optional workflow JSON object
 * {"columns":[{"key","label","icon","drag_target"}...],"manual":{src:[dst...]}};
 * a missing/null workflow selects live.DEFAULT_WORKFLOW. */
/* PoP: w13_kanbanwork_7d88c0_keys @ hermes_cli/kanban_workflow.py:Workflow.keys */
struct json_t *w13_kanbanwork_7d88c0_keys(const struct json_t *workflow);
/* PoP: w13_kanbanwork_7d88c0_column @ hermes_cli/kanban_workflow.py:Workflow.column */
struct json_t *w13_kanbanwork_7d88c0_column(const struct json_t *workflow, const char *key);
/* PoP: w13_kanbanwork_7d88c0_can_move @ hermes_cli/kanban_workflow.py:Workflow.can_move */
struct json_t *w13_kanbanwork_7d88c0_can_move(const struct json_t *workflow,
                                              const char *src, const char *dst);
/* PoP: w13_kanbanwork_7d88c0_to_dict @ hermes_cli/kanban_workflow.py:Workflow.to_dict */
struct json_t *w13_kanbanwork_7d88c0_to_dict(const struct json_t *workflow);
/* PoP: w13_kanbanwork_7d88c0_validate @ hermes_cli/kanban_workflow.py:Workflow.validate */
struct json_t *w13_kanbanwork_7d88c0_validate(const struct json_t *workflow);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W13_KANBANWORK_7D88C0_H */
