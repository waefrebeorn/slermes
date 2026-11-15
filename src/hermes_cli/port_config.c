/* port_config.c -- pure helpers ported from hermes_cli/config.py.
 *
 * PoP: cfg_split_key_path        @ hermes_cli/config.py:_split_key_path
 * PoP: cfg_env_var_policy_name   @ hermes_cli/config.py:_env_var_policy_name
 * PoP: cfg_validate_env_var_name @ hermes_cli/config.py:validate_env_var_name_for_write
 * PoP: cfg_is_env_config_key     @ hermes_cli/config.py:_is_env_config_key
 *
 * Only the string-only parts are ported here. The dict-navigation cluster
 * (_set_nested/_get_nested/_unset_nested/_locate_nested) needs the JSON object
 * model and is a separate unit.
 */
#include "port_config.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* ── _split_key_path ────────────────────────────────────────────────────────
 * Split a dotted config-key path, honouring backslash-escaped dots
 * (``a\.b`` -> one segment ``a.b``). A backslash before any other character is
 * preserved verbatim.
 *
 * Returns the number of segments written to `out` (each NUL-terminated), or -1
 * if `out` cannot hold them. `max` is the segment capacity, not a byte count.
 */
int
cfg_split_key_path(const char *key, char **out, int max)
{
    if (!key || !out || max <= 0)
        return -1;

    size_t klen = strlen(key);
    /* One segment can never exceed the key length. */
    char *scratch = (char *)malloc(klen + 1);
    if (!scratch)
        return -1;

    int n = 0;
    size_t w = 0;                /* write cursor inside scratch */
    const char *p = key;

    for (;;) {
        if (*p == '\\' && p[1] == '.') {
            /* escaped dot: a literal '.' inside the CURRENT segment */
            scratch[w++] = '.';
            p += 2;
            continue;
        }
        if (*p == '.') {
            if (n >= max) { free(scratch); return -1; }
            scratch[w] = '\0';
            out[n] = strdup(scratch);
            if (!out[n]) { free(scratch); return -1; }
            n++;
            w = 0;
            p++;
            continue;
        }
        if (*p == '\0') {
            if (n >= max) { free(scratch); return -1; }
            scratch[w] = '\0';
            out[n] = strdup(scratch);
            if (!out[n]) { free(scratch); return -1; }
            free(scratch);
            return n + 1;
        }
        scratch[w++] = *p++;
    }
}

/* ── _env_var_policy_name ──────────────────────────────────────────────────
 * Windows env names are case-insensitive, POSIX are not: the name used for
 * policy comparisons is upper-cased on Windows only.
 */
void
cfg_env_var_policy_name(const char *key, int is_windows, char *out, size_t cap)
{
    if (!key || !out || cap == 0)
        return;
    size_t i = 0;
    for (; key[i] && i + 1 < cap; i++)
        out[i] = (char)(is_windows ? (unsigned char)toupper((unsigned char)key[i])
                                : (unsigned char)key[i]);
    out[i] = '\0';
}

/* ── _is_env_config_key ─────────────────────────────────────────────────────
 * Whether `hermes config set` routes this credential-shaped key to .env.
 * Dotted keys are never routed (they address nested config, not env).
 */
static const char *const ENV_CONFIG_KEYS[] = {
    "OPENROUTER_API_KEY", "OPENAI_API_KEY", "ANTHROPIC_API_KEY",
    "VOICE_TOOLS_OPENAI_KEY", "EXA_API_KEY", "PARALLEL_API_KEY",
    "FIRECRAWL_API_KEY", "FIRECRAWL_API_URL", "FIRECRAWL_GATEWAY_URL",
    "TOOL_GATEWAY_URL", "CONNECTOR_GATEWAY_URL", "TOOL_GATEWAY_DOMAIN",
    "TOOL_GATEWAY_SCHEME", "TOOL_GATEWAY_USER_TOKEN", "TAVILY_API_KEY",
    "PERPLEXITY_API_KEY", "API_SERVER_KEY", "BROWSERBASE_API_KEY",
    "BROWSERBASE_PROJECT_ID", "BROWSER_USE_API_KEY", "FAL_KEY",
    "TELEGRAM_BOT_TOKEN", "DISCORD_BOT_TOKEN", "TERMINAL_SSH_HOST",
    "TERMINAL_SSH_USER", "TERMINAL_SSH_KEY", "SUDO_PASSWORD",
    "SLACK_BOT_TOKEN", "SLACK_APP_TOKEN", "GITHUB_TOKEN", "HONCHO_API_KEY",
};

int
cfg_is_env_config_key(const char *key)
{
    if (!key || strchr(key, '.'))
        return 0;

    char upper[256];
    size_t i = 0;
    for (; key[i] && i + 1 < sizeof(upper); i++)
        upper[i] = (char)toupper((unsigned char)key[i]);
    upper[i] = '\0';

    for (size_t k = 0; k < sizeof(ENV_CONFIG_KEYS) / sizeof(ENV_CONFIG_KEYS[0]); k++)
        if (strcmp(upper, ENV_CONFIG_KEYS[k]) == 0)
            return 1;

    static const char *const suffixes[] = {"_API_KEY", "_TOKEN", "_SECRET"};
    for (size_t k = 0; k < sizeof(suffixes) / sizeof(suffixes[0]); k++) {
        size_t sl = strlen(suffixes[k]);
        if (strlen(upper) >= sl &&
            strcmp(upper + strlen(upper) - sl, suffixes[k]) == 0)
            return 1;
    }
    return strncmp(upper, "TERMINAL_SSH", 12) == 0;
}

/* ── validate_env_var_name_for_write ───────────────────────────────────────
 * Result: CFG_ENV_OK, CFG_ENV_INVALID_NAME, or CFG_ENV_DENIED.
 * The external `auth_external_process.env_writer_launch_denylist` is not
 * modelled here; the built-in denylist and prefixes are.
 */
static const char *const ENV_DENY_PREFIXES[] = {"LD_", "DYLD_", "GIT_CONFIG_"};

static const char *const ENV_DENYLIST[] = {
    "LD_PRELOAD", "LD_LIBRARY_PATH", "LD_AUDIT", "LD_DEBUG",
    "DYLD_INSERT_LIBRARIES", "DYLD_LIBRARY_PATH", "DYLD_FRAMEWORK_PATH",
    "DYLD_FALLBACK_LIBRARY_PATH", "DYLD_FALLBACK_FRAMEWORK_PATH",
    "PYTHONPATH", "PYTHONHOME", "PYTHONSTARTUP", "PYTHONUSERBASE",
    "PYTHONEXECUTABLE", "PYTHONNOUSERSITE", "PYTHONBREAKPOINT", "PYTHONCASEOK",
    "NODE_OPTIONS", "NODE_PATH",
    "PERL5OPT", "PERL5LIB", "PERLLIB", "RUBYOPT", "RUBYLIB", "CLASSPATH",
    "JAVA_TOOL_OPTIONS", "_JAVA_OPTIONS", "JDK_JAVA_OPTIONS",
    "GOFLAGS", "RUSTFLAGS",
    "PATH", "SHELL", "BROWSER", "EDITOR", "VISUAL", "PAGER", "MANPAGER",
    "GIT_SSH_COMMAND", "GIT_EXEC_PATH", "GIT_SHELL",
    "GIT_SSH", "GIT_ASKPASS", "SSH_ASKPASS", "SUDO_ASKPASS",
    "GIT_EDITOR", "GIT_SEQUENCE_EDITOR", "GIT_PAGER", "GIT_EXTERNAL_DIFF",
    "GIT_PROXY_COMMAND", "GIT_TEMPLATE_DIR", "GIT_DIR",
    "BASH_ENV", "ENV", "ZDOTDIR", "PROMPT_COMMAND", "VIMINIT", "EXINIT",
    "HERMES_HOME", "HERMES_PROFILE", "HERMES_CONFIG", "HERMES_ENV",
    "HERMES_CONFIG_PATH", "HERMES_ENV_PATH",
    "HERMES_OPTIONAL_MCPS",
    "HERMES_YOLO_MODE", "HERMES_ACCEPT_HOOKS", "HERMES_REDACT_SECRETS",
    "HERMES_INTERACTIVE", "HERMES_EXEC_ASK", "HERMES_GATEWAY_SESSION",
    "HERMES_CRON_SESSION", "HERMES_SINGLE_QUERY_SESSION",
    "HERMES_SESSION_KEY", "HERMES_SESSION_PLATFORM",
};

static int
env_name_shape_ok(const char *key)
{
    if (!key || !*key)
        return 0;
    if (!isalpha((unsigned char)key[0]) && key[0] != '_')
        return 0;
    for (const char *p = key + 1; *p; p++)
        if (!isalnum((unsigned char)*p) && *p != '_')
            return 0;
    return 1;
}

int
cfg_validate_env_var_name(const char *key, int is_windows)
{
    if (!env_name_shape_ok(key))
        return CFG_ENV_INVALID_NAME;

    char policy[512];
    cfg_env_var_policy_name(key, is_windows, policy, sizeof(policy));

    for (size_t k = 0; k < sizeof(ENV_DENYLIST) / sizeof(ENV_DENYLIST[0]); k++)
        if (strcmp(policy, ENV_DENYLIST[k]) == 0)
            return CFG_ENV_DENIED;
    for (size_t k = 0; k < sizeof(ENV_DENY_PREFIXES) / sizeof(ENV_DENY_PREFIXES[0]); k++)
        if (strncmp(policy, ENV_DENY_PREFIXES[k], strlen(ENV_DENY_PREFIXES[k])) == 0)
            return CFG_ENV_DENIED;
    return CFG_ENV_OK;
}
