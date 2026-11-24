#ifndef NOC_W18_MAINPROVID_56BCED_H
#define NOC_W18_MAINPROVID_56BCED_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: mainprovid_short_url @ hermes_cli/main_provider_setup.py:_short_url */
char *mainprovid_short_url(const char *url);
/* PoP: mainprovid_aux_effort_word @ hermes_cli/main_provider_setup.py:_aux_effort_word */
char *mainprovid_aux_effort_word(const struct json_t *task_cfg);
/* PoP: mainprovid_format_aux_current @ hermes_cli/main_provider_setup.py:_format_aux_current */
char *mainprovid_format_aux_current(const struct json_t *task_cfg);
/* PoP: mainprovid_delegation_cfg_as_task @ hermes_cli/main_provider_setup.py:_delegation_cfg_as_task */
struct json_t *mainprovid_delegation_cfg_as_task(const struct json_t *cfg);
/* PoP: mainprovid_auto_provider_name @ hermes_cli/main_provider_setup.py:_auto_provider_name */
char *mainprovid_auto_provider_name(const char *base_url);
/* PoP: mainprovid_api_key_config_value @ hermes_cli/main_provider_setup.py:_custom_provider_api_key_config_value */
char *mainprovid_api_key_config_value(const struct json_t *provider_info, const char *resolved_api_key);
/* PoP: mainprovid_base_url_config_value @ hermes_cli/main_provider_setup.py:_custom_provider_base_url_config_value */
char *mainprovid_base_url_config_value(const struct json_t *provider_info, const char *resolved_base_url);
/* PoP: mainprovid_infer_stepfun_region @ hermes_cli/main_provider_setup.py:_infer_stepfun_region */
char *mainprovid_infer_stepfun_region(const char *base_url);

#ifdef __cplusplus
}
#endif

#endif /* NOC_W18_MAINPROVID_56BCED_H */
