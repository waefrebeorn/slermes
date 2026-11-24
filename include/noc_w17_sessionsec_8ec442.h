#ifndef NOC_W17_SESSIONSEC_8EC442_H
#define NOC_W17_SESSIONSEC_8EC442_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w17_sessionsec_8ec442_skipped @ gateway/session_secret_prompt.py:_skipped */
struct json_t *w17_sessionsec_8ec442_skipped(const char *var_name, const char *reason,
                                            const char *message);
/* PoP: w17_sessionsec_8ec442_callback_result @ gateway/session_secret_prompt.py:secret_prompt_scope.<locals>.callback
 * Scripted seams: live_session = whether authority.sessions.get(session_id) returns a live row;
 * ask_value = the viewer's answer (NULL/"" means empty -> cancelled); saved = the dict returned
 * by save_env_value_secure(var_name, value). */
struct json_t *w17_sessionsec_8ec442_callback_result(const char *var_name, const char *prompt,
                                    int has_metadata, const char *metadata, int live_session,
                                    const char *ask_value, const struct json_t *saved);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W17_SESSIONSEC_8EC442_H */
