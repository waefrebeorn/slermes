/*
 * noc_w21_register_55dd31.h -- public C API for the port of upstream
 *   plugins/platforms/qqbot/register.py
 *
 * The plugin's declarative surface: the connectivity probe every server uses
 * (`is_connected`), the ``config_connected_fn`` it registers, the
 * ``PlatformEntry`` metadata table (name/label/emoji/docs/env/install hint/
 * standalone-send policy) and the cached system-prompt platform hint.
 *
 * ``setup_qqbot`` (prompts, env writes, QR onboarding) and ``register`` itself
 * (needs a live PluginContext and the mautrix-free adapter) stay upstream.
 */
#ifndef NOC_W21_REGISTER_55DD31_H
#define NOC_W21_REGISTER_55DD31_H

#include "hermes_json.h"

/*
 * is_connected(extra, env_app_id, env_client_secret) -> bool
 *
 *   extra            : PlatformConfig.extra (may be NULL / missing keys)
 *   env_app_id       : profile-resolved QQ_APP_ID value ("" when unset)
 *   env_client_secret: profile-resolved QQ_CLIENT_SECRET value
 *
 * Both `extra` values (config.yaml) must be non-blank after str().strip(), OR
 * both env values -- exactly `str(extra.get(k) or "").strip() and _env_configured(pconfig)`.
 */
int w21_register_55dd31_is_connected(const json_t *extra,
                                     const char *env_app_id,
                                     const char *env_client_secret);

/* config_connected_extra("app_id", "client_secret"): token/api_key OR both extras. */
int w21_register_55dd31_config_connected(const json_t *extra,
                                         const json_t *token,
                                         const json_t *api_key);

/* _PLATFORM_HINT -- cached-prompt bytes, keep stable. */
const char *w21_register_55dd31_platform_hint(void);

/*
 * The PlatformEntry table register() declares (string/bool fields only; the
 * adapter factories, setup_fn and gateway_env AUTHZ bridge are the live seams):
 *   name, label, emoji, docs_url, dashboard_description, required_env,
 *   dashboard_env_vars, install_hint, cron_deliver_env_var, platform_hint,
 *   standalone_send{label,media,captionable,text_direct}
 */
json_t *w21_register_55dd31_register_meta(void);

#endif /* NOC_W21_REGISTER_55DD31_H */
