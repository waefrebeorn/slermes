/*
 * hermes_env_keys.c — .env API key management for Slermes C
 * Phase 569+: S0a #2 .env key wizard parity gap.
 *
 * Provides:
 *   /key list            — show all provider keys (masked)
 *   /key set <provider>  — interactively set a provider's key
 *   /key show <provider> — show a specific key (masked)
 *   /key unset <provider>— remove a provider's key from .env
 *
 * Parity with Python Hermes' setup.py key management.
 */

/* PoP: environment variable keys (port of hermes_constants) */

#include "hermes_env_keys.h"
#include "hermes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* ================================================================
 * Provider→env-var mapping table
 * ================================================================ */

const provider_key_map_t PROVIDER_KEY_MAP[] = {
    {"openai",     "OPENAI_API_KEY",       "sk-"},
    {"anthropic",  "ANTHROPIC_API_KEY",    "sk-ant-"},
    {"google",     "GOOGLE_API_KEY",       "AIza"},
    {"deepseek",   "DEEPSEEK_API_KEY",     "sk-"},
    {"xai",        "XAI_API_KEY",          "xai-"},
    {"openrouter", "OPENROUTER_API_KEY",   "sk-or-"},
    {"azure",      "AZURE_API_KEY",        NULL},
    {"bedrock",    "AWS_ACCESS_KEY_ID",    NULL},
    {"groq",       "GROQ_API_KEY",         "gsk_"},
    {"together",   "TOGETHER_API_KEY",     NULL},
    {"mistral",    "MISTRAL_API_KEY",      NULL},
    {"cohere",     "COHERE_API_KEY",       NULL},
    {"perplexity", "PERPLEXITY_API_KEY",   NULL},
    {"custom",     "HERMES_API_KEY",       NULL},
    {NULL, NULL, NULL}  /* sentinel */
};

const provider_key_map_t *key_map_find(const char *provider) {
    if (!provider) return NULL;
    for (int i = 0; PROVIDER_KEY_MAP[i].provider; i++) {
        if (strcmp(provider, PROVIDER_KEY_MAP[i].provider) == 0)
            return &PROVIDER_KEY_MAP[i];
    }
    return NULL;
}

const provider_key_map_t *key_map_find_by_env(const char *env_var) {
    if (!env_var) return NULL;
    for (int i = 0; PROVIDER_KEY_MAP[i].env_var; i++) {
        if (strcmp(env_var, PROVIDER_KEY_MAP[i].env_var) == 0)
            return &PROVIDER_KEY_MAP[i];
    }
    return NULL;
}

/* ================================================================
 * .env file path resolution
 * ================================================================ */

const char *key_env_path(const char *hermes_home, char *buf, size_t bufsz) {
    if (!hermes_home || !hermes_home[0]) {
        /* Try SLERMES_HOME, HERMES_HOME, then ~/.slermes, ~/.hermes */
        const char *home = getenv("SLERMES_HOME");
        if (!home) home = getenv("HERMES_HOME");
        if (!home) home = getenv("HOME");
        if (!home) return NULL;
        char tmp[1024];
        snprintf(tmp, sizeof(tmp), "%s/.slermes", home);
        if (access(tmp, F_OK) == 0)
            snprintf(buf, bufsz, "%s/.slermes/.env", home);
        else
            snprintf(buf, bufsz, "%s/.hermes/.env", home);
    } else {
        snprintf(buf, bufsz, "%s/.env", hermes_home);
    }
    return buf;
}

/* ================================================================
 * Key list (masked)
 * ================================================================ */

void key_list_all(void) {
    printf("Configured API keys:\n\n");
    int found = 0;
    for (int i = 0; PROVIDER_KEY_MAP[i].provider; i++) {
        const char *val = getenv(PROVIDER_KEY_MAP[i].env_var);
        if (val && val[0]) {
            /* Mask: show first 8 chars + "..." */
            char masked[64];
            size_t vlen = strlen(val);
            if (vlen > 12) {
                snprintf(masked, sizeof(masked), "%.8s... (len=%zu)", val, vlen);
            } else {
                snprintf(masked, sizeof(masked), "%.*s...", (int)(vlen > 4 ? 4 : vlen), val);
            }
            printf("  %-18s  %s  %s\n", PROVIDER_KEY_MAP[i].env_var, "✓", masked);
            found++;
        } else {
            printf("  %-18s  %s  (not set)\n", PROVIDER_KEY_MAP[i].env_var, " ");
        }
    }
    if (!found) {
        printf("\n  No API keys found in environment.\n");
        printf("  Use '/key set <provider>' to configure one.\n");
    }
    printf("\n");
}

/* ================================================================
 * Key show (masked)
 * ================================================================ */

void key_show(const char *provider) {
    const provider_key_map_t *m = key_map_find(provider);
    if (!m) {
        printf("Unknown provider: %s\n", provider);
        printf("Known providers: ");
        for (int i = 0; PROVIDER_KEY_MAP[i].provider; i++) {
            if (i > 0) printf(", ");
            printf("%s", PROVIDER_KEY_MAP[i].provider);
        }
        printf("\n");
        return;
    }
    const char *val = getenv(m->env_var);
    if (!val || !val[0]) {
        printf("%s: %s — NOT SET\n", m->provider, m->env_var);
        return;
    }
    /* Show first 8 chars + last 4 chars */
    size_t vlen = strlen(val);
    char masked[128];
    if (vlen > 12) {
        snprintf(masked, sizeof(masked), "%.8s...%.4s (len=%zu)", val, val + vlen - 4, vlen);
    } else {
        snprintf(masked, sizeof(masked), "%s", val);
    }
    printf("%s: %s = %s\n", m->provider, m->env_var, masked);
}

/* ================================================================
 * .env file line manipulation helpers
 * ================================================================ */

/* Read .env file into a heap buffer. Returns NULL on error. */
static char *read_env_file(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (fsize <= 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)fsize + 2);
    if (!buf) { fclose(f); return NULL; }
    size_t nread = fread(buf, 1, (size_t)fsize, f);
    fclose(f);
    buf[nread] = '\n';   /* ensure trailing newline */
    buf[nread + 1] = '\0';
    if (out_len) *out_len = nread;
    return buf;
}

/* Write buffer to .env file */
static int write_env_file(const char *path, const char *buf) {
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    size_t len = strlen(buf);
    if (len > 0 && buf[len-1] == '\n') {
        fwrite(buf, 1, len, f);
    } else {
        fwrite(buf, 1, len, f);
        fputc('\n', f);
    }
    fclose(f);
    return 0;
}

/* ================================================================
 * Key set/modify in .env
 * ================================================================ */

int key_set(const char *hermes_home, const char *provider, const char *value) {
    const provider_key_map_t *m = key_map_find(provider);
    if (!m) return -1;

    char env_path[1024];
    key_env_path(hermes_home, env_path, sizeof(env_path));

    /* Ensure directory exists */
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s", env_path);
    char *slash = strrchr(dir, '/');
    if (slash) {
        *slash = '\0';
        mkdir(dir, 0700);
    }

    /* Read existing .env or start fresh */
    size_t flen = 0;
    char *buf = read_env_file(env_path, &flen);
    int found = 0;

    if (buf) {
        /* Search for existing KEY=value or #KEY=value (commented) lines */
        size_t key_len = strlen(m->env_var);
        char *line_start = buf;
        char *new_buf = (char *)malloc(flen + 256);
        if (!new_buf) { free(buf); return -1; }
        new_buf[0] = '\0';

        while (*line_start) {
            char *nl = strchr(line_start, '\n');
            size_t linelen = nl ? (size_t)(nl - line_start) : strlen(line_start);
            bool is_comment = (line_start[0] == '#');

            /* Check if this line starts with KEY= (optionally commented) */
            const char *check = is_comment ? line_start + 1 : line_start;
            /* Skip leading whitespace on check */
            while (*check == ' ' || *check == '\t') check++;

            if (strncmp(check, m->env_var, key_len) == 0 && check[key_len] == '=') {
                /* Replace this line */
                if (!found) {
                    char newline[1024];
                    if (value && value[0]) {
                        snprintf(newline, sizeof(newline), "%s=%s", m->env_var, value);
                    } else {
                        snprintf(newline, sizeof(newline), "#%s=", m->env_var);
                    }
                    strcat(new_buf, newline);
                    strcat(new_buf, "\n");
                    found = 1;
                }
                /* Skip the old line */
                line_start = nl + 1;
                continue;
            }

            /* Keep this line as-is */
            strncat(new_buf, line_start, linelen);
            strcat(new_buf, "\n");
            line_start = nl + 1;
        }

        /* Append if not found */
        if (!found) {
            char newline[1024];
            if (value && value[0]) {
                snprintf(newline, sizeof(newline), "%s=%s", m->env_var, value);
            } else {
                snprintf(newline, sizeof(newline), "#%s=", m->env_var);
            }
            strcat(new_buf, newline);
            strcat(new_buf, "\n");
        }

        free(buf);
        int ret = write_env_file(env_path, new_buf);
        free(new_buf);
        return ret;
    }

    /* No existing .env — create one */
    FILE *f = fopen(env_path, "w");
    if (!f) return -1;
    fprintf(f, "# Slermes API Keys\n");
    fprintf(f, "# One key per line: PROVIDER_API_KEY=your-key-here\n");
    if (value && value[0]) {
        fprintf(f, "%s=%s\n", m->env_var, value);
    } else {
        /* Write commented template entries for all known providers */
        for (int i = 0; PROVIDER_KEY_MAP[i].provider; i++) {
            fprintf(f, "#%s=\n", PROVIDER_KEY_MAP[i].env_var);
        }
    }
    fclose(f);
    return 0;
}

/* ================================================================
 * Key unset (remove from .env)
 * ================================================================ */

int key_unset(const char *hermes_home, const char *provider) {
    return key_set(hermes_home, provider, NULL);  /* comment out */
}

/* ================================================================
 * Interactive key wizard
 * ================================================================ */

int key_wizard(const char *hermes_home, const char *provider) {
    const provider_key_map_t *m = key_map_find(provider);
    if (!m) {
        printf("Unknown provider: %s\n", provider);
        printf("Known providers: ");
        for (int i = 0; PROVIDER_KEY_MAP[i].provider; i++) {
            if (i > 0) printf(", ");
            printf("%s", PROVIDER_KEY_MAP[i].provider);
        }
        printf("\n");
        return -1;
    }

    /* Check current status */
    const char *current = getenv(m->env_var);
    printf("=== API Key: %s ===\n\n", provider);
    printf("  Env var: %s\n", m->env_var);
    if (m->prefix) printf("  Expected prefix: %s\n", m->prefix);
    if (current && current[0]) {
        size_t vlen = strlen(current);
        char masked[128];
        if (vlen > 8) {
            snprintf(masked, sizeof(masked), "%.8s... (len=%zu)", current, vlen);
        } else {
            snprintf(masked, sizeof(masked), "%s", current);
        }
        printf("  Current: %s\n", masked);
    } else {
        printf("  Current: (not set)\n");
    }

    printf("\nEnter API key (or press Enter to cancel, '!' to clear):\n> ");
    fflush(stdout);

    char input[1024] = {0};
    if (!fgets(input, sizeof(input), stdin)) return -1;
    /* Trim newline */
    char *nl = strchr(input, '\n');
    if (nl) *nl = '\0';

    if (input[0] == '\0') {
        printf("Cancelled.\n");
        return 0;
    }
    if (strcmp(input, "!") == 0) {
        if (key_unset(hermes_home, provider) == 0) {
            printf("Key cleared for %s.\n", provider);
            printf("Run '/reload env' to apply changes.\n");
            return 0;
        }
        printf("Error: could not clear key.\n");
        return -1;
    }

    if (key_set(hermes_home, provider, input) == 0) {
        printf("Key saved for %s.\n", provider);
        printf("Run '/reload env' to apply changes.\n");
        return 0;
    }
    printf("Error: could not save key.\n");
    return -1;
}
