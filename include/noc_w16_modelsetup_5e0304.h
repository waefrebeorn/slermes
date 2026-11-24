#ifndef NOC_W16_MODELSETUP_5E0304_H
#define NOC_W16_MODELSETUP_5E0304_H

struct json_t;

#ifdef __cplusplus
extern "C" {
#endif

/* PoP: w16_modelsetup_5e0304_nous_login_args @ hermes_cli/model_setup_flows.py:_nous_login_args */
struct json_t *w16_modelsetup_5e0304_nous_login_args(const struct json_t *args);
/* PoP: w16_modelsetup_5e0304_select_zai_endpoint @ hermes_cli/model_setup_flows.py:_select_zai_endpoint */
struct json_t *w16_modelsetup_5e0304_select_zai_endpoint(const char *current_base,
                                                         const struct json_t *selected,
                                                         const char *override);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W16_MODELSETUP_5E0304_H */
