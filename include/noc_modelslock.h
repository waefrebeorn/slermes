#ifndef NOC_MODELSLOCK_H
#define NOC_MODELSLOCK_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: modelslock_provider_models_cache_lock @ hermes_cli/models_cache_lock.py:provider_models_cache_lock
 * The lock spec derived from `path`: the sibling .lock path, the 10s timeout
 * and the exact timeout message. Paths are normalized to Windows separators. */
struct json_t *modelslock_lock_spec(const char *path);

/* PoP: modelslock_write_under_cache_lock @ hermes_cli/models_cache_lock.py:write_under_cache_lock
 * `raise_kind` selects what the injected write()/lock does:
 *   none | write_oserror | write_timeouterror | write_valueerror
 *   lock_oserror | lock_timeouterror | lock_valueerror
 * Returns {"write_called": bool, "outcome": "written"|"skipped"|"propagated"}. */
struct json_t *modelslock_write_under_lock(const char *raise_kind);

#ifdef __cplusplus
}
#endif

#endif /* NOC_MODELSLOCK_H */
