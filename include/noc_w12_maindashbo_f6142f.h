#ifndef NOC_W12_MAIN_DASHBO_F6142F_H
#define NOC_W12_MAIN_DASHBO_F6142F_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w12_maindashbo_f6142f_dashboard_probe_host @ hermes_cli/main_dashboard.py:_dashboard_probe_host */
char *w12_maindashbo_f6142f_dashboard_probe_host(const char *host);

/* PoP: w12_maindashbo_f6142f_extract_scope_from_cgroup @ hermes_cli/main_dashboard.py:_extract_scope_from_cgroup */
char *w12_maindashbo_f6142f_extract_scope_from_cgroup(const char *entry);

/* PoP: w12_maindashbo_f6142f_systemd_restart_commands @ hermes_cli/main_dashboard.py:_systemd_restart_commands */
struct json_t *w12_maindashbo_f6142f_systemd_restart_commands(const char *unit, const char *scope);

/* PoP: w12_maindashbo_f6142f_format_restart_commands @ hermes_cli/main_dashboard.py:_format_restart_commands */
char *w12_maindashbo_f6142f_format_restart_commands(const struct json_t *commands);

/* PoP: w12_maindashbo_f6142f_explicit_endpoint_flags @ hermes_cli/main_dashboard.py:_explicit_endpoint_flags */
struct json_t *w12_maindashbo_f6142f_explicit_endpoint_flags(const struct json_t *argv);

/* PoP: w12_maindashbo_f6142f_is_electron_packaged_web_dist @ hermes_cli/main_dashboard.py:_is_electron_packaged_web_dist */
int w12_maindashbo_f6142f_is_electron_packaged_web_dist(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W12_MAIN_DASHBO_F6142F_H */
