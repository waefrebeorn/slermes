/* tools_config_providers.h — faithful C11 port of the provider-aware
 * configuration surface of hermes_cli/tools_config.py:
 * plugin provider catalogs, _visible_providers, installed-state probes,
 * needs-configuration gate, provider-activity checks, model pickers
 * (imagegen / videogen / STT), xAI storage prompts, _write_provider_config,
 * _configure_provider, vision backend pickers and simple env requirements.
 *
 * TOOL_CATEGORIES / TOOLSET_ENV_REQUIREMENTS have no static C table; they are
 * carried as JSON documents registered at startup (see
 * tools_config_register_tool_categories). A missing entry mirrors Python's
 * dict.get(...) → None paths.
 */
#ifndef SLERMES_TOOLS_CONFIG_PROVIDERS_H
#define SLERMES_TOOLS_CONFIG_PROVIDERS_H

#include <stdbool.h>
#include <stddef.h>
#include "hermes_json.h"

/* ── module-level data registries ──────────────────────────────────────── */

/* Register the TOOL_CATEGORIES analog (json object ts_key → category row).
 * Borrowed pointer; embedders own the lifetime. */
void tools_config_register_tool_categories(json_t *categories);
json_t *tools_config_tool_categories(void);

/* Register the TOOLSET_ENV_REQUIREMENTS analog
 * (json object ts_key → array of [var, url] pairs). Borrowed pointer. */
void tools_config_register_toolset_env_requirements(json_t *reqs);
json_t *tools_config_toolset_env_requirements(void);

/* get_nous_subscription_features(config, force_fresh) analog. The returned
 * document is shaped {nous_auth_present, account_info{logged_in,
 * paid_service_access, tool_gateway_entitled}, features{<name>:{available,
 * managed_by_nous}}}. Caller json_free()s. NULL when portal state is
 * unavailable (mirrors the exception path). */
json_t *tools_config_nous_subscription_features(const json_t *config,
                                                bool force_fresh);

/* ── plugin provider catalogs ──────────────────────────────────────────── */

json_t *tools_config_plugin_image_gen_providers(void);
json_t *tools_config_plugin_video_gen_providers(void);
json_t *tools_config_plugin_web_search_providers(void);
json_t *tools_config_plugin_browser_providers(void);
json_t *tools_config_plugin_tts_providers(void);

/* PoP: _visible_providers — returns a fresh array of provider rows. */
json_t *tools_config_visible_providers(const json_t *cat, const json_t *config,
                                       bool force_fresh,
                                       const json_t *features);

/* ── installed-state probes ────────────────────────────────────────────── */

bool tools_config_agent_browser_installed(void);
bool tools_config_camofox_installed(void);
bool tools_config_cloud_agent_browser_installed(void);

/* ── configuration gates ───────────────────────────────────────────────── */

bool tools_config_toolset_needs_configuration_prompt(const char *ts_key,
                                                     const json_t *config,
                                                     bool force_fresh);
void tools_config_configure_tool_category(const char *ts_key,
                                          const json_t *cat,
                                          json_t *config, bool force_fresh);
bool tools_config_web_tier_matches(const json_t *provider,
                                   const json_t *config);
bool tools_config_is_provider_active(const json_t *provider,
                                     const json_t *config, bool force_fresh,
                                     const json_t *features);

/* ── model pickers ─────────────────────────────────────────────────────── */

/* _fal_model_catalog (port lives beside the FAL_MODELS table in
 * src/tools/image_generation_tool.c). */
bool tools_config_fal_model_catalog(json_t **catalog, char **default_model);

void tools_config_configure_imagegen_model(const char *backend_name,
                                           json_t *config);
json_t *tools_config_plugin_image_gen_catalog(const char *plugin_name,
                                              char **default_model);
void tools_config_configure_imagegen_model_for_plugin(const char *plugin_name,
                                                      json_t *config);
void tools_config_configure_xai_imagine_storage(const char *section_name,
                                                json_t *config);
void tools_config_select_plugin_image_gen_provider(const char *plugin_name,
                                                   json_t *config,
                                                   bool use_gateway);
json_t *tools_config_plugin_video_gen_catalog(const char *plugin_name,
                                              char **default_model);
void tools_config_configure_videogen_model_for_plugin(const char *plugin_name,
                                                      json_t *config);
void tools_config_configure_stt_model(const char *stt_provider, json_t *config);
void tools_config_select_plugin_video_gen_provider(const char *plugin_name,
                                                   json_t *config,
                                                   bool use_gateway);

/* ── config writing + configure flow ───────────────────────────────────── */

void tools_config_write_provider_config(const json_t *provider, json_t *config,
                                        const char *managed_feature);
void tools_config_apply_provider_selection(const char *ts_key,
                                           const char *provider_name,
                                           json_t *config);
void tools_config_configure_provider(const json_t *provider, json_t *config,
                                     bool force_fresh);
void tools_config_configure_vision_backend(void);
void tools_config_configure_vision_provider_model(json_t *config,
                                                  json_t *vision_cfg);
void tools_config_configure_simple_requirements(const char *ts_key);

#endif /* SLERMES_TOOLS_CONFIG_PROVIDERS_H */
