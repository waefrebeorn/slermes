/* port_config.c -- pure helpers ported from hermes_cli/config.py.
 *
 * PoP: cfg_split_key_path        @ hermes_cli/config.py:split_key_path
 * PoP: cfg_env_var_policy_name   @ hermes_cli/config.py:env_var_policy_name
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

#include "libjson/json.h"

/* ── split_key_path ────────────────────────────────────────────────────────
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

/* ── env_var_policy_name ──────────────────────────────────────────────────
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

/* ── install-method helpers ──────────────────────────────────────────────
 *
 * PoP: cfg_is_nix_install_method            @ hermes_cli/config.py:is_nix_install_method
 * PoP: cfg_recommended_update_command_for_method @ hermes_cli/config.py:recommended_update_command_for_method
 * PoP: cfg_is_uv_tool_install               @ hermes_cli/config.py:is_uv_tool_install
 * PoP: cfg_is_unsupported_install_method    @ hermes_cli/config.py:is_unsupported_install_method
 * PoP: cfg_format_unsupported_install_warning @ hermes_cli/config.py:format_unsupported_install_warning
 *
 * format_docker_update_message and format_managed_message are NOT here: they
 * are already ported and linked in src/hermes_cli/config_ports.c, and a second
 * definition would collide at link time.
 *
 * detect_install_method / recommended_update_command are NOT here: they
 * read the filesystem and the package-manager marker at call time.
 */

static const char *const NIX_UPDATE_MSG =
    "Update Hermes through the Nix source that installed it (e.g. nix profile upgrade, or update your flake input and rebuild with nixos-rebuild or home-manager switch)";

/* These three are deliberate shims in the Python: the old updater is
 * disabled until relaunch. Ported as the constants they are, not as
 * re-derived logic. */
int
cfg_is_uv_tool_install(void)
{
    return 0;
}

int
cfg_is_unsupported_install_method(const char *method)
{
    (void)method;
    return 0;
}

void
cfg_format_unsupported_install_warning(const char *method, char *out, size_t cap)
{
    (void)method;
    if (out && cap)
        out[0] = '\0';
}

int
cfg_is_nix_install_method(const char *method)
{
    static const char *const nix_systems[] = {
        "home-manager",
        "nixos",
    };
    if (!method)
        return 0;
    if (strcmp(method, "nix") == 0)
        return 1;
    for (size_t i = 0; i < sizeof(nix_systems) / sizeof(nix_systems[0]); i++)
        if (strcmp(method, nix_systems[i]) == 0)
            return 1;
    return 0;
}

void
cfg_recommended_update_command_for_method(const char *method, char *out, size_t cap)
{
    static const struct { const char *k, *v; } table[] = {
        {"apt", "pkg upgrade hermes-agent"},
        {"docker", "docker pull nousresearch/hermes-agent:latest"},
    };
    if (!out || cap == 0)
        return;
    if (cfg_is_nix_install_method(method)) {
        snprintf(out, cap, "%s", NIX_UPDATE_MSG);
        return;
    }
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++)
        if (strcmp(method, table[i].k) == 0) {
            snprintf(out, cap, "%s", table[i].v);
            return;
        }
    snprintf(out, cap, "%s", "hermes update");
}

/* ── config-structure validation ───────────────────────────────────────────
 * PoP: ac_require_fields            @ hermes_cli/config.py:_require_fields
 * PoP: ac_validate_entry_list       @ hermes_cli/config.py:_validate_entry_list
 * PoP: ac_validate_custom_providers @ hermes_cli/config.py:_validate_custom_providers
 * PoP: ac_validate_fallback_model   @ hermes_cli/config.py:_validate_fallback_model
 *
 * Issues are modelled as a JSON array of {severity, message, hint}, mirroring
 * ConfigIssue. validate_config_structure itself is NOT here: it loads config
 * from disk.
 */

static void
ac_cfg_issue(json_t *issues, const char *severity, const char *message,
             const char *hint)
{
    if (!issues || issues->type != JSON_ARRAY)
        return;
    json_t *e = json_object();
    if (!e)
        return;
    json_set(e, "severity", json_string(severity ? severity : ""));
    json_set(e, "message", json_string(message ? message : ""));
    json_set(e, "hint", json_string(hint ? hint : ""));
    json_append(issues, e);
}

/* Python type(x).__name__ for a JSON value. */
static const char *
ac_json_typename(const json_t *v)
{
    if (!v)
        return "NoneType";
    switch (v->type) {
    case JSON_OBJECT: return "dict";
    case JSON_ARRAY:  return "list";
    case JSON_STRING: return "str";
    case JSON_NUMBER: return "int";      /* Python ints are the common case */
    case JSON_BOOL:   return "bool";
    case JSON_NULL:   return "NoneType";
    }
    return "unknown";
}

/* Substitute {i} and {type} in a message template. Caller frees. */
static char *
ac_fmt_msg(const char *tpl, int i, const char *type)
{
    size_t cap = strlen(tpl) + 64;
    char *out = (char *)malloc(cap);
    if (!out)
        return NULL;
    size_t w = 0;
    for (const char *p = tpl; *p && w + 1 < cap; p++) {
        if (p[0] == '{' && p[1] == 'i' && p[2] == '}') {
            w += (size_t)snprintf(out + w, cap - w, "%d", i);
            p += 2;
        } else if (p[0] == '{' && p[1] == 't' && p[2] == 'y' && p[3] == 'p' &&
                   p[4] == 'e' && p[5] == '}') {
            w += (size_t)snprintf(out + w, cap - w, "%s", type);
            p += 5;
        } else {
            out[w++] = *p;
        }
    }
    out[w] = '\0';
    return out;
}

void
ac_require_fields(json_t *issues, json_t *entry, const char *label,
                  const char **fields, int nfields, const char *suffix)
{
    if (!entry || entry->type != JSON_OBJECT)
        return;
    for (int i = 0; i < nfields; i++) {
        const char *field = fields[2 * i];
        const char *hint = fields[2 * i + 1];
        json_t *v = json_obj_get(entry, field);
        int falsy = (!v) ||
                    (v->type == JSON_NULL) ||
                    (v->type == JSON_STRING && (!v->str_val || !v->str_val[0])) ||
                    (v->type == JSON_ARRAY && v->c.count == 0) ||
                    (v->type == JSON_OBJECT && v->c.count == 0) ||
                    (v->type == JSON_BOOL && !v->bool_val);
        if (falsy) {
            char msg[1024];
            snprintf(msg, sizeof(msg), "%s is missing '%s' field%s",
                     label ? label : "", field, suffix ? suffix : "");
            ac_cfg_issue(issues, "warning", msg, hint ? hint : "");
        }
    }
}

void
ac_validate_entry_list(json_t *issues, json_t *entries, const char *label,
                       const char **fields, int nfields,
                       const char *severity, const char *message,
                       const char *hint)
{
    if (!entries || entries->type != JSON_ARRAY)
        return;
    for (size_t i = 0; i < entries->c.count; i++) {
        json_t *entry = entries->c.items[i];
        if (!entry || entry->type != JSON_OBJECT) {
            char *m = ac_fmt_msg(message ? message : "", (int)i,
                                 ac_json_typename(entry));
            ac_cfg_issue(issues, severity ? severity : "warning",
                         m ? m : "", hint ? hint : "");
            free(m);
        } else {
            char lbl[512];
            snprintf(lbl, sizeof(lbl), "%s[%d]", label ? label : "", (int)i);
            ac_require_fields(issues, entry, lbl, fields, nfields, "");
        }
    }
}

static const char *const CP_REQUIRED_FIELDS[] = {
    "name", "Add a name, e.g.: name: my-provider",
    "base_url", "Add the API endpoint URL, e.g.: base_url: https://api.example.com/v1",
};
static const char *const FB_REQUIRED_FIELDS[] = {
    "provider", "Add: provider: openrouter (or another provider)",
    "model", "Add: model: <model-name>",
};
static const char *const FB_SINGLE_REQUIRED_FIELDS[] = {
    "provider", "Add: provider: openrouter (or another provider)",
    "model", "Add: model: anthropic/claude-sonnet-4 (or another model)",
};

static const char *const CP_LIST_HINT =
    "Change to:\n  custom_providers:\n    - name: my-provider\n"
    "      base_url: https://...\n      api_key: ...";

void
ac_validate_custom_providers(json_t *issues, json_t *cp)
{
    static const char *const like_fields[] =
        {"base_url", "api_key", "rate_limit_delay", "api_mode"};

    if (!cp) {
        /* Absent key: the Python never sees it (the caller .get()s it). */
        return;
    }
    if (cp->type == JSON_OBJECT) {
        ac_cfg_issue(issues, "error",
                     "custom_providers is a dict — it must be a YAML list "
                     "(items prefixed with '-')", CP_LIST_HINT);
        /* suspicious = sorted(set(cp) & LIKE_FIELDS) -- sorted, not table order */
        const char *hits[sizeof(like_fields) / sizeof(like_fields[0])];
        size_t nh = 0;
        for (size_t k = 0; k < sizeof(like_fields) / sizeof(like_fields[0]); k++)
            if (json_obj_get(cp, like_fields[k]))
                hits[nh++] = like_fields[k];
        for (size_t a = 1; a < nh; a++) {
            const char *cur = hits[a];
            size_t b = a;
            while (b > 0 && strcmp(hits[b - 1], cur) > 0) {
                hits[b] = hits[b - 1];
                b--;
            }
            hits[b] = cur;
        }
        char buf[512];
        size_t w = (size_t)snprintf(buf, sizeof(buf), "[");
        for (size_t k = 0; k < nh; k++)
            w += (size_t)snprintf(buf + w, sizeof(buf) - w, "%s'%s'",
                                  k ? ", " : "", hits[k]);
        snprintf(buf + w, sizeof(buf) - w, "]");
        if (nh > 0) {
            char msg[1024];
            snprintf(msg, sizeof(msg),
                     "Root-level keys %s look like custom_providers entry fields",
                     buf);
            ac_cfg_issue(issues, "warning", msg,
                         "These should be indented under a '- name: ...' list "
                         "entry, not at root level");
        }
    } else if (cp->type == JSON_ARRAY) {
        ac_validate_entry_list(issues, cp, "custom_providers",
                               CP_REQUIRED_FIELDS, 2,
                               "warning",
                               "custom_providers[{i}] is not a dict (got {type})",
                               "Each entry should have at minimum: name, base_url");
    } else {
        char msg[512];
        snprintf(msg, sizeof(msg),
                 "custom_providers is a %s — it must be a YAML list (items "
                 "prefixed with '-'); legacy custom_providers entries are "
                 "ignored until it is", ac_json_typename(cp));
        ac_cfg_issue(issues, "error", msg, CP_LIST_HINT);
    }
}

void
ac_validate_fallback_model(json_t *issues, json_t *fb)
{
    static const char *const CHANGE_HINT =
        "Change to:\n  fallback_model:\n    provider: openrouter\n"
        "    model: anthropic/claude-sonnet-4";

    if (!fb)
        return;
    if (fb->type == JSON_ARRAY) {
        ac_validate_entry_list(issues, fb, "fallback_model",
                               FB_REQUIRED_FIELDS, 2,
                               "error",
                               "fallback_model[{i}] should be a dict, got {type}",
                               "Each entry needs provider + model");
    } else if (fb->type != JSON_OBJECT) {
        char msg[512];
        snprintf(msg, sizeof(msg),
                 "fallback_model should be a dict with 'provider' and 'model', "
                 "got %s", ac_json_typename(fb));
        ac_cfg_issue(issues, "error", msg, CHANGE_HINT);
    } else if (fb->c.count > 0) {
        ac_require_fields(issues, fb, "fallback_model",
                          FB_SINGLE_REQUIRED_FIELDS, 2,
                          " — fallback will be disabled");
    }
}
