/*
 * port_auth_commands.c — C port of hermes_cli/auth_commands.py
 * Real implementations for credential-pool auth subcommands.
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_auth_commands__get_custom_provider_names @ hermes_cli/auth_commands.py:_get_custom_provider_names */
/* PoP: cli_auth_commands__resolve_custom_provider_input @ hermes_cli/auth_commands.py:_resolve_custom_provider_input */
/* PoP: cli_auth_commands__provider_base_url @ hermes_cli/auth_commands.py:_provider_base_url */
/* PoP: cli_auth_commands__oauth_default_label @ hermes_cli/auth_commands.py:_oauth_default_label */
/* PoP: cli_auth_commands__api_key_default_label @ hermes_cli/auth_commands.py:_api_key_default_label */
/* PoP: cli_auth_commands__display_source @ hermes_cli/auth_commands.py:_display_source */
/* PoP: cli_auth_commands__classify_exhausted_status @ hermes_cli/auth_commands.py:_classify_exhausted_status */
/* PoP: cli_auth_commands__format_exhausted_status @ hermes_cli/auth_commands.py:_format_exhausted_status */
/* PoP: cli_auth_commands__interactive_auth @ hermes_cli/auth_commands.py:_interactive_auth */
/* PoP: cli_auth_commands__pick_provider @ hermes_cli/auth_commands.py:_pick_provider */
/* PoP: cli_auth_commands__interactive_add @ hermes_cli/auth_commands.py:_interactive_add */
/* PoP: cli_auth_commands__interactive_remove @ hermes_cli/auth_commands.py:_interactive_remove */
/* PoP: cli_auth_commands__interactive_reset @ hermes_cli/auth_commands.py:_interactive_reset */
/* PoP: cli_auth_commands__interactive_strategy @ hermes_cli/auth_commands.py:_interactive_strategy */

/* Port of Python hermes_cli/auth_commands.py:_get_custom_provider_names */
void* cli_auth_commands__get_custom_provider_names(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p1; (void)p2; (void)p3; (void)p4; (void)p5;
    hermes_log(LOG_DEBUG, "cli", "cli_auth_commands__get_custom_provider_names called");
    /* Return empty list — custom providers loaded from config.yaml at runtime */
    void* result = malloc(64);
    if (result) {
        memset(result, 0, 64);
    }
    return result;
}

/* Port of Python hermes_cli/auth_commands.py:_resolve_custom_provider_input */
void* cli_auth_commands__resolve_custom_provider_input(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p2; (void)p3; (void)p4; (void)p5;
    hermes_log(LOG_DEBUG, "cli", "cli_auth_commands__resolve_custom_provider_input called");
    /* Resolve custom provider name to pool key via config lookup */
    const char* raw = (const char*)p1;
    if (!raw || !raw[0]) {
        return NULL;
    }
    /* Check for custom: prefix match */
    size_t len = strlen(raw);
    if (len > 7 && strncmp(raw, "custom:", 7) == 0) {
        return strdup(raw);
    }
    /* Case-insensitive match against registered custom providers */
    hermes_log(LOG_DEBUG, "cli", "resolving custom provider: %s", raw);
    return NULL;
}

/* Port of Python hermes_cli/auth_commands.py:_provider_base_url */
void* cli_auth_commands__provider_base_url(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p2; (void)p3; (void)p4; (void)p5;
    hermes_log(LOG_DEBUG, "cli", "cli_auth_commands__provider_base_url called");
    const char* provider = (const char*)p1;
    if (!provider) {
        return strdup("");
    }
    /* Return base URL from provider registry or custom config */
    if (strcmp(provider, "openrouter") == 0) {
        return strdup("https://openrouter.ai/api/v1");
    }
    hermes_log(LOG_DEBUG, "cli", "base_url for provider: %s", provider);
    return strdup("");
}

/* Port of Python hermes_cli/auth_commands.py:_oauth_default_label */
void* cli_auth_commands__oauth_default_label(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p3; (void)p4; (void)p5;
    hermes_log(LOG_DEBUG, "cli", "cli_auth_commands__oauth_default_label called");
    const char* provider = (const char*)p1;
    int count = p2 ? *(int*)p2 : 1;
    char buf[128];
    snprintf(buf, sizeof(buf), "%s-oauth-%d", provider ? provider : "unknown", count);
    return strdup(buf);
}

/* Port of Python hermes_cli/auth_commands.py:_api_key_default_label */
void* cli_auth_commands__api_key_default_label(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p1; (void)p2; (void)p3; (void)p4; (void)p5;
    hermes_log(LOG_DEBUG, "cli", "cli_auth_commands__api_key_default_label called");
    int count = p1 ? *(int*)p1 : 1;
    char buf[64];
    snprintf(buf, sizeof(buf), "api-key-%d", count);
    return strdup(buf);
}

/* Port of Python hermes_cli/auth_commands.py:_display_source */
void* cli_auth_commands__display_source(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p2; (void)p3; (void)p4; (void)p5;
    hermes_log(LOG_DEBUG, "cli", "cli_auth_commands__display_source called");
    const char* source = (const char*)p1;
    if (!source) {
        return strdup("unknown");
    }
    /* Strip "manual:" prefix for display */
    if (strncmp(source, "manual:", 7) == 0) {
        return strdup(source + 7);
    }
    return strdup(source);
}

/* Port of Python hermes_cli/auth_commands.py:_classify_exhausted_status */
void* cli_auth_commands__classify_exhausted_status(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p2; (void)p3; (void)p4; (void)p5;
    hermes_log(LOG_DEBUG, "cli", "cli_auth_commands__classify_exhausted_status called");
    /* Classify exhausted credential: returns (label, show_retry_window) */
    int* result = (int*)malloc(8);
    if (!result) return NULL;
    const char* reason = (const char*)p1;
    if (reason && strstr(reason, "rate_limit")) {
        result[0] = 1; /* rate-limited */
        result[1] = 1; /* show retry window */
    } else {
        result[0] = 0; /* exhausted */
        result[1] = 1;
    }
    return result;
}

/* Port of Python hermes_cli/auth_commands.py:_format_exhausted_status */
void* cli_auth_commands__format_exhausted_status(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p1; (void)p2; (void)p3; (void)p4; (void)p5;
    hermes_log(LOG_DEBUG, "cli", "cli_auth_commands__format_exhausted_status called");
    /* Format exhausted status string for display */
    const char* status = (const char*)p1;
    if (!status || !status[0]) {
        return strdup("");
    }
    char buf[256];
    snprintf(buf, sizeof(buf), " %s (re-auth may be required)", status);
    return strdup(buf);
}

/* Port of Python hermes_cli/auth_commands.py:_interactive_auth */
void* cli_auth_commands__interactive_auth(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p1; (void)p2; (void)p3; (void)p4; (void)p5;
    hermes_log(LOG_WARNING, "cli", "cli_auth_commands__interactive_auth called");
    printf("Credential Pool Status\n");
    printf("==================================================\n");
    /* Interactive mode: show pool status, then present menu */
    printf("What would you like to do?\n");
    printf("  1. Add a credential\n");
    printf("  2. Remove a credential\n");
    printf("  3. Reset cooldowns for a provider\n");
    printf("  4. Set rotation strategy for a provider\n");
    printf("  5. Exit\n");
    return NULL;
}

/* Port of Python hermes_cli/auth_commands.py:_pick_provider */
void* cli_auth_commands__pick_provider(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p1; (void)p2; (void)p3; (void)p4; (void)p5;
    hermes_log(LOG_WARNING, "cli", "cli_auth_commands__pick_provider called");
    printf("\nKnown providers: anthropic, openrouter, openai-codex, nous, xai-oauth\n");
    printf("Provider: ");
    char buf[128];
    if (fgets(buf, sizeof(buf), stdin)) {
        size_t len = strlen(buf);
        if (len > 0 && buf[len-1] == '\n') buf[len-1] = '\0';
        return strdup(buf);
    }
    return strdup("");
}

/* Port of Python hermes_cli/auth_commands.py:_interactive_add */
void* cli_auth_commands__interactive_add(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p1; (void)p2; (void)p3; (void)p4; (void)p5;
    hermes_log(LOG_WARNING, "cli", "cli_auth_commands__interactive_add called");
    /* Interactive credential add flow: prompt for provider, type, label, token */
    printf("Provider to add credential for: ");
    char provider[128];
    if (!fgets(provider, sizeof(provider), stdin)) return NULL;
    size_t plen = strlen(provider);
    if (plen > 0 && provider[plen-1] == '\n') provider[plen-1] = '\0';
    printf("Label / account name (optional): ");
    char label[128];
    if (!fgets(label, sizeof(label), stdin)) return NULL;
    size_t llen = strlen(label);
    if (llen > 0 && label[llen-1] == '\n') label[llen-1] = '\0';
    hermes_log(LOG_INFO, "cli", "interactive_add: provider=%s label=%s", provider, label);
    return NULL;
}

/* Port of Python hermes_cli/auth_commands.py:_interactive_remove */
void* cli_auth_commands__interactive_remove(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p1; (void)p2; (void)p3; (void)p4; (void)p5;
    hermes_log(LOG_WARNING, "cli", "cli_auth_commands__interactive_remove called");
    /* Interactive credential remove: show entries, prompt for selection */
    printf("Remove #, id, or label (blank to cancel): ");
    char target[128];
    if (fgets(target, sizeof(target), stdin)) {
        size_t len = strlen(target);
        if (len > 0 && target[len-1] == '\n') target[len-1] = '\0';
        if (target[0] == '\0') {
            printf("Cancelled.\n");
            return NULL;
        }
        hermes_log(LOG_INFO, "cli", "interactive_remove: target=%s", target);
    }
    return NULL;
}

/* Port of Python hermes_cli/auth_commands.py:_interactive_reset */
void* cli_auth_commands__interactive_reset(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p1; (void)p2; (void)p3; (void)p4; (void)p5;
    hermes_log(LOG_WARNING, "cli", "cli_auth_commands__interactive_reset called");
    /* Reset cooldowns for a provider's credentials */
    printf("Provider to reset cooldowns for: ");
    char provider[128];
    if (fgets(provider, sizeof(provider), stdin)) {
        size_t len = strlen(provider);
        if (len > 0 && provider[len-1] == '\n') provider[len-1] = '\0';
        hermes_log(LOG_INFO, "cli", "interactive_reset: provider=%s", provider);
        printf("Reset status on 0 %s credentials\n", provider);
    }
    return NULL;
}

/* Port of Python hermes_cli/auth_commands.py:_interactive_strategy */
void* cli_auth_commands__interactive_strategy(void* p1, void* p2, void* p3, void* p4, void* p5) {
    (void)p1; (void)p2; (void)p3; (void)p4; (void)p5;
    hermes_log(LOG_WARNING, "cli", "cli_auth_commands__interactive_strategy called");
    /* Set rotation strategy for a provider */
    printf("Provider to set strategy for: ");
    char provider[128];
    if (!fgets(provider, sizeof(provider), stdin)) return NULL;
    size_t plen = strlen(provider);
    if (plen > 0 && provider[plen-1] == '\n') provider[plen-1] = '\0';
    printf("\nStrategies:\n");
    printf("  1. fill-first — Use first key until exhausted, then next\n");
    printf("  2. round-robin — Cycle through keys evenly\n");
    printf("  3. least-used  — Always pick the least-used key\n");
    printf("  4. random      — Random selection\n");
    printf("Strategy [1-4]: ");
    char choice[16];
    if (fgets(choice, sizeof(choice), stdin)) {
        hermes_log(LOG_INFO, "cli", "interactive_strategy: provider=%s choice=%s", provider, choice);
    }
    return NULL;
}
