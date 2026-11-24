#ifndef NOC_W17_CONFIGENV_64FB4F_H
#define NOC_W17_CONFIGENV_64FB4F_H

#include "hermes_json.h"

#ifdef __cplusplus
extern "C" {
#endif

/* PoP port of hermes_cli/config_env_routing.py (gate w17_configenv_64fb4f).
 * The config.yaml/.env routing decision is driven by the key SHAPE, with the
 * Hermes registry sets injected from the fixture. */
json_t *w17_configenv_64fb4f_is_registered_env_name(const char *name, const json_t *registry);
json_t *w17_configenv_64fb4f_is_env_setting_key(const char *key, const json_t *registry);

#ifdef __cplusplus
}
#endif
#endif /* NOC_W17_CONFIGENV_64FB4F_H */
