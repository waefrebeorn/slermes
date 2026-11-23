#ifndef NOC_W13_PLUGINSCAD_FD67F1_H
#define NOC_W13_PLUGINSCAD_FD67F1_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w13_pluginscad_fd67f1_install_fingerprint @ hermes_cli/plugins_cadence.py:_install_fingerprint */
struct json_t *w13_pluginscad_fd67f1_install_fingerprint(const struct json_t *in);
/* PoP: w13_pluginscad_fd67f1_check_interval_hours @ hermes_cli/plugins_cadence.py:check_interval_hours */
struct json_t *w13_pluginscad_fd67f1_check_interval_hours(const struct json_t *in);
/* PoP: w13_pluginscad_fd67f1_auto_apply_enabled @ hermes_cli/plugins_cadence.py:auto_apply_enabled */
struct json_t *w13_pluginscad_fd67f1_auto_apply_enabled(const struct json_t *in);
/* PoP: w13_pluginscad_fd67f1_cached_git_updates @ hermes_cli/plugins_cadence.py:cached_git_updates */
struct json_t *w13_pluginscad_fd67f1_cached_git_updates(const struct json_t *in);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W13_PLUGINSCAD_FD67F1_H */
