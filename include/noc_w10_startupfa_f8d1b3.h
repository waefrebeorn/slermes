#ifndef NOC_W10_STARTUPFA_F8D1B3_H
#define NOC_W10_STARTUPFA_F8D1B3_H
struct json_t;
#ifdef __cplusplus
extern "C" {
#endif
/* PoP: w10_startupfa_f8d1b3_is_global_fast_version_argv @ hermes_cli/_startup_fast.py:is_global_fast_version_argv
 * argv is a JSON array of strings; returns JSON bool. */
struct json_t *w10_startupfa_f8d1b3_is_global_fast_version_argv(const struct json_t *argv);
/* PoP: w10_startupfa_f8d1b3_is_desktop_ssh_backend_argv @ hermes_cli/_startup_fast.py:is_desktop_ssh_backend_argv
 * argv is a JSON array of strings; returns JSON bool. */
struct json_t *w10_startupfa_f8d1b3_is_desktop_ssh_backend_argv(const struct json_t *argv);
/* PoP: w10_startupfa_f8d1b3_active_profile_may_override_home @ hermes_cli/_startup_fast.py:active_profile_may_override_home
 * active_text is an explicit seam for the _read_text(active_profile) probe; returns JSON bool. */
struct json_t *w10_startupfa_f8d1b3_active_profile_may_override_home(const char *active_text);
/* PoP: w10_startupfa_f8d1b3_normalize_hermes_home_env @ hermes_cli/_startup_fast.py:normalize_hermes_home_env
 * raw/env are explicit seams for os.environ: expandvars then expanduser once, write back only
 * when the result differs from raw. Returns the expanded JSON string, or JSON null if unchanged. */
struct json_t *w10_startupfa_f8d1b3_normalize_hermes_home_env(const struct json_t *raw,
                                                              const struct json_t *env);
/* PoP: w10_startupfa_f8d1b3_read_install_method @ hermes_cli/_startup_fast.py:read_install_method
 * text is an explicit seam for the .install_method stamp read; returns stripped/lower string or null. */
struct json_t *w10_startupfa_f8d1b3_read_install_method(const char *text);
#ifdef __cplusplus
}
#endif
#endif /* NOC_W10_STARTUPFA_F8D1B3_H */
