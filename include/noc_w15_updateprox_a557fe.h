#ifndef NOC_W15_UPDATEPROX_A557FE_H
#define NOC_W15_UPDATEPROX_A557FE_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w15_updateprox_a557fe_no_proxy_entries @ hermes_cli/update_proxy_env.py:agent.proxy_bypass.no_proxy_entries
 * Returns a JSON array of the comma/whitespace-separated entries of value. */
struct json_t *w15_updateprox_a557fe_no_proxy_entries(const char *value);

/* PoP: w15_updateprox_a557fe_restore_user_proxy_env @ hermes_cli/update_proxy_env.py:restore_user_proxy_env
 * Mutates and returns env. `published` maps env-var names (case-insensitive) to
 * arrays of dotenv-published values, the injected seam for dotenv_published_values(). */
struct json_t *w15_updateprox_a557fe_restore_user_proxy_env(struct json_t *env,
                                                           const struct json_t *published);

/* PoP: w15_updateprox_a557fe_prefer_dotenv_proxy_env @ hermes_cli/update_proxy_env.py:prefer_dotenv_proxy_env
 * Mutates and returns env; `published` is the same injected dotenv seam. */
struct json_t *w15_updateprox_a557fe_prefer_dotenv_proxy_env(struct json_t *env,
                                                            const struct json_t *published);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_UPDATEPROX_A557FE_H */
