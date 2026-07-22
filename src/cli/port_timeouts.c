/*
 * port_timeouts.c — pure helpers ported from hermes_cli/timeouts.py.
 * Self-contained; reuses libjson. The two get_provider_* functions are ported
 * to take an explicit config JSON string (the Python originals call
 * load_config_readonly(); in C we pass the config dict in, which is the
 * faithful adaptation — same config-shape lookup, no side-effecting load).
 *
 *   - _coerce_timeout          -> timeouts_coerce_timeout
 *   - _get_model_config         -> timeouts_get_model_config (writes into out_*)
 *   - get_provider_request_timeout -> timeouts_get_provider_request_timeout
 *   - get_provider_stale_timeout  -> timeouts_get_provider_stale_timeout
 */

#include "timeouts_helpers.h"
#include "libjson/json.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* PoP: _coerce_timeout @ hermes_cli/timeouts.py:_coerce_timeout */
/* Coerce a raw timeout value to a positive float, else None(-1). */
/* PoP: timeouts_coerce_timeout @ hermes_cli/timeouts.py:_coerce_timeout */
double timeouts_coerce_timeout(const char *raw)
{
    if (!raw) return -1.0;
    char *end = NULL;
    /* reject obvious non-numeric garbage (e.g. "None", empty) */
    if (*raw == '\0') return -1.0;
    double v = strtod(raw, &end);
    if (end == raw) return -1.0;            /* no digits consumed */
    if (v <= 0.0) return -1.0;           /* Python: timeout <= 0 -> None */
    return v;
}

/* PoP: _get_model_config @ hermes_cli/timeouts.py:_get_model_config */
/* Given a provider_config JSON object + optional model name, return the model's
 * config object (borrowed from config), or NULL if absent. Does NOT free. */
/* PoP: timeouts_get_model_config @ hermes_cli/timeouts.py:_get_model_config */
const json_t *timeouts_get_model_config(const json_t *provider_config,
                                       const char *model)
{
    if (!provider_config || provider_config->type != JSON_OBJECT) return NULL;
    if (!model || !*model) return NULL;
    const json_t *models = json_obj_get(provider_config, "models");
    if (!models || models->type != JSON_OBJECT) return NULL;
    const json_t *mc = json_obj_get(models, model);
    if (!mc || mc->type != JSON_OBJECT) return NULL;
    return mc;
}

/* Shared body for the two get_provider_* functions. stale==0 -> request
 * timeout; stale==1 -> stale timeout. */
static double get_provider_timeout(const char *config_json,
                                  const char *provider_id,
                                  const char *model,
                                  int stale)
{
    if (!provider_id || !*provider_id) return -1.0;
    if (!config_json) return -1.0;

    char *err = NULL;
    json_t *config = json_parse(config_json, &err);
    if (err) { free(err); return -1.0; }
    if (!config || config->type != JSON_OBJECT) { if (config) json_free(config); return -1.0; }

    const json_t *providers = json_obj_get(config, "providers");
    if (!providers || providers->type != JSON_OBJECT) { json_free(config); return -1.0; }
    const json_t *pc = json_obj_get(providers, provider_id);
    if (!pc || pc->type != JSON_OBJECT) { json_free(config); return -1.0; }

    /* model-specific takes precedence */
    const json_t *mcp = timeouts_get_model_config(pc, model);
    if (mcp) {
        const char *key = stale ? "stale_timeout_seconds" : "timeout_seconds";
        const json_t *tv = json_obj_get(mcp, key);
        if (tv && (tv->type == JSON_NUMBER || tv->type == JSON_STRING)) {
            char buf[64];
            double got = -1.0;
            if (tv->type == JSON_NUMBER) got = tv->num_val;
            else { snprintf(buf, sizeof buf, "%s", tv->str_val); got = timeouts_coerce_timeout(buf); }
            json_free(config);
            return got;
        }
    }

    const char *pkey = stale ? "stale_timeout_seconds" : "request_timeout_seconds";
    const json_t *pv = json_obj_get(pc, pkey);
    if (pv && (pv->type == JSON_NUMBER || pv->type == JSON_STRING)) {
        double got = -1.0;
        if (pv->type == JSON_NUMBER) got = pv->num_val;
        else { char buf[64]; snprintf(buf, sizeof buf, "%s", pv->str_val); got = timeouts_coerce_timeout(buf); }
        json_free(config);
        return got;
    }

    json_free(config);
    return -1.0;
}

/* PoP: get_provider_request_timeout @ hermes_cli/timeouts.py:get_provider_request_timeout */
double timeouts_get_provider_request_timeout(const char *config_json,
                                            const char *provider_id,
                                            const char *model)
{
    return get_provider_timeout(config_json, provider_id, model, 0);
}

/* PoP: get_provider_stale_timeout @ hermes_cli/timeouts.py:get_provider_stale_timeout */
double timeouts_get_provider_stale_timeout(const char *config_json,
                                          const char *provider_id,
                                          const char *model)
{
    return get_provider_timeout(config_json, provider_id, model, 1);
}
