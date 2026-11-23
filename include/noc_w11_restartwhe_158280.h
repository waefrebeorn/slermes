#ifndef NOC_W11_RESTARTWHE_158280_H
#define NOC_W11_RESTARTWHE_158280_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w11_restartwhe_158280_log_view @ gateway/restart_when_idle.py:_log_view
 * Returns a malloc'd string: kinds/ids of the work units, or "nothing".
 * `units` is a JSON array whose elements may be any JSON type (non-dicts are
 * skipped). str() coercion, truthiness filtering and key order match Python.
 * Caller frees. */
char *w11_restartwhe_158280_log_view(const struct json_t *units);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W11_RESTARTWHE_158280_H */
