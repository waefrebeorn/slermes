#ifndef NOC_TGDRAFT_H
#define NOC_TGDRAFT_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: tgdraft_claim_ai_action @ plugins/platforms/telegram/telegram_draft_budget.py:claim_ai_action
 * now + calls[] are the adapter state/loop-time seam; new_calls_out receives the
 * resulting deque (caller frees). Returns the skip delay (0.0 == slot claimed). */
double tgdraft_claim_ai_action_ex(double now, const double *calls, size_t ncalls,
                                  struct json_t **new_calls_out);
#ifdef __cplusplus
}
#endif
#endif /* NOC_TGDRAFT_H */
