#ifndef NOC_W11_UPDATEFINI_FEFA40_H
#define NOC_W11_UPDATEFINI_FEFA40_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w11_updatefini_fefa40_restore_plan @ hermes_cli/update_finish.py:_restore_plan
 * data: the request's plan dict (or null/empty -> Python None).
 * plan_fields / rt_fields are injected dataclass field-name lists (UpdatePlan /
 * RuntimeRecord) so the field-selection seams stay explicit.
 * Returns the restored values dict, with "runtimes" filtered to rt_fields. */
struct json_t *w11_updatefini_fefa40_restore_plan(const struct json_t *data,
                                                   const struct json_t *plan_fields,
                                                   const struct json_t *rt_fields);
/* PoP: w11_updatefini_fefa40_tail_owed @ hermes_cli/update_finish.py:finish_update
 * any(step in TAIL_FOLLOWUPS for step, _ in owed); followups / tail list injected. */
int w11_updatefini_fefa40_tail_owed(const struct json_t *owed,
                                    const struct json_t *tail_followups);
/* PoP: w11_updatefini_fefa40_resume_handled @ hermes_cli/update_finish.py:main
 * attempted or not token or not token.get("resume_needed"). */
int w11_updatefini_fefa40_resume_handled(int attempted, const struct json_t *token);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W11_UPDATEFINI_FEFA40_H */
