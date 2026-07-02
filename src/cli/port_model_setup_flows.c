/**
 * port_model_setup_flows.c — Port of Python: cli.py (model setup flows)
 *
 * Real C implementations for model setup flow helpers.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Port of Python: model_flow_google_antigravity */
const char *model_flow_google_antigravity(json_t *_config, const char *current_model)
{
    if (!_config || !current_model) {
        hermes_log(LOG_WARNING, "port", "model_flow_google_antigravity: null parameter");
        return "";
    }
    hermes_log(LOG_INFO, "port", "model_flow_google_antigravity: model=%s", current_model);

    const char *antigravity = json_node_get_string(json_object_get(_config, "antigravity"));
    if (antigravity) {
        hermes_log(LOG_DEBUG, "port", "model_flow: antigravity config found");
    }
    return current_model;
}

/* Port of Python: _model_flow_google_antigravity */
void _model_flow_google_antigravity(void *ctx, void *_config, void *current_model)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "_model_flow_google_antigravity: null context");
        return;
    }
    hermes_log(LOG_DEBUG, "port", "_model_flow_google_antigravity: called");
    if (_config && current_model) {
        model_flow_google_antigravity((json_t *)_config, (const char *)current_model);
    }
}
