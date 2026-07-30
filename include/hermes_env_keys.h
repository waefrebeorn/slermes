/*
 * hermes_env_keys.h — .env API key management for Slermes C
 * Port of Python Hermes' setup.py key management functionality.
 * Phase 569+: S0a #2 .env key wizard parity gap.
 */
#ifndef HERMES_ENV_KEYS_H
#define HERMES_ENV_KEYS_H

#include <stdbool.h>
#include <stddef.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Provider-to-env-var mapping table entry */
typedef struct {
    const char *provider;   /* canonical provider name */
    const char *env_var;    /* env var name (e.g. "OPENAI_API_KEY") */
    const char *prefix;     /* expected key prefix hint (e.g. "sk-") */
} provider_key_map_t;

/* Known provider→env_var mappings (null-terminated array) */
extern const provider_key_map_t PROVIDER_KEY_MAP[];

/* Find provider by name → returns provider_key_map_t* or NULL */
const provider_key_map_t *key_map_find(const char *provider);

/* Find provider by env var name → returns provider_key_map_t* or NULL */
const provider_key_map_t *key_map_find_by_env(const char *env_var);

/* List all provider keys (with masked values) to stdout */
void key_list_all(void);

/* Set a provider's API key in .env file
 * Returns 0 on success, -1 on failure. */
int key_set(const char *hermes_home, const char *provider, const char *value);

/* Remove a provider's API key from .env file
 * Returns 0 on success, -1 on failure. */
int key_unset(const char *hermes_home, const char *provider);

/* Show a provider's key (masked) to stdout */
void key_show(const char *provider);

/* Interactive wizard: prompt to set a specific key */
int key_wizard(const char *hermes_home, const char *provider);

/* Get path to .env file */
const char *key_env_path(const char *hermes_home, char *buf, size_t bufsz);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_ENV_KEYS_H */
