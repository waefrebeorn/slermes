/*
 * port_agent_onboarding.c — C port of agent/onboarding.py
 */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_agent_onboarding__get_seen_dict @ agent/onboarding.py:_get_seen_dict */

/* Port of Python agent/onboarding.py:_get_seen_dict */
/* Get the seen dict from config, returning an empty dict if not found. */
char *cli_agent_onboarding__get_seen_dict(
    const char **config_keys, const char **config_values, int config_count)
{
    if (!config_keys || !config_values || config_count <= 0) {
        return strdup("{}");
    }

    /* Look for onboarding.seen in config */
    for (int i = 0; i < config_count; i++) {
        if (config_keys[i] && strcmp(config_keys[i], "onboarding.seen") == 0) {
            if (config_values[i] && *config_values[i]) {
                return strdup(config_values[i]);
            }
            return strdup("{}");
        }
    }

    /* Also check for nested onboarding -> seen */
    int onboarding_idx = -1;
    for (int i = 0; i < config_count; i++) {
        if (config_keys[i] && strcmp(config_keys[i], "onboarding") == 0) {
            onboarding_idx = i;
            break;
        }
    }

    if (onboarding_idx >= 0 && config_values[onboarding_idx]) {
        /* Parse the onboarding value as a JSON-like dict and extract "seen" */
        const char *val = config_values[onboarding_idx];
        const char *seen = strstr(val, "\"seen\"");
        if (seen) {
            seen += 6; /* skip "\"seen\"" */
            while (*seen == ' ' || *seen == '\t' || *seen == ':') seen++;
            if (*seen == '{') {
                /* Extract the seen dict */
                const char *end = strchr(seen, '}');
                if (end) {
                    size_t len = (size_t)(end - seen + 1);
                    char *result = (char *)malloc(len + 1);
                    if (result) {
                        memcpy(result, seen, len);
                        result[len] = '\0';
                        return result;
                    }
                }
            }
        }
    }

    return strdup("{}");
}
