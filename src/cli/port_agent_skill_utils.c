/*
 * port_agent_skill_utils.c — C port of agent/skill_utils.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_agent_skill_utils__raw_config_cache_clear @ agent/skill_utils.py:_raw_config_cache_clear */

/* Port of Python agent/skill_utils.py:_raw_config_cache_clear */
/* Test hook — drop the shared raw config cache. */
void cli_agent_skill_utils__raw_config_cache_clear(void)
{
    /* In C, config caching is handled by the hermes_cli_config module.
     * This is a no-op placeholder — the cache is cleared by
     * re-reading config.yaml on the next access. */
    hermes_log(LOG_DEBUG, "skill_utils", "raw config cache clear requested (no-op in C)");
}

/* PoP: cli_agent_skill_utils__load_raw_config @ agent/skill_utils.py:_load_raw_config */

/* Port of Python agent/skill_utils.py:_load_raw_config */
/* Read config.yaml with a shared mtime+size keyed cache. */
char *cli_agent_skill_utils__load_raw_config(void)
{
    const char *config_path = getenv("HERMES_CONFIG_PATH");
    if (!config_path || !*config_path) {
        /* Default: $HERMES_HOME/config.yaml */
        const char *home = getenv("HERMES_HOME");
        if (!home || !*home) {
            home = getenv("HOME");
            if (!home || !*home) {
                return strdup("{}");
            }
            char path[1024];
            snprintf(path, sizeof(path), "%s/.hermes/config.yaml", home);
            config_path = path;
        } else {
            char path[1024];
            snprintf(path, sizeof(path), "%s/config.yaml", home);
            config_path = path;
        }
    }

    /* Read config.yaml */
    FILE *f = fopen(config_path, "r");
    if (!f) {
        hermes_log(LOG_DEBUG, "skill_utils", "Cannot read config: %s", config_path);
        return strdup("{}");
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) {
        fclose(f);
        return strdup("{}");
    }

    char *content = (char *)malloc((size_t)size + 1);
    if (!content) {
        fclose(f);
        return strdup("{}");
    }

    size_t n = fread(content, 1, (size_t)size, f);
    fclose(f);
    content[n] = '\0';

    return content;
}
