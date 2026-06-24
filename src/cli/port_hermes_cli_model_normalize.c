/*
 * port_hermes_cli_model_normalize.c — C port of hermes_cli/model_normalize.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_hermes_cli_model_normalize__strip_vendor_prefix @ hermes_cli/model_normalize.py:_strip_vendor_prefix */

/* Port of Python hermes_cli/model_normalize.py:_strip_vendor_prefix */
/* Removes a vendor/ prefix if present. */
int cli_hermes_cli_model_normalize__strip_vendor_prefix(
    const char *model_name, char *output, size_t output_size)
{
    if (!model_name || !output || output_size == 0) {
        return -1;
    }
    const char *slash = strchr(model_name, '/');
    if (slash) {
        strncpy(output, slash + 1, output_size - 1);
    } else {
        strncpy(output, model_name, output_size - 1);
    }
    output[output_size - 1] = '\0';
    return 0;
}

/* PoP: cli_hermes_cli_model_normalize__dots_to_hyphens @ hermes_cli/model_normalize.py:_dots_to_hyphens */

/* Port of Python hermes_cli/model_normalize.py:_dots_to_hyphens */
/* Replaces dots with hyphens in a model name. */
int cli_hermes_cli_model_normalize__dots_to_hyphens(
    const char *model_name, char *output, size_t output_size)
{
    if (!model_name || !output || output_size == 0) {
        return -1;
    }
    strncpy(output, model_name, output_size - 1);
    output[output_size - 1] = '\0';
    for (char *p = output; *p; p++) {
        if (*p == '.') *p = '-';
    }
    return 0;
}

/* PoP: cli_hermes_cli_model_normalize__normalize_provider_alias @ hermes_cli/model_normalize.py:_normalize_provider_alias */

/* Port of Python hermes_cli/model_normalize.py:_normalize_provider_alias */
/* Resolves provider aliases to canonical IDs. */
int cli_hermes_cli_model_normalize__normalize_provider_alias(
    const char *provider_name, char *output, size_t output_size)
{
    if (!provider_name || !output || output_size == 0) {
        return -1;
    }
    /* Simple alias map for common providers. */
    if (strcasecmp(provider_name, "openrouter") == 0) {
        strncpy(output, "openrouter", output_size - 1);
    } else if (strcasecmp(provider_name, "anthropic") == 0) {
        strncpy(output, "anthropic", output_size - 1);
    } else if (strcasecmp(provider_name, "openai") == 0) {
        strncpy(output, "openai", output_size - 1);
    } else {
        strncpy(output, provider_name, output_size - 1);
    }
    output[output_size - 1] = '\0';
    for (char *p = output; *p; p++) {
        if (*p >= 'A' && *p <= 'Z') *p = *p - 'A' + 'a';
    }
    return 0;
}

/* PoP: cli_hermes_cli_model_normalize__strip_matching_provider_prefix @ hermes_cli/model_normalize.py:_strip_matching_provider_prefix */

/* Port of Python hermes_cli/model_normalize.py:_strip_matching_provider_prefix */
/* Strips provider/ only when the prefix matches the target provider. */
int cli_hermes_cli_model_normalize__strip_matching_provider_prefix(
    const char *model_name, const char *target_provider,
    char *output, size_t output_size)
{
    if (!model_name || !target_provider || !output || output_size == 0) {
        return -1;
    }
    const char *slash = strchr(model_name, '/');
    if (!slash) {
        strncpy(output, model_name, output_size - 1);
        output[output_size - 1] = '\0';
        return 0;
    }
    /* Check if prefix matches target provider. */
    size_t prefix_len = (size_t)(slash - model_name);
    char prefix[128];
    if (prefix_len >= sizeof(prefix)) prefix_len = sizeof(prefix) - 1;
    strncpy(prefix, model_name, prefix_len);
    prefix[prefix_len] = '\0';
    /* Normalize both for comparison. */
    for (char *p = prefix; *p; p++) {
        if (*p >= 'A' && *p <= 'Z') *p = *p - 'A' + 'a';
    }
    if (strcmp(prefix, target_provider) == 0) {
        strncpy(output, slash + 1, output_size - 1);
    } else {
        strncpy(output, model_name, output_size - 1);
    }
    output[output_size - 1] = '\0';
    return 0;
}

/* PoP: cli_hermes_cli_model_normalize_detect_vendor @ hermes_cli/model_normalize.py:detect_vendor */

/* Port of Python hermes_cli/model_normalize.py:detect_vendor */
/* Detects the vendor slug from a bare model name. */
int cli_hermes_cli_model_normalize_detect_vendor(
    const char *model_name, char *output, size_t output_size)
{
    if (!model_name || !output || output_size == 0) {
        return -1;
    }
    /* Check for vendor/ prefix first. */
    const char *slash = strchr(model_name, '/');
    if (slash) {
        size_t len = (size_t)(slash - model_name);
        if (len >= output_size) len = output_size - 1;
        strncpy(output, model_name, len);
        output[len] = '\0';
        return 0;
    }
    /* Use first hyphen-delimited token. */
    const char *hyphen = strchr(model_name, '-');
    size_t len;
    if (hyphen) {
        len = (size_t)(hyphen - model_name);
    } else {
        len = strlen(model_name);
    }
    if (len >= output_size) len = output_size - 1;
    /* Copy first token and look up vendor. */
    char first_token[128];
    if (len >= sizeof(first_token)) len = sizeof(first_token) - 1;
    strncpy(first_token, model_name, len);
    first_token[len] = '\0';
    /* Simple vendor lookup. */
    if (strcasecmp(first_token, "claude") == 0) {
        strncpy(output, "anthropic", output_size - 1);
    } else if (strcasecmp(first_token, "gpt") == 0 || strcasecmp(first_token, "o1") == 0 ||
               strcasecmp(first_token, "o3") == 0 || strcasecmp(first_token, "o4") == 0) {
        strncpy(output, "openai", output_size - 1);
    } else if (strcasecmp(first_token, "gemini") == 0 || strcasecmp(first_token, "gemma") == 0) {
        strncpy(output, "google", output_size - 1);
    } else if (strcasecmp(first_token, "deepseek") == 0) {
        strncpy(output, "deepseek", output_size - 1);
    } else {
        output[0] = '\0';
        return -1;
    }
    output[output_size - 1] = '\0';
    return 0;
}

/* PoP: cli_hermes_cli_model_normalize__prepend_vendor @ hermes_cli/model_normalize.py:_prepend_vendor */

/* Port of Python hermes_cli/model_normalize.py:_prepend_vendor */
/* Prepends the detected vendor/ prefix if missing. */
int cli_hermes_cli_model_normalize__prepend_vendor(
    const char *model_name, char *output, size_t output_size)
{
    if (!model_name || !output || output_size == 0) {
        return -1;
    }
    if (strchr(model_name, '/')) {
        strncpy(output, model_name, output_size - 1);
        output[output_size - 1] = '\0';
        return 0;
    }
    char vendor[64];
    if (cli_hermes_cli_model_normalize_detect_vendor(model_name, vendor, sizeof(vendor)) == 0 && vendor[0]) {
        snprintf(output, output_size, "%s/%s", vendor, model_name);
    } else {
        strncpy(output, model_name, output_size - 1);
        output[output_size - 1] = '\0';
    }
    return 0;
}

/* PoP: cli_hermes_cli_model_normalize_normalize_model_for_provider @ hermes_cli/model_normalize.py:normalize_model_for_provider */

/* Port of Python hermes_cli/model_normalize.py:normalize_model_for_provider */
/* Translates a model name into the format the target provider expects. */
int cli_hermes_cli_model_normalize_normalize_model_for_provider(
    const char *model_input, const char *target_provider,
    char *output, size_t output_size)
{
    if (!model_input || !target_provider || !output || output_size == 0) {
        return -1;
    }
    /* Normalize provider name. */
    char provider[64];
    cli_hermes_cli_model_normalize__normalize_provider_alias(
        target_provider, provider, sizeof(provider));
    /* Aggregators: need vendor/model format. */
    if (strcmp(provider, "openrouter") == 0 || strcmp(provider, "nous") == 0 ||
        strcmp(provider, "kilocode") == 0) {
        return cli_hermes_cli_model_normalize__prepend_vendor(
            model_input, output, output_size);
    }
    /* Anthropic: strip matching prefix, dots -> hyphens. */
    if (strcmp(provider, "anthropic") == 0) {
        char stripped[256];
        cli_hermes_cli_model_normalize__strip_matching_provider_prefix(
            model_input, provider, stripped, sizeof(stripped));
        return cli_hermes_cli_model_normalize__dots_to_hyphens(
            stripped, output, output_size);
    }
    /* Copilot: strip matching prefix, keep dots. */
    if (strcmp(provider, "copilot") == 0 || strcmp(provider, "copilot-acp") == 0) {
        return cli_hermes_cli_model_normalize__strip_matching_provider_prefix(
            model_input, provider, output, output_size);
    }
    /* DeepSeek: map to canonical names. */
    if (strcmp(provider, "deepseek") == 0) {
        char stripped[256];
        cli_hermes_cli_model_normalize__strip_matching_provider_prefix(
            model_input, provider, stripped, sizeof(stripped));
        /* Check for reasoner keywords. */
        char lower[256];
        strncpy(lower, stripped, sizeof(lower) - 1);
        lower[sizeof(lower) - 1] = '\0';
        for (char *p = lower; *p; p++) {
            if (*p >= 'A' && *p <= 'Z') *p = *p - 'A' + 'a';
        }
        if (strstr(lower, "reasoner") || strstr(lower, "r1") ||
            strstr(lower, "think") || strstr(lower, "reasoning")) {
            strncpy(output, "deepseek-reasoner", output_size - 1);
        } else {
            strncpy(output, "deepseek-chat", output_size - 1);
        }
        output[output_size - 1] = '\0';
        return 0;
    }
    /* Default: pass through. */
    strncpy(output, model_input, output_size - 1);
    output[output_size - 1] = '\0';
    return 0;
}
