#ifndef NOC_W10_INSTALLSTA_F7E5F6_H
#define NOC_W10_INSTALLSTA_F7E5F6_H
#include "hermes_json.h"

/* PoP port of hermes_cli/install_state.py — "has this OS user used Hermes?" probes. */
/* _unit_account: last ^\s*User=\s*(\S+)\s*$ (MULTILINE) capture, or None. */
json_t *w10_installsta_f7e5f6_unit_account(const char *unit);
/* Pure per-root checks fed a parsed config.yaml / auth store mapping. */
int w10_installsta_f7e5f6_onboarding_done(json_t *config);
int w10_installsta_f7e5f6_provider_config(json_t *config);
int w10_installsta_f7e5f6_provider_login(json_t *auth_stores);
#endif /* NOC_W10_INSTALLSTA_F7E5F6_H */
