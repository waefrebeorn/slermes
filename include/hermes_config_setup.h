/* Self-contained public API. No god headers — opaque types via core_types only.
 * C11 only.
 */
#ifndef SLERMES_CONFIG_SETUP_H
#define SLERMES_CONFIG_SETUP_H

#include "hermes_core_types.h"

/* CLI config-setup helpers (ports of hermes_cli/setup.py) */
char *setup_gateway_platform_short_label(const char *label);
bool config_setup_supports_same_provider_pool_setup(const char *provider);
bool setup_check_espeak_ng(void);
bool setup_xai_oauth_logged_in(void);
void setup_model_config_dict(const hermes_config_t *cfg, char *out_default, size_t out_size);
const char *setup_current_reasoning_effort(void);
bool setup_model_section_has_credentials(const hermes_config_t *cfg, const char *provider);

#endif /* SLERMES_CONFIG_SETUP_H */
