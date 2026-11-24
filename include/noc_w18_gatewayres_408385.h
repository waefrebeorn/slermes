#ifndef NOC_W18_GATEWAYRES_408385_H
#define NOC_W18_GATEWAYRES_408385_H

#include "hermes_json.h"

/* Faithful C11 port of hermes_cli/gateway_restart_env.py
 * (gate w18_gatewayres_408385). Late-imported helpers are injected as explicit
 * seams: `resolved_home` is str(get_profile_dir(name)), `supplied` is the union
 * of .env / dotenv / source names, `adapter_secrets` the allowed secret set,
 * `supervised` the is_supervised_gateway_launch() answer. */

json_t *w18_gatewayres_408385_argv_profile_home(const json_t *argv, const char *resolved_home);
json_t *w18_gatewayres_408385_launch_bot_token_names(const json_t *supplied,
                                                      const json_t *adapter_secrets,
                                                      int supervised);
json_t *w18_gatewayres_408385_host_watcher_env(const json_t *env);

#endif /* NOC_W18_GATEWAYRES_408385_H */
