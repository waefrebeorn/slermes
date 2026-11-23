#ifndef NOC_W13_PLUGINVALI_6A7B64_H
#define NOC_W13_PLUGINVALI_6A7B64_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w13_pluginvali_6a7b64_declared_list @ hermes_cli/plugin_validate.py:_declared_list */
struct json_t *w13_pluginvali_6a7b64_declared_list(const struct json_t *in);

/* PoP: w13_pluginvali_6a7b64_requires_hermes_spec_valid @ hermes_cli/plugin_validate.py:_requires_hermes_spec_valid */
struct json_t *w13_pluginvali_6a7b64_requires_hermes_spec_valid(const struct json_t *in);

/* PoP: w13_pluginvali_6a7b64_check_requires_env @ hermes_cli/plugin_validate.py:_check_requires_env */
struct json_t *w13_pluginvali_6a7b64_check_requires_env(const struct json_t *in);

/* PoP: w13_pluginvali_6a7b64_validation_report @ hermes_cli/plugin_validate.py:ValidationReport */
struct json_t *w13_pluginvali_6a7b64_validation_report(const struct json_t *in);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W13_PLUGINVALI_6A7B64_H */
