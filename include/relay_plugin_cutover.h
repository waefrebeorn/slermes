/*
 * relay_plugin_cutover.h — C11 port of hermes_cli/relay_plugin_cutover.py
 *
 * Shared migration guards for Hermes' native NeMo Relay ownership.
 */
#ifndef SLERMES_RELAY_PLUGIN_CUTOVER_H
#define SLERMES_RELAY_PLUGIN_CUTOVER_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RELAY_PLUGINS_CONFIG_ENV "HERMES_NEMO_RELAY_PLUGINS_TOML"

/* legacy_relay_plugin_keys @ hermes_cli/relay_plugin_cutover.py:
 * legacy_relay_plugin_keys — sorted removed Relay plugin identities present
 * in values. Returns a malloc'd array of malloc'd strings (sorted); caller
 * frees each + the array. */
char **cutover_legacy_relay_plugin_keys(const char *const *values, size_t n,
                                        size_t *out_n);

/* configured_legacy_relay_env_vars @ ...:configured_legacy_relay_env_vars —
 * sorted non-empty legacy Relay exporter variables. env_names/env_vals are
 * parallel arrays describing the env mapping (NULL env → empty). */
char **cutover_configured_legacy_relay_env_vars(
    const char *const *env_names, const char *const *env_vals, size_t n,
    size_t *out_n);

#ifdef __cplusplus
}
#endif

#endif /* SLERMES_RELAY_PLUGIN_CUTOVER_H */
