#ifndef NOC_W9_DOCTORTOOLS_317C87_H
#define NOC_W9_DOCTORTOOLS_317C87_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w9_doctortools_317c87_plural @ hermes_cli/doctor_tools.py:_plural */
struct json_t *w9_doctortools_317c87_plural(int n);

/* PoP: w9_doctortools_317c87_setup_gated @ hermes_cli/doctor_tools.py:_setup_gated */
int w9_doctortools_317c87_setup_gated(const struct json_t *item);

/* PoP: w9_doctortools_317c87_kanban_env_gate @ hermes_cli/doctor_tools.py:_is_kanban_worker_env_gate
 * env_value is injected from the fixture instead of reading os.environ. */
int w9_doctortools_317c87_kanban_env_gate(const struct json_t *item, const struct json_t *env_value);

/* PoP: w9_doctortools_317c87_tool_availability_detail @ hermes_cli/doctor_tools.py:_doctor_tool_availability_detail */
struct json_t *w9_doctortools_317c87_tool_availability_detail(const char *toolset,
                                                              const struct json_t *env_value);

/* PoP: w9_doctortools_317c87_apply_overrides @ hermes_cli/doctor_tools.py:_apply_doctor_tool_availability_overrides */
struct json_t *w9_doctortools_317c87_apply_overrides(const struct json_t *available,
                                                     const struct json_t *unavailable,
                                                     const struct json_t *env_value);

/* PoP: w9_doctortools_317c87_missing_api_key_toolsets @ hermes_cli/doctor_tools.py:_missing_api_key_toolsets_for_summary
 * enabled=null means config resolution failed (no filtering), as in Python. */
struct json_t *w9_doctortools_317c87_missing_api_key_toolsets(const struct json_t *unavailable,
                                                              const struct json_t *enabled);

/* PoP: w9_doctortools_317c87_safe_which @ hermes_cli/doctor_tools.py:_safe_which
 * path_map replaces shutil.which with a deterministic fixture lookup. */
struct json_t *w9_doctortools_317c87_safe_which(const char *cmd, const struct json_t *path_map);

/* PoP: w9_doctortools_317c87_chromium_not_needed_reason @ hermes_cli/doctor_tools.py:_chromium_not_needed_reason
 * config and cdp_env are injected instead of calling load_config_readonly/os.environ. */
struct json_t *w9_doctortools_317c87_chromium_not_needed_reason(const struct json_t *config,
                                                                const struct json_t *cdp_env);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W9_DOCTORTOOLS_317C87_H */
