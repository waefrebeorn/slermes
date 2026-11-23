#ifndef NOC_W8_SESSIONSHEL_FE6651_H
#define NOC_W8_SESSIONSHEL_FE6651_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Faithful C11 port of the pure decision surface in gateway/session_shell.py. */

/* PoP: w8_sessionshel_fe6651_timeout_seconds @ gateway/session_shell.py:TIMEOUT_SECONDS */
struct json_t *w8_sessionshel_fe6651_timeout_seconds(void);

/* PoP: w8_sessionshel_fe6651_local_backends @ gateway/session_shell.py:_LOCAL_BACKENDS */
struct json_t *w8_sessionshel_fe6651_local_backends(void);

/* PoP: w8_sessionshel_fe6651_handler_names @ gateway/session_shell.py:handlers */
struct json_t *w8_sessionshel_fe6651_handler_names(void);

/* PoP: w8_sessionshel_fe6651_params_valid @ gateway/session_shell.py:shell_exec
 * Exact inline validation: params keys subset of {session_id, profile, command},
 * ref.session_id a non-empty str, command a str with non-blank strip(). */
struct json_t *w8_sessionshel_fe6651_params_valid(const struct json_t *params,
        const struct json_t *session_id, const struct json_t *command);

/* PoP: w8_sessionshel_fe6651_backend_is_local @ gateway/session_shell.py:shell_exec
 * str(value or '').strip().lower() in { '', 'local' }. */
struct json_t *w8_sessionshel_fe6651_backend_is_local(const struct json_t *terminal_env);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W8_SESSIONSHEL_FE6651_H */
