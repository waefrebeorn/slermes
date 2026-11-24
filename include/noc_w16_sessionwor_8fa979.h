#ifndef NOC_W16_SESSIONWOR_8FA979_H
#define NOC_W16_SESSIONWOR_8FA979_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w16_sessionwor_8fa979_limit @ gateway/session_worker_child.py:_limit */
long long w16_sessionwor_8fa979_limit(const struct json_t *section, const char *key);
/* PoP: w16_sessionwor_8fa979_root_execution_id @ gateway/session_worker_child.py:delegation_limits */
struct json_t *w16_sessionwor_8fa979_root_execution_id(const char *execution_id);
/* PoP: w16_sessionwor_8fa979_is_child @ gateway/session_worker_child.py:delegation_limits */
int w16_sessionwor_8fa979_is_child(const char *execution_id);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_SESSIONWOR_8FA979_H */
