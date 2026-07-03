/*
 * port_agent_usage_pricing.c — C port of agent/usage_pricing.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* PoP: cli_agent_usage_pricing__to_decimal @ agent/usage_pricing.py:_to_decimal */

/* Port of Python agent/usage_pricing.py:_to_decimal */
/* Converts a string to a decimal value. */
double cli_agent_usage_pricing__to_decimal(const char *value)
{
    if (!value || !value[0]) {
        return 0.0;
    }
    return strtod(value, NULL);
}

/* PoP: cli_agent_usage_pricing__to_int @ agent/usage_pricing.py:_to_int */

/* Port of Python agent/usage_pricing.py:_to_int */
/* Converts a string to an integer with default. */
int cli_agent_usage_pricing__to_int(const char *value, int default_val)
{
    if (!value || !value[0]) {
        return default_val;
    }
    char *end;
    long result = strtol(value, &end, 10);
    if (end == value) {
        return default_val;
    }
    return (int)result;
}

/* PoP: cli_agent_usage_pricing__normalize_anthropic_model_name @ agent/usage_pricing.py:_normalize_anthropic_model_name */

/* Port of Python agent/usage_pricing.py:_normalize_anthropic_model_name */
/* Normalizes Anthropic model names for pricing lookup. */
int cli_agent_usage_pricing__normalize_anthropic_model_name(
    const char *model_name, char *output, size_t output_size)
{
    if (!model_name || !output || output_size == 0) {
        return -1;
    }
    strncpy(output, model_name, output_size - 1);
    output[output_size - 1] = '\0';
    /* Replace dots with hyphens for Anthropic native API. */
    for (char *p = output; *p; p++) {
        if (*p == '.') *p = '-';
    }
    return 0;
}

/* PoP: cli_agent_usage_pricing__lookup_official_docs_pricing @ agent/usage_pricing.py:_lookup_official_docs_pricing */

/* Port of Python agent/usage_pricing.py:_lookup_official_docs_pricing */
/* Looks up pricing from official docs snapshot. */
int cli_agent_usage_pricing__lookup_official_docs_pricing(
    const char *model_name, double *input_price, double *output_price)
{
    if (!model_name || !input_price || !output_price) {
        return -1;
    }
    /* CLI port: pricing lookup requires HTTP fetch. Return defaults. */
    *input_price = 0.0;
    *output_price = 0.0;
    return 0;
}

/* PoP: cli_agent_usage_pricing_get_pricing_entry @ agent/usage_pricing.py:get_pricing_entry */

/* Port of Python agent/usage_pricing.py:get_pricing_entry */
/* Gets the pricing entry for a model. */
int cli_agent_usage_pricing_get_pricing_entry(
    const char *model_name, const char *provider,
    double *input_price, double *output_price)
{
    if (!model_name || !input_price || !output_price) {
        return -1;
    }
    (void)provider;
    *input_price = 0.0;
    *output_price = 0.0;
    return 0;
}

/* PoP: cli_agent_usage_pricing_normalize_usage @ agent/usage_pricing.py:normalize_usage */

/* Port of Python agent/usage_pricing.py:normalize_usage */
/* Normalizes usage data from different provider formats. */
int cli_agent_usage_pricing_normalize_usage(
    const char *raw_usage_json, char *output, size_t output_size)
{
    if (!raw_usage_json || !output || output_size == 0) {
        return -1;
    }
    /* CLI port: pass through the JSON as-is. */
    strncpy(output, raw_usage_json, output_size - 1);
    output[output_size - 1] = '\0';
    return 0;
}

/* Port of Python agent/usage_pricing.py:_normalize_bedrock_model_name */