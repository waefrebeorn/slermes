#ifndef NOC_W15_MODELASSIG_D6FF99_H
#define NOC_W15_MODELASSIG_D6FF99_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* C home for upstream Hermes hermes_cli/model_assignment.py (gate w15_modelassig_d6ff99).
 * All results are freshly allocated json_t values freed by the caller. */

/* PoP: w15_modelassig_d6ff99_same_endpoint @ hermes_cli/model_assignment.py:_same_endpoint */
struct json_t *w15_modelassig_d6ff99_same_endpoint(const struct json_t *a, const struct json_t *b);

/* PoP: w15_modelassig_d6ff99_is_explicit_custom_endpoint @ hermes_cli/model_assignment.py:is_explicit_custom_endpoint */
struct json_t *w15_modelassig_d6ff99_is_explicit_custom_endpoint(const struct json_t *provider,
                                                                const struct json_t *base_url);

/* PoP: w15_modelassig_d6ff99_entry_points_elsewhere @ hermes_cli/model_assignment.py:entry_points_elsewhere */
struct json_t *w15_modelassig_d6ff99_entry_points_elsewhere(const struct json_t *entry,
                                                            const struct json_t *base_url);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_MODELASSIG_D6FF99_H */
