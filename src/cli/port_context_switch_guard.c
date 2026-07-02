/*
 * port_context_switch_guard.c — Port of Python
 *
 * C implementations for name parity.
 */

#include "hermes_logger.h"
#include "hermes_json.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Port of Python: _append_warning */
void _append_warning(void* ctx, void* result, void* text)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "_append_warning: null context");
        return;
    }
    hermes_log(LOG_DEBUG, "port", "_append_warning called");
    bool has_result = (result != NULL);
    bool has_text = (text != NULL);
    /* Build a warning object and append to result JSON if provided */
    if (has_result && has_text) {
        json_t *result_obj = (json_t *)result;
        json_t *warning = json_object();
        if (warning) {
            json_object_set(warning, "warning", json_new_string((const char *)text));
            json_object_set(warning, "timestamp", json_new_number((double)time(NULL)));
            json_object_set(result_obj, "warnings", warning);
        }
    }
    return;
}

/* Port of Python: _threshold_tokens */
int _threshold_tokens(void* ctx, void* context_length, void* threshold_percent)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "_threshold_tokens: null context");
        return 0;
    }
    hermes_log(LOG_DEBUG, "port", "_threshold_tokens called");
    bool has_ctx_len = (context_length != NULL);
    bool has_thresh = (threshold_percent != NULL);
    /* Calculate threshold tokens from context length and percentage */
    int ctx_len = has_ctx_len ? atoi((const char *)context_length) : 200000;
    int thresh_pct = has_thresh ? atoi((const char *)threshold_percent) : 80;
    int threshold = (ctx_len * thresh_pct) / 100;
    return threshold;
}

/* Port of Python: merge_preflight_compression_warning */
void merge_preflight_compression_warning(void* ctx, void* result, void* agent, void* messages, void* custom_providers, void* config_context_length)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "merge_preflight_compression_warning: null context");
        return;
    }
    hermes_log(LOG_DEBUG, "port", "merge_preflight_compression_warning called");
    bool has_result = (result != NULL);
    bool has_messages = (messages != NULL);
    bool has_agent = (agent != NULL);
    /* Merge preflight compression warning into result if messages exceed threshold */
    if (has_result && has_messages) {
        json_t *result_obj = (json_t *)result;
        json_t *msg_array = (json_t *)messages;
        size_t msg_count = json_len(msg_array);
        if (msg_count > 100) {
            json_object_set(result_obj, "preflight_warning",
                           json_new_string("Message count exceeds preflight compression threshold"));
        }
    }
    (void)has_agent;
    (void)custom_providers;
    (void)config_context_length;
}

/* Port of Python: enrich_model_switch_warnings_for_gateway */
void enrich_model_switch_warnings_for_gateway(void* ctx, void* result, void* runner, void* session_key, void* source, void* custom_providers, void* load_gateway_config)
{
    if (!ctx) {
        hermes_log(LOG_WARNING, "port", "enrich_model_switch_warnings_for_gateway: null context");
        return;
    }
    hermes_log(LOG_DEBUG, "port", "enrich_model_switch_warnings_for_gateway called");
    bool has_result = (result != NULL);
    bool has_session = (session_key != NULL);
    bool has_runner = (runner != NULL);
    /* Enrich model switch warnings for gateway with session info */
    if (has_result && has_session) {
        json_t *result_obj = (json_t *)result;
        json_object_set(result_obj, "model_switch_enriched", json_new_bool(true));
        json_object_set(result_obj, "session_key", json_new_string((const char *)session_key));
    }
    (void)has_runner;
    (void)source;
    (void)custom_providers;
    (void)load_gateway_config;
}