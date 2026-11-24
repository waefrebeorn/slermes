#ifndef NOC_W17_PLUGINSIDL_DC8B64_H
#define NOC_W17_PLUGINSIDL_DC8B64_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w17_pluginsidl_dc8b64_base_state @ hermes_cli/plugins_idle_tasks.py:_base_state */
struct json_t *w17_pluginsidl_dc8b64_base_state(const char *plugin_id, const char *name);

/* PoP: w17_pluginsidl_dc8b64_valid_name @ hermes_cli/plugins_idle_tasks.py:_NAME_RE */
int w17_pluginsidl_dc8b64_valid_name(const char *name);

/* PoP: w17_pluginsidl_dc8b64_build_spec @ hermes_cli/plugins_idle_tasks.py:build_idle_task_spec
 * Returns {"ok":true,name,interval,idle,first_run} on success, {"ok":false} on ValueError.
 * interval_hours/min_idle_hours are decimal strings (float() inputs). */
struct json_t *w17_pluginsidl_dc8b64_build_spec(const char *name,
                                                const char *interval_hours,
                                                const char *min_idle_hours,
                                                const char *first_run);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W17_PLUGINSIDL_DC8B64_H */
