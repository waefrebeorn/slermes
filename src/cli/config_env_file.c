/*
 * config_env_file.c — home-dir + .env file helpers.
 *
 * Split from config.c (soul.md: self-contained modules). Home resolution
 * and .env parsing; no file-scope state.
 */
#include "config_env_file.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void get_slermes_home(char *buf, size_t sz) {
    hermes_get_home(buf, sz);
}


void parse_env_file(const char *path, hermes_config_t *cfg) {
    FILE *f = fopen(path, "r");
    if (!f) return;

    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        /* Strip trailing newline */
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r'))
            line[--len] = '\0';

        /* Skip empty/comment lines */
        const char *p = line;
        while (*p == ' ') p++;
        if (*p == '#' || *p == '\0') continue;

        /* Find '=' */
        const char *eq = strchr(p, '=');
        if (!eq) continue;

        size_t key_len = (size_t)(eq - p);
        if (key_len == 0) continue;

        const char *val = eq + 1;

        /* Match known keys and update config */
        if (strncmp(p, "HERMES_API_KEY", key_len) == 0 && key_len == 14) {
            snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", val);
            snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", val);
        }
        else if (strncmp(p, "OPENAI_API_KEY", key_len) == 0 && key_len == 14) {
            if (cfg->api_key[0] == '\0') {
                snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", val);
                snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", val);
            }
        }
        else if (strncmp(p, "ANTHROPIC_API_KEY", key_len) == 0 && key_len == 17) {
            /* Store for anthropic provider; also keep as generic fallback */
            if (cfg->api_key[0] == '\0') {
                snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", val);
                snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", val);
            }
        }
        else if (strncmp(p, "GOOGLE_API_KEY", key_len) == 0 && key_len == 14) {
            if (cfg->api_key[0] == '\0') {
                snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", val);
                snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", val);
            }
        }
        else if (strncmp(p, "NOUS_API_KEY", key_len) == 0 && key_len == 12) {
            if (cfg->api_key[0] == '\0') {
                snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", val);
                snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", val);
            }
        }
        else if (strncmp(p, "XAI_API_KEY", key_len) == 0 && key_len == 11) {
            if (cfg->api_key[0] == '\0') {
                snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", val);
                snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", val);
            }
        }
        else if (strncmp(p, "OPENROUTER_API_KEY", key_len) == 0 && key_len == 18) {
            if (cfg->api_key[0] == '\0') {
                snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", val);
                snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", val);
            }
        }
        else if (strncmp(p, "DEEPSEEK_API_KEY", key_len) == 0 && key_len == 16) {
            if (cfg->api_key[0] == '\0') {
                snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", val);
                snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", val);
            }
            snprintf(cfg->provider_cfg.deepseek_api_key, sizeof(cfg->provider_cfg.deepseek_api_key), "%s", val);
        }
        else if (strncmp(p, "SLERMES_API_KEY", key_len) == 0 && key_len == 15) {
            if (cfg->api_key[0] == '\0') {
                snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", val);
                snprintf(cfg->provider_cfg.api_key, sizeof(cfg->provider_cfg.api_key), "%s", val);
            }
        }
        else if (strncmp(p, "HERMES_MODEL", key_len) == 0 && key_len == 12) {
            snprintf(cfg->model, sizeof(cfg->model), "%s", val);
            snprintf(cfg->provider_cfg.model, sizeof(cfg->provider_cfg.model), "%s", val);
        }
        else if (strncmp(p, "HERMES_PROVIDER", key_len) == 0 && key_len == 15) {
            snprintf(cfg->provider, sizeof(cfg->provider), "%s", val);
            snprintf(cfg->provider_cfg.provider, sizeof(cfg->provider_cfg.provider), "%s", val);
        }
        else if (strncmp(p, "HERMES_BASE_URL", key_len) == 0 && key_len == 15) {
            snprintf(cfg->base_url, sizeof(cfg->base_url), "%s", val);
            snprintf(cfg->provider_cfg.base_url, sizeof(cfg->provider_cfg.base_url), "%s", val);
        }
        else if (strncmp(p, "SLERMES_HOME", key_len) == 0 && key_len == 12) {
            /* Handled by get_slermes_home() */
        }
    }
    fclose(f);
}
