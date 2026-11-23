#ifndef NOC_W7_MOAALTERAT_E7A67F_H
#define NOC_W7_MOAALTERAT_E7A67F_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: moaalt_destination_key @ agent/moa_alternation.py:destination_key
 * Returns ["(route, model)"] as a two-element JSON array. */
struct json_t *moaalt_destination_key(const struct json_t *runtime);

/* PoP: moaalt_merge_same_role_messages @ agent/moa_alternation.py:merge_same_role_messages */
struct json_t *moaalt_merge_same_role_messages(const struct json_t *messages);

/* PoP: moaalt_is_role_alternation_rejection @ agent/moa_alternation.py:is_role_alternation_rejection
 * 'classified' is the boolean seam result of classify_api_error(...).reason
 * is FailoverReason.role_alternation. */
int moaalt_is_role_alternation_rejection(int classified, const struct json_t *runtime);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W7_MOAALTERAT_E7A67F_H */
