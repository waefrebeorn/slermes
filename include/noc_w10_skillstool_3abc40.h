#ifndef NOC_W10_SKILLSTOOL_3ABC40_H
#define NOC_W10_SKILLSTOOL_3ABC40_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Pure readiness/secret helpers of tools/skills_tool_setup.py.
 * Env/plugin lookups are injected through explicit seams instead of touching
 * the process environment (Windows CRT setenv("") unsets rather than blanks). */
json_t *w10_skillstool_3abc40_is_remote_env_backend(const char *backend);
json_t *w10_skillstool_3abc40_as_dict_list(const json_t *raw);
json_t *w10_skillstool_3abc40_clean_str(const json_t *value);
json_t *w10_skillstool_3abc40_get_required_environment_variables(const json_t *frontmatter);
json_t *w10_skillstool_3abc40_capture_result(const json_t *missing_names, int setup_skipped, const json_t *gateway_setup_hint);
json_t *w10_skillstool_3abc40_build_setup_note(const char *readiness_status, const json_t *missing, const json_t *setup_help);
json_t *w10_skillstool_3abc40_is_env_var_persisted(const char *var_name, const json_t *env_snapshot, const json_t *secret_value);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W10_SKILLSTOOL_3ABC40_H */
