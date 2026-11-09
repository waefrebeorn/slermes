/*
 * credential_remove.c - credential-source removal helpers.
 *
 * Split from credential_sources.c (soul.md: self-contained modules).
 * Removal-result bookkeeping, source matching and per-source removers.
 * No file-scope state.
 */
#include "credential_remove.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>



/* ================================================================
 *  RemovalResult / RemovalStep types
 * ================================================================ */

void removal_result_init(removal_result_t *r) {
    if (!r) return;
    memset(r, 0, sizeof(*r));
    r->suppress = true;
}


void removal_result_add_cleaned(removal_result_t *r, const char *msg) {
    if (!r || !msg) return;
    if (r->cleaned_count < REMOVAL_RESULT_MAX_LINES) {
        r->cleaned[r->cleaned_count++] = strdup(msg);
    }
}


void removal_result_add_hint(removal_result_t *r, const char *msg) {
    if (!r || !msg) return;
    if (r->hints_count < REMOVAL_RESULT_MAX_LINES) {
        r->hints[r->hints_count++] = strdup(msg);
    }
}


void removal_result_free(removal_result_t *r) {
    if (!r) return;
    for (int i = 0; i < r->cleaned_count; i++) free(r->cleaned[i]);
    for (int i = 0; i < r->hints_count; i++) free(r->hints[i]);
    memset(r, 0, sizeof(*r));
}


/* Port of Python agent/credential_sources.py:_remove_claude_code(). */ removal_result_t remove_claude_code(const char *provider, const char *source) {
    (void)provider; (void)source;
    removal_result_t result;
    removal_result_init(&result);
    removal_result_add_hint(&result, "Suppressed claude_code credential — it will not be re-seeded.");
    removal_result_add_hint(&result, "Note: Claude Code credentials still live in ~/.claude/.credentials.json");
    removal_result_add_hint(&result, "Run `hermes auth add anthropic` to re-enable if needed.");
    return result;
}


/* Port of Python agent/credential_sources.py:_remove_hermes_pkce(). */ removal_result_t remove_hermes_pkce(const char *provider, const char *source) {
    (void)provider; (void)source;
    removal_result_t result;
    removal_result_init(&result);

    const char *home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) return result;

    char path[1024];
    snprintf(path, sizeof(path), "%s/.hermes/.anthropic_oauth.json", home);

    if (access(path, F_OK) == 0) {
        if (unlink(path) == 0) {
            removal_result_add_cleaned(&result, "Cleared Hermes Anthropic OAuth credentials");
        } else {
            char hint[512];
            snprintf(hint, sizeof(hint), "Could not delete %s", path);
            removal_result_add_hint(&result, hint);
        }
    }

    return result;
}


/* Port of Python agent/credential_sources.py:_remove_qwen_cli(). */ removal_result_t remove_qwen_cli(const char *provider, const char *source) {
    (void)provider; (void)source;
    removal_result_t result;
    removal_result_init(&result);
    removal_result_add_hint(&result, "Suppressed qwen-cli credential — it will not be re-seeded.");
    removal_result_add_hint(&result, "Note: Qwen CLI credentials still live in ~/.qwen/oauth_creds.json");
    removal_result_add_hint(&result, "Run `hermes auth add qwen-oauth` to re-enable if needed.");
    return result;
}


/* Port of Python agent/credential_sources.py:_remove_custom_config(). */ removal_result_t remove_custom_config(const char *provider, const char *source) {
    (void)provider;
    removal_result_t result;
    removal_result_init(&result);

    char hint[512];
    snprintf(hint, sizeof(hint), "Suppressed %s — it will not be re-seeded.", source);
    removal_result_add_hint(&result, hint);
    removal_result_add_hint(&result,
        "Note: The underlying value in config.yaml is unchanged.  Edit it directly if you want to remove the credential from disk.");

    return result;
}


/* ================================================================
 *  Match functions
 * ================================================================ */ bool match_env_source(const char *source) {
    return (source && strncmp(source, "env:", 4) == 0);
}
 bool match_copilot_source(const char *source) {
    if (!source) return false;
    return (strcmp(source, "gh_cli") == 0 || strncmp(source, "env:", 4) == 0);
}
 bool match_device_code(const char *source) {
    if (!source) return false;
    return (strcmp(source, "device_code") == 0 || strstr(source, ":device_code") != NULL);
}
 bool match_config_source(const char *source) {
    if (!source) return false;
    return (strncmp(source, "config:", 7) == 0 || strcmp(source, "model_config") == 0);
}
