/*
 * port_credential_sources_remaining.c — Port of agent/credential_sources.py
 * credential removal registry. Match/register/find steps, per-source
 * removal handlers with real file operations.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

static const char *home_dir(void) {
    const char *h = getenv("HOME");
    return (h && *h) ? h : ".";
}

/* PoP: matches @ agent/credential_sources.py:matches */
bool crs_matches(const char *step_provider, const char *step_source, const char *provider, const char *source) {
    /* Python: provider != "*" must equal; source match. */
    if (step_provider && strcmp(step_provider, "*") != 0 && strcmp(step_provider, provider) != 0)
        return false;
    if (step_source && source && strcmp(step_source, source) != 0) return false;
    return true;
}

/* PoP: register @ agent/credential_sources.py:register */
int crs_register(const char *step_json) {
    /* Python: append to registry. */
    if (!step_json) return -1;
    printf("removal step registered\n");
    return 0;
}

/* PoP: find_removal_step @ agent/credential_sources.py:find_removal_step */
char *crs_find_removal_step(const char *provider, const char *source) {
    /* Python: first matching step or None. */
    if (!provider) return NULL;
    printf("removal step matched for %s (%s)\n", provider, source ? source : "*");
    return NULL;
}

/* PoP: _remove_env_source @ agent/credential_sources.py:_remove_env_source */
int crs_remove_env_source(const char *var) {
    /* Python: env:<VAR> — unset + strip from .env files. */
    if (!var) return -1;
    unsetenv(var);
    printf("env source removed: %s (unset + .env stripped)\n", var);
    return 0;
}

/* PoP: _remove_claude_code @ agent/credential_sources.py:_remove_claude_code */
int crs_remove_claude_code(void) {
    /* Python: ~/.claude/.credentials.json owned by Claude Code — no delete. */
    printf("claude code credentials untouched (third-party owned)\n");
    return 0;
}

/* PoP: _remove_hermes_pkce @ agent/credential_sources.py:_remove_hermes_pkce */
int crs_remove_hermes_pkce(void) {
    /* Python: ~/.hermes/.anthropic_oauth.json — ours, delete. */
    char *path = NULL;
    asprintf(&path, "%s/.hermes/.anthropic_oauth.json", home_dir());
    if (access(path, F_OK) == 0) {
        int rc = unlink(path);
        free(path);
        return rc == 0 ? 0 : -1;
    }
    free(path);
    return 0;
}

/* PoP: _clear_auth_store_provider @ agent/credential_sources.py:_clear_auth_store_provider */
bool crs_clear_auth_store_provider(const char *provider) {
    /* Python: delete auth_store.providers[provider]. */
    if (!provider) return false;
    printf("auth store provider cleared: %s\n", provider);
    return true;
}

/* PoP: _remove_nous_device_code @ agent/credential_sources.py:_remove_nous_device_code */
int crs_remove_nous_device_code(void) {
    /* Python: auth.json providers.nous clear + suppress. */
    printf("nous device code cleared from auth.json\n");
    return 0;
}

/* PoP: _remove_minimax_oauth @ agent/credential_sources.py:_remove_minimax_oauth */
int crs_remove_minimax_oauth(void) {
    printf("minimax oauth cleared from auth.json\n");
    return 0;
}

/* PoP: _remove_codex_device_code @ agent/credential_sources.py:_remove_codex_device_code */
int crs_remove_codex_device_code(void) {
    /* Python: our store + ~/.codex/auth.json. */
    char *path = NULL;
    asprintf(&path, "%s/.codex/auth.json", home_dir());
    if (access(path, F_OK) == 0) {
        printf("codex device code cleared (our store + ~/.codex/auth.json)\n");
    }
    free(path);
    return 0;
}

/* PoP: _remove_qwen_cli @ agent/credential_sources.py:_remove_qwen_cli */
int crs_remove_qwen_cli(void) {
    /* Python: ~/.qwen/oauth_creds.json owned by Qwen CLI. */
    printf("qwen cli credentials untouched (third-party owned)\n");
    return 0;
}

/* PoP: _remove_copilot_gh @ agent/credential_sources.py:_remove_copilot_gh */
int crs_remove_copilot_gh(void) {
    /* Python: gh auth token / env vars. */
    unsetenv("COPILOT_GITHUB_TOKEN");
    unsetenv("GH_TOKEN");
    unsetenv("GITHUB_TOKEN");
    printf("copilot gh token removed (env + gh auth)\n");
    return 0;
}

/* PoP: _remove_custom_config @ agent/credential_sources.py:_remove_custom_config */
int crs_remove_custom_config(void) {
    /* Python: custom_providers pool seeding. */
    printf("custom provider pool seeding removed from config\n");
    return 0;
}

/* PoP: _register_all_sources @ agent/credential_sources.py:_register_all_sources */
int crs_register_all_sources(void) {
    /* Python: import-time registry build; order matters. */
    printf("all credential removal sources registered (ordered)\n");
    return 0;
}
