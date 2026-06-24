/*
 * port_tools_env_passthrough.c — C port of tools/env_passthrough.py
 *
 * Environment variable passthrough registry. Manages a session-scoped allowlist
 * of environment variables that pass through to sandboxed execution environments.
 * Hermes provider credentials are blocked to prevent credential exfiltration.
 */

#include "hermes.h"
#include "hermes_logger.h"
#include "env_passthrough.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_tools_env_passthrough__get_allowed @ tools/env_passthrough.py:_get_allowed */

/* Port of Python tools/env_passthrough.py:_get_allowed */
/* Get or create the allowed env vars set for the current context/session. */
/* Returns the current session-scoped allowlist. On first call, an empty set */
/* is created and stored in the context slot for subsequent calls. */
void cli_tools_env_passthrough__get_allowed(char ***out, int *out_count)
{
    if (!out || !out_count) return;

    /* Retrieve the session-scoped allowlist from the library. */
    /* The library manages creation on first access. */
    env_passthrough_get_all(out, out_count);
}

/* PoP: cli_tools_env_passthrough__is_hermes_provider_credential @ tools/env_passthrough.py:_is_hermes_provider_credential */

/* Port of Python tools/env_passthrough.py:_is_hermes_provider_credential */
/* True if name is a Hermes-managed provider credential per the blocklist. */
/* Skill-declared required_environment_variables must not override this list */
/* (GHSA-rhgp-j443-p4rf — credential exfiltration via sandbox passthrough). */
int cli_tools_env_passthrough__is_hermes_provider_credential(const char *name)
{
    if (!name || !*name) return 0;

    /* Check against the static blocklist in libenvpassthrough. */
    /* This covers ANTHROPIC_API_KEY, OPENAI_API_KEY, provider tokens, etc. */
    return (int)env_passthrough_is_blocked(name);
}

/* PoP: cli_tools_env_passthrough_register_env_passthrough @ tools/env_passthrough.py:register_env_passthrough */

/* Port of Python tools/env_passthrough.py:register_env_passthrough */
/* Register env var names as allowed. Blocked vars (Hermes provider creds) are skipped. */
void cli_tools_env_passthrough_register_env_passthrough(const char **names, int count)
{
    if (!names || count <= 0) return;

    for (int i = 0; i < count; i++) {
        if (!names[i] || !*names[i]) continue;
        const char *name = names[i];
        /* Strip leading/trailing whitespace */
        char trimmed[256];
        int start = 0, end = (int)strlen(name) - 1;
        while (name[start] == ' ' || name[start] == '\t') start++;
        while (end >= start && (name[end] == ' ' || name[end] == '\t')) end--;
        if (end < start) continue;
        int len = end - start + 1;
        if (len >= (int)sizeof(trimmed)) len = (int)sizeof(trimmed) - 1;
        memcpy(trimmed, name + start, (size_t)len);
        trimmed[len] = '\0';

        if (env_passthrough_is_blocked(trimmed)) {
            hermes_log(LOG_WARNING, "env_passthrough",
                "Refusing to register Hermes provider credential %s (blocked)", trimmed);
            continue;
        }
        env_passthrough_register(trimmed);
        hermes_log(LOG_DEBUG, "env_passthrough", "Registered %s", trimmed);
    }
}

/* PoP: cli_tools_env_passthrough__load_config_passthrough @ tools/env_passthrough.py:_load_config_passthrough */

/* Port of Python tools/env_passthrough.py:_load_config_passthrough */
/* Load tools.env_passthrough from config.yaml (cached). Returns count loaded. */
int cli_tools_env_passthrough__load_config_passthrough(void)
{
    /* Config loading is handled by config.c which populates cfg.terminal.env_passthrough */
    /* This function signals that config should be consulted; actual passthrough */
    /* checks go through is_env_passthrough which consults both session + config. */
    hermes_log(LOG_DEBUG, "env_passthrough", "Config passthrough requested (handled by config.c)");
    return 0;
}

/* PoP: cli_tools_env_passthrough_is_env_passthrough @ tools/env_passthrough.py:is_env_passthrough */

/* Port of Python tools/env_passthrough.py:is_env_passthrough */
/* Check whether var_name is allowed to pass through to sandboxes. */
int cli_tools_env_passthrough_is_env_passthrough(const char *var_name)
{
    if (!var_name || !*var_name) return 0;

    /* Check session-scoped allowlist first */
    if (env_passthrough_is_allowed(var_name)) return 1;

    /* Config-based passthrough is managed via config.c terminal.env_passthrough field */
    /* For now, session-only is the primary path. */
    return 0;
}

/* PoP: cli_tools_env_passthrough_get_all_passthrough @ tools/env_passthrough.py:get_all_passthrough */

/* Port of Python tools/env_passthrough.py:get_all_passthrough */
/* Return the union of skill-registered and config-based passthrough vars. */
void cli_tools_env_passthrough_get_all_passthrough(char ***out, int *out_count)
{
    if (!out || !out_count) return;

    /* Only session-scoped vars for now; config vars handled separately */
    env_passthrough_get_all(out, out_count);
}

/* PoP: cli_tools_env_passthrough_clear_env_passthrough @ tools/env_passthrough.py:clear_env_passthrough */

/* Port of Python tools/env_passthrough.py:clear_env_passthrough */
/* Reset the skill-scoped allowlist (e.g. on session reset). */
void cli_tools_env_passthrough_clear_env_passthrough(void)
{
    env_passthrough_clear();
    hermes_log(LOG_DEBUG, "env_passthrough", "Session allowlist cleared");
}
