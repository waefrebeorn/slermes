/* tools_config_more.h — additional C11 ports from hermes_cli/tools_config.py */

#ifndef HERMES_TOOLS_CONFIG_MORE_H
#define HERMES_TOOLS_CONFIG_MORE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── hermes_cli/tools_config.py additional ports ───────────────────── */

/* PoP: _camofox_installed @ hermes_cli/tools_config.py:_camofox_installed
 * True when the Camofox npm package is already in node_modules. */
bool tools_config_camofox_installed(void);

/* PoP: _cloud_agent_browser_installed @ hermes_cli/tools_config.py:_cloud_agent_browser_installed
 * Installed-check for the browserbase hook (cloud provider rows). */
bool tools_config_cloud_agent_browser_installed(void);

/* PoP: _web_tier_matches @ hermes_cli/tools_config.py:_web_tier_matches
 * Return True when a web picker row's tier matches the configured tier.
 * web_backend: the provider's web_backend name
 * web_tier: the row's web_tier (or NULL for tier-agnostic)
 * config_web_backend: the configured web.backend value (or NULL)
 * config_provider_tier: the configured web.provider_tier.<backend> value (or NULL)
 */
bool tools_config_web_tier_matches(const char *web_backend,
                                    const char *web_tier,
                                    const char *config_web_backend,
                                    const char *config_provider_tier);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_TOOLS_CONFIG_MORE_H */
