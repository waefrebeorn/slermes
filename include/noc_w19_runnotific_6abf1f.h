/* noc_w19_runnotific_6abf1f.h - public C API for gateway/run_notifications_update.py. */
#ifndef NOC_W19_RUNNOTIFIC_6ABF1F_H
#define NOC_W19_RUNNOTIFIC_6ABF1F_H
#include "hermes_json.h"
#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w19_runnotific_6abf1f_output_tail @ gateway/run_notifications_update.py:_update_output_tail
 * Last `limit` CHARACTERS of an update log, prefixed with U+2026 when cut. */
json_t *w19_runnotific_6abf1f_output_tail(const char *output, long long limit);

/* PoP: w19_runnotific_6abf1f_marker_profile @ gateway/run_notifications_update.py:GatewayUpdateNotificationsMixin._marker_profile */
json_t *w19_runnotific_6abf1f_marker_profile(json_t *data);

/* PoP: w19_runnotific_6abf1f_marker_age @ gateway/run_notifications_update.py:GatewayUpdateNotificationsMixin._marker_age_seconds
 * `now_iso` is the injected clock (datetime.now(...) seam); null when the stamp is absent/unparseable. */
json_t *w19_runnotific_6abf1f_marker_age(json_t *data, const char *now_iso);

/* PoP: w19_runnotific_6abf1f_read_since @ gateway/run_notifications_update.py:GatewayUpdateNotificationsMixin._read_update_output_since
 * `data` is the file's text; the offset counts BYTES of its UTF-8 encoding.
 * -> {"text": str, "offset": int} */
json_t *w19_runnotific_6abf1f_read_since(const char *data, long long offset);

/* PoP: w19_runnotific_6abf1f_update_paths @ gateway/run_notifications_update.py:GatewayUpdateNotificationsMixin._update_paths
 * -> {"pending","claimed","output","exit_code","prompt","response"} as str(Path) renders them. */
json_t *w19_runnotific_6abf1f_update_paths(const char *hermes_home);

/* PoP: w19_runnotific_6abf1f_update_exit_code @ gateway/run_notifications_update.py:GatewayUpdateNotificationsMixin._update_exit_code
 * int(text.strip() or "1"); null where int() would raise (the watcher only sees numeric files). */
json_t *w19_runnotific_6abf1f_update_exit_code(const char *text);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W19_RUNNOTIFIC_6ABF1F_H */
