#ifndef NOC_W10_SESSIONAPI_8E7979_H
#define NOC_W10_SESSIONAPI_8E7979_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w10_sessionapi_8e7979_valid_owner_scope @ gateway/session_api_turn.py:_valid_owner_scope */
struct json_t *w10_sessionapi_8e7979_valid_owner_scope(const struct json_t *value);
/* PoP: w10_sessionapi_8e7979_owns_api_run @ gateway/session_api_turn.py:owns_api_run */
struct json_t *w10_sessionapi_8e7979_owns_api_run(const struct json_t *owner_scope,
                                                 int run_raise,
                                                 const struct json_t *stored_scope);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W10_SESSIONAPI_8E7979_H */
