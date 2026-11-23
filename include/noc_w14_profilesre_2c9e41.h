#ifndef NOC_W14_PROFILESRE_2C9E41_H
#define NOC_W14_PROFILESRE_2C9E41_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w14_profilesre_2c9e41_make_writable_actions @ hermes_cli/profiles_remove.py:_rmtree_make_writable
 * Model the onexc/onerror handler: whether it re-raises, which paths get +w,
 * and whether func(path) is invoked. Returns a JSON object. */
struct json_t *w14_profilesre_2c9e41_make_writable_actions(const struct json_t *in);

/* PoP: w14_profilesre_2c9e41_rmtree_retry_actions @ hermes_cli/profiles_remove.py:_rmtree_with_retry
 * Model the 3-attempt rmtree retry loop with scripted per-attempt outcomes.
 * Returns {"result","rmtree_calls","exists_calls","sleeps"}. */
struct json_t *w14_profilesre_2c9e41_rmtree_retry_actions(const struct json_t *in);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W14_PROFILESRE_2C9E41_H */
