#ifndef NOC_W15_ROUTEENTRI_26892C_H
#define NOC_W15_ROUTEENTRI_26892C_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w15_routeentri_26892c_sanitize_prompt_pin @ session_rules/route_entries.py:sanitize_prompt_pin
 * Returns a NEW dict (version/context_key/context_prompt/redact_pii/channel_prompt/
 * parent_chat_id) or NULL for python None. Caller frees. */
struct json_t *w15_routeentri_26892c_sanitize_prompt_pin(const struct json_t *pin);

/* PoP: w15_routeentri_26892c_sanitize_model_override @ session_rules/route_entries.py:sanitize_model_override
 * Returns a NEW dict of persistable keys or NULL for python None. Caller frees. */
struct json_t *w15_routeentri_26892c_sanitize_model_override(const struct json_t *override);

/* PoP: w15_routeentri_26892c_is_path_unsafe @ session_rules/route_entries.py:_is_path_unsafe
 * strict=1 is the filename form (any separator rejected); strict=0 allows interior '/'. */
int w15_routeentri_26892c_is_path_unsafe(const struct json_t *value, int strict);

/* PoP: w15_routeentri_26892c_session_key_namespace @ session_rules/route_entries.py:_session_key_namespace
 * Returns a malloc'd string (caller frees). */
char *w15_routeentri_26892c_session_key_namespace(const char *profile);

/* PoP: w15_routeentri_26892c_build_session_key @ session_rules/route_entries.py:build_session_key
 * `source` is a mapping with platform/chat_id/chat_type/user_id/user_id_alt/
 * thread_id/prospective_thread_id/scope_id/shared_session/detached_route.
 * Returns a malloc'd session key (caller frees). */
char *w15_routeentri_26892c_build_session_key(const struct json_t *source,
                                              int group_sessions_per_user,
                                              int thread_sessions_per_user,
                                              const char *profile);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W15_ROUTEENTRI_26892C_H */
