/*
 * relay_plugin_cutover.c — C11 port of hermes_cli/relay_plugin_cutover.py
 *
 * Shared migration guards for Hermes' native NeMo Relay ownership: detect
 * removed Relay plugin identities in config values and non-empty legacy
 * Relay exporter env vars.
 */
#define _POSIX_C_SOURCE 200809L

#include "relay_plugin_cutover.h"
#include <stdlib.h>
#include <string.h>

static const char *const LEGACY_RELAY_PLUGIN_KEYS[] = {
    "nemo_relay", "observability/nemo_relay", NULL,
};

static const char *const LEGACY_RELAY_EXPORT_ENV_VARS[] = {
    "HERMES_NEMO_RELAY_ATOF_ENABLED",
    "HERMES_NEMO_RELAY_ATOF_OUTPUT_DIRECTORY",
    "HERMES_NEMO_RELAY_ATOF_FILENAME",
    "HERMES_NEMO_RELAY_ATOF_MODE",
    "HERMES_NEMO_RELAY_ATIF_ENABLED",
    "HERMES_NEMO_RELAY_ATIF_OUTPUT_DIRECTORY",
    "HERMES_NEMO_RELAY_ATIF_FILENAME_TEMPLATE",
    "HERMES_NEMO_RELAY_ATIF_AGENT_NAME",
    "HERMES_NEMO_RELAY_ATIF_AGENT_VERSION",
    "HERMES_NEMO_RELAY_ATIF_EXPORT_TIMEOUT_S",
    "HERMES_NEMO_RELAY_ATIF_MODEL_NAME",
    "HERMES_NEMO_RELAY_ATIF_SUBAGENT_EXPORT_MODE",
    NULL,
};

/* PoP: legacy_relay_plugin_keys @ hermes_cli/relay_plugin_cutover.py:
 * legacy_relay_plugin_keys */
char **cutover_legacy_relay_plugin_keys(const char *const *values, size_t n,
                                        size_t *out_n)
{
    if (out_n) *out_n = 0;
    if (!values) return NULL; /* not a list/tuple/set → () */
    /* collect matching values into a set (dedupe) then sort */
    char **found = NULL;
    size_t fn = 0;
    for (size_t i = 0; i < n; i++) {
        const char *value = values[i];
        if (!value) continue; /* isinstance(value, str) guard */
        int legacy = 0, dup = 0;
        for (size_t k = 0; LEGACY_RELAY_PLUGIN_KEYS[k]; k++)
            if (strcmp(value, LEGACY_RELAY_PLUGIN_KEYS[k]) == 0)
                { legacy = 1; break; }
        if (!legacy) continue;
        for (size_t j = 0; j < fn; j++)
            if (strcmp(found[j], value) == 0) { dup = 1; break; }
        if (dup) continue;
        char **nf = realloc(found, (fn + 1) * sizeof(char *));
        if (!nf) break;
        found = nf;
        found[fn] = strdup(value);
        if (found[fn]) fn++;
    }
    for (size_t i = 0; i + 1 < fn; i++)          /* sorted(...) */
        for (size_t j = i + 1; j < fn; j++)
            if (strcmp(found[i], found[j]) > 0) {
                char *tmp = found[i]; found[i] = found[j]; found[j] = tmp;
            }
    if (out_n) *out_n = fn;
    return found;
}

/* PoP: configured_legacy_relay_env_vars @ ...:configured_legacy_relay_env_vars */
char **cutover_configured_legacy_relay_env_vars(
    const char *const *env_names, const char *const *env_vals, size_t n,
    size_t *out_n)
{
    if (out_n) *out_n = 0;
    if (!env_names || !env_vals) return NULL; /* env is None → () */
    char **found = NULL;
    size_t fn = 0;
    for (size_t i = 0; i < n; i++) {
        const char *name = env_names[i];
        if (!name) continue;
        int legacy = 0;
        for (size_t k = 0; LEGACY_RELAY_EXPORT_ENV_VARS[k]; k++)
            if (strcmp(name, LEGACY_RELAY_EXPORT_ENV_VARS[k]) == 0)
                { legacy = 1; break; }
        if (!legacy) continue;
        const char *value = env_vals[i];
        /* None or whitespace-only values do not count as configured. */
        int configured = 0;
        if (value) {
            const char *p = value;
            while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
                p++;
            configured = *p != '\0';
        }
        if (!configured) continue;
        int dup = 0;
        for (size_t j = 0; j < fn; j++)
            if (strcmp(found[j], name) == 0) { dup = 1; break; }
        if (dup) continue;
        char **nf = realloc(found, (fn + 1) * sizeof(char *));
        if (!nf) break;
        found = nf;
        found[fn] = strdup(name);
        if (found[fn]) fn++;
    }
    for (size_t i = 0; i + 1 < fn; i++)          /* sorted(...) */
        for (size_t j = i + 1; j < fn; j++)
            if (strcmp(found[i], found[j]) > 0) {
                char *tmp = found[i]; found[i] = found[j]; found[j] = tmp;
            }
    if (out_n) *out_n = fn;
    return found;
}
