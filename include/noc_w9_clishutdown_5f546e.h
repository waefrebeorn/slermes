#ifndef NOC_W9_CLISHUTDOWN_5F546E_H
#define NOC_W9_CLISHUTDOWN_5F546E_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w9_clishutdown_5f546e_float_env @ hermes_cli/cli_shutdown.py:_float_env
 * float(os.getenv(name, default)) with unparseable -> default. env_value is an
 * explicit injected value (NULL means the variable was unset). */
double w9_clishutdown_5f546e_float_env(const struct json_t *env_value, double def);

/* PoP: w9_clishutdown_5f546e_should_emit_cleanup_session_finalize @ hermes_cli/cli_shutdown.py:_should_emit_cleanup_session_finalize
 * gateway / handed-off / single-query-attempt checks, all injected. */
int w9_clishutdown_5f546e_should_emit_cleanup_session_finalize(const struct json_t *session_id,
                                                              int gateway_owns,
                                                              const struct json_t *handed_off,
                                                              const struct json_t *attempted);

/* PoP: w9_clishutdown_5f546e_oneshot_session @ hermes_cli/cli_shutdown.py:_oneshot_agent_and_session
 * agent.session_id or cli.session_id, returned as JSON string or null. */
struct json_t *w9_clishutdown_5f546e_oneshot_session(const struct json_t *agent_session,
                                                    const struct json_t *cli_session);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W9_CLISHUTDOWN_5F546E_H */
