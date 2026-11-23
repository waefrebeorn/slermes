#ifndef NOC_W10_PLANSNAPSH_242594_H
#define NOC_W10_PLANSNAPSH_242594_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w10_plansnapsh_242594_snapshot_full_ids @ agent/plan_snapshot_sync.py:_snapshot_full_ids
 * Ids of full allowances in the cached window snapshot; NULL when it has no window plan.
 * When present, a sorted+unique JSON string array is returned (caller frees). */
struct json_t *w10_plansnapsh_242594_snapshot_full_ids(const struct json_t *snapshot);

/* PoP: w10_plansnapsh_242594_plan_snapshot_is_stale @ agent/plan_snapshot_sync.py:plan_snapshot_is_stale */
int w10_plansnapsh_242594_plan_snapshot_is_stale(const struct json_t *state, const struct json_t *snapshot);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W10_PLANSNAPSH_242594_H */
