#ifndef NOC_W13_PLUGINSUPD_A7336F_H
#define NOC_W13_PLUGINSUPD_A7336F_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w13_pluginsupd_a7336f_https_update_url @ hermes_cli/plugins_updates.py:https_update_url */
struct json_t *w13_pluginsupd_a7336f_https_update_url(const struct json_t *in);
/* PoP: w13_pluginsupd_a7336f_version_is_newer @ hermes_cli/plugins_updates.py:_version_is_newer */
struct json_t *w13_pluginsupd_a7336f_version_is_newer(const struct json_t *in);
/* PoP: w13_pluginsupd_a7336f_check_result_json @ hermes_cli/plugins_updates.py:CheckResult.to_json */
struct json_t *w13_pluginsupd_a7336f_check_result_json(const struct json_t *in);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W13_PLUGINSUPD_A7336F_H */
