#ifndef NOC_W14_HOSTEDTASK_5005E2_H
#define NOC_W14_HOSTEDTASK_5005E2_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w14_hostedtask_5005e2_validate_task_input @ session_rules/hosted_tasks.py:validate_task_input
 * Returns {"watermark":int,"event_seqs":[int,...]} or {"error":msg}. */
struct json_t *w14_hostedtask_5005e2_validate_task_input(const struct json_t *value);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_HOSTEDTASK_5005E2_H */
