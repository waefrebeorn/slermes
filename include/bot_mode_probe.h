#ifndef HERMES_BOT_MODE_PROBE_H
#define HERMES_BOT_MODE_PROBE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Bot Mode roster probe — port of tools/bot_mode_probe.py.
 * Pure filesystem + YAML + SHA-256 logic. All functions never raise. */

/* Resolve the agent's HERMES_HOME: explicit arg > HERMES_HOME env >
 * ~/.hermes > ~/.hermes. Writes into out (n bytes). Returns out. */
const char *bot_mode_probe_resolve_home(const char *home, char *out, size_t n);

/* Cached probe entry point — one filesystem pass per (home). */
const char *get_bot_mode_protocol_section(const char *home, int force_refresh);

/* 12-hex capability-epoch digest for the profile at home. */
char *capability_fingerprint(const char *home);

/* The epoch stamp appended to a Bot Chat prompt. Caller frees. */
char *epoch_line(const char *home);

/* True when stored_prompt's embedded epoch no longer matches disk state. */
int stored_prompt_capability_stale(const char *stored_prompt, const char *home);

/* True when a legacy (unstamped) Bot Chat prompt should be rebuilt. */
int stored_bot_chat_prompt_needs_upgrade(const char *stored_prompt, const char *home);

/* Clear the cached probe (test hook). */
void bot_mode_probe_reset_cache_for_tests(void);

/* Public _peers wrapper: registered peer gateway names, sorted, malloc'd
 * array of malloc'd strings (bot_mode_dm consumer). Free with
 * bot_mode_probe_peers_free. */
char **bot_mode_probe_peers(const char *root, size_t *count);
void   bot_mode_probe_peers_free(char **names, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* HERMES_BOT_MODE_PROBE_H */
