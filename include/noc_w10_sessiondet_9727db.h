#ifndef NOC_W10_SESSIONDET_9727DB_H
#define NOC_W10_SESSIONDET_9727DB_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w10_sessiondet_9727db_detached_key @ gateway/session_detached.py:detached_key */
struct json_t *w10_sessiondet_9727db_detached_key(const char *route, const char *session_id);
/* PoP: w10_sessiondet_9727db_detached_session_id @ gateway/session_detached.py:detached_session_id */
struct json_t *w10_sessiondet_9727db_detached_session_id(const char *key);
/* PoP: w10_sessiondet_9727db_own_detached_key @ gateway/session_detached.py:own_detached_key */
struct json_t *w10_sessiondet_9727db_own_detached_key(const char *key, const char *session_id);
/* PoP: w10_sessiondet_9727db_detached_route_of @ gateway/session_detached.py:detached_route_of */
struct json_t *w10_sessiondet_9727db_detached_route_of(const struct json_t *source);
/* PoP: w10_sessiondet_9727db_running_key @ gateway/session_detached.py:running_key */
struct json_t *w10_sessiondet_9727db_running_key(const struct json_t *running, const char *route, const char *session_id);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W10_SESSIONDET_9727DB_H */
