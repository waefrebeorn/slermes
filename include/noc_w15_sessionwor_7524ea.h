#ifndef NOC_W15_SESSIONWOR_7524EA_H
#define NOC_W15_SESSIONWOR_7524EA_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w15_sessionwor_7524ea_is_container_path @ tui_gateway/session_workdir.py:_is_container_path */
struct json_t *w15_sessionwor_7524ea_is_container_path(const char *raw);
/* PoP: w15_sessionwor_7524ea_workdir_valid_generation @ tui_gateway/session_workdir.py:_workdir_valid_generation */
struct json_t *w15_sessionwor_7524ea_workdir_valid_generation(const struct json_t *generation);
/* PoP: w15_sessionwor_7524ea_policy_backend @ tui_gateway/session_workdir.py:_policy_backend */
struct json_t *w15_sessionwor_7524ea_policy_backend(const struct json_t *policy);
/* PoP: w15_sessionwor_7524ea_session_source @ tui_gateway/session_workdir.py:_session_source */
struct json_t *w15_sessionwor_7524ea_session_source(const struct json_t *session,
                                                     const char *platform_fallback);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W15_SESSIONWOR_7524EA_H */
