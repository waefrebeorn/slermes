/*
 * config_setup.c — Interactive + non-interactive setup wizard for Slermes.
 *
 * Single-concern extraction from the monolithic config.c. Contains:
 *   - hermes_config_setup_reload / hermes_config_check_reload (SIGHUP hot-reload)
 *   - hermes_config_setup_interactive / _noninteractive / _section / _quick / _portal
 *   - all private static setup helpers (setup_prompt_*, setup_agent_settings,
 *     setup_tts, setup_telegram, setup_slack, ...)
 *
 * Calls into the core config API (hermes_config_load/defaults/export/...) which
 * remains in config.c; those symbols resolve at link time. Keeps config.c
 * focused on load/validate/diff/merge/migrate, and this file on user onboarding.
 */

#include "hermes_core_types.h"
#include "config_schema.h"
#include "hermes_yaml.h"
#include "hermes_json.h"
#include "hermes_auth.h"
#include "provider_metadata.h"
#include "curses_widget.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <sys/stat.h>


/* SIGHUP reload flag — shared between the reload handler and hermes_config_check_reload. */
static volatile sig_atomic_t g_config_reload_requested = 0;

static void config_sighup_handler(int sig) {
    (void)sig;
    g_config_reload_requested = 1;
}


void hermes_config_setup_reload(void) {
    signal(SIGHUP, config_sighup_handler);
}

bool hermes_config_check_reload(hermes_config_t *cfg, const char *config_dir) {
    if (!g_config_reload_requested)
        return false;
    g_config_reload_requested = 0;

    fprintf(stderr, "SIGHUP received — reloading config...\n");

    /* Save current config as fallback */
    hermes_config_t old = *cfg;

    /* Reload — this resets to defaults then overlays YAML + env */
    if (!hermes_config_load(cfg, config_dir)) {
        fprintf(stderr, "Config reload FAILED — restoring previous config\n");
        *cfg = old;
        return false;
    }

    fprintf(stderr, "Config reloaded successfully.\n");
    return true;
}

/* U01: Config init — create default config.yaml + .env template */
bool hermes_config_init(const char *config_dir) {
    char dir[4096];
    if (config_dir && config_dir[0]) {
        snprintf(dir, sizeof(dir), "%s", config_dir);
    } else {
        const char *home = getenv("SLERMES_HOME");
        if (!home) home = getenv("HERMES_HOME");
        if (!home) home = getenv("HOME");
        if (!home) { fprintf(stderr, "Error: cannot determine home.\n"); return false; }
        if (getenv("SLERMES_HOME") || getenv("HERMES_HOME"))
            snprintf(dir, sizeof(dir), "%s", home);
        else
            snprintf(dir, sizeof(dir), "%s/.slermes", home);
    }
    struct stat st;
    if (stat(dir, &st) != 0) mkdir(dir, 0700);
    char path[4096];
    snprintf(path, sizeof(path), "%s/config.yaml", dir);
    if (stat(path, &st) == 0) {
        printf("Config already exists at %s\n", path);
    } else {
        hermes_config_t cfg;
        hermes_config_defaults(&cfg);
        hermes_config_export(&cfg, path);
        printf("Created: %s\n", path);
    }
    snprintf(path, sizeof(path), "%s/.env", dir);
    if (stat(path, &st) == 0) {
        printf("Env file already exists at %s\n", path);
    } else {
        FILE *f = fopen(path, "w");
        if (f) {
            fprintf(f, "# Slermes API Keys\\n");
            fprintf(f, "#OPENAI_API_KEY=sk-...\\n");
            fprintf(f, "#ANTHROPIC_API_KEY=sk-ant-...\\n");
            fprintf(f, "#GOOGLE_API_KEY=AIza...\\n");
            fprintf(f, "#DEEPSEEK_API_KEY=sk-...\\n");
            fprintf(f, "#XAI_API_KEY=xai-...\n");
            fclose(f);
            printf("Created: %s (edit to add API keys)\n", path);
        }
    }
    printf("\nSlermes config initialized at %s\n", dir);
    printf("Next: edit %s/.env, then run ./slermes\n", dir);
    return true;
}

/* Port of Python hermes_cli/setup.py:_apply_default_agent_settings().
 * Apply sensible agent defaults for first-time install. */
static void setup_apply_default_agent_settings(hermes_config_t *cfg) {
    if (!cfg) return;
    cfg->max_turns = 90;
    cfg->verbose = 1;       /* "all" mode */
    cfg->compress_enabled = true;
}

/* Port of Python hermes_cli/setup.py:print_header(). */
static void setup_print_header(const char *title) {
    printf("\n");
    int len = strlen(title);
    printf("╔═");
    for (int i = 0; i < len; i++) printf("═");
    printf("═╗\n");
    printf("║ %s ║\n", title);
    printf("╚═");
    for (int i = 0; i < len; i++) printf("═");
    printf("═╝\n\n");
}

/* Prompt for a string with optional default */
/* Port of Python hermes_cli/setup.py:prompt(). */
static char *setup_prompt(const char *question, const char *default_val) {
    if (default_val && default_val[0])
        printf("%s [%s]: ", question, default_val);
    else
        printf("%s: ", question);
    fflush(stdout);

    static char buf[512];
    if (!fgets(buf, sizeof(buf), stdin)) return NULL;
    char *nl = strchr(buf, '\n');
    if (nl) *nl = '\0';
    if (buf[0] == '\0' && default_val)
        strncpy(buf, default_val, sizeof(buf) - 1);
    return buf;
}

/* Prompt for yes/no. Returns true for yes, false for no. */
/* Port of Python hermes_cli/setup.py:prompt_yes_no(). */
static bool setup_prompt_yes_no(const char *question, bool default_yes) {
    const char *msg = default_yes ? " (Y/n)" : " (y/N)";
    char buf[128];
    snprintf(buf, sizeof(buf), "%s%s", question, msg);
    return cw_confirm(buf, NULL);
}

/* Prompt for a choice from a list. Returns index or -1 on cancel/error. */
/* Port of Python hermes_cli/setup.py:prompt_choice(). */
static int setup_prompt_choice(const char *question, const char *choices[],
                                int n_choices, int default_idx) {
    char **items = (char **)choices;
    int result = cw_radiolist(question, items, n_choices,
                               default_idx, default_idx, NULL, false);
    if (result < 0) result = default_idx;
    return result;
}

/* Port of Python hermes_cli/setup.py:setup_agent_settings().
 * Configure agent behavior: max iterations, tool progress, compression. */
static void setup_agent_settings(hermes_config_t *cfg) {
    setup_print_header("Agent Settings");

    /* Max iterations */
    printf("Maximum tool-calling iterations per conversation.\n");
    printf("Higher = more complex tasks, but costs more tokens.\n");
    char *max_str = setup_prompt("Max iterations",
                                  cfg->max_turns > 0 ? (char[]){0} : "90");
    (void)max_str; /* In-memory config already has defaults */

    /* Tool progress display */
    printf("\nTool Progress Display\n");
    printf("  off     — Silent, just the final response\n");
    printf("  all     — Show every tool call with a short preview (default)\n");
    printf("  verbose — Full args, results, and debug logs\n");
    const char *mode_choices[] = {"off", "all", "verbose"};
    int mode_idx = setup_prompt_choice("Select tool progress mode:", mode_choices, 3, 1);
    cfg->verbose = mode_idx;
    printf("  Tool progress: %s\n", mode_choices[mode_idx]);

    /* Compression */
    setup_print_header("Context Compression");
    printf("Automatically summarizes old messages when context gets too long.\n");
    bool compress = setup_prompt_yes_no("Enable context compression?", true);
    cfg->compress_enabled = compress;
    printf("  Context compression: %s\n", compress ? "enabled" : "disabled");
}

/* Port of Python hermes_cli/setup.py:setup_terminal_backend().
 * Configure where shell commands run. */
static void setup_terminal_backend(hermes_config_t *cfg) {
    setup_print_header("Terminal Backend");
    printf("Choose where commands run. This affects tool execution and isolation.\n");

    const char *backends[] = {
        "Local — run directly on this machine",
        "Docker — isolated container",
    };
    int idx = setup_prompt_choice("Select terminal backend:", backends, 2, 0);
    if (idx == 0) {
        cfg->tools.persistent_shell = true;
        printf("  Terminal: Local\n");
    } else if (idx == 1) {
        cfg->tools.persistent_shell = false;
        printf("  Terminal: Docker\n");
        printf("  Note: Docker integration requires docker CLI in PATH.\n");
    }
}

/* Port of Python hermes_cli/setup.py:_print_setup_summary().
 * Print a summary of what was configured. */
static void setup_print_summary(const hermes_config_t *cfg) {
    setup_print_header("Setup Complete");
    printf("  Provider: %s\n", cfg->provider_cfg.provider);
    printf("  Model:    %s\n", cfg->provider_cfg.model);
    printf("  Max iterations: %d\n", cfg->max_turns);
    printf("  Compression:    %s\n", cfg->compress_enabled ? "on" : "off");
    printf("  Tool progress:  ");
    if (cfg->verbose == 0) printf("off\n");
    else if (cfg->verbose == 1) printf("all\n");
    else printf("verbose\n");
    printf("  TTS provider:   %s\n", cfg->tts.provider[0] ? cfg->tts.provider : "edge (default)");
    printf("\n  Config:  %s\n", cfg->config_path);
    printf("\nRun ./slermes to start chatting.\n");
}

/* Port of Python hermes_cli/setup.py:is_interactive_stdin(). */
static bool setup_is_interactive(void) {
    return isatty(fileno(stdin));
}

/* Port of Python hermes_cli/setup.py:print_noninteractive_setup_guidance(). */
static void setup_print_noninteractive_guidance(const char *reason) {
    printf("\n=== Slermes Setup — Non-interactive mode ===\n\n");
    if (reason)
        printf("  %s\n", reason);
    printf("  The interactive wizard cannot be used here.\n");
    printf("\n");
    printf("  Configure Slermes using environment variables or config commands:\n");
    printf("    Set SLERMES_HOME to your config directory\n");
    printf("    Edit config.yaml and .env directly\n");
    printf("\n");
    printf("  Or run 'slermes setup' in an interactive terminal to use the full wizard.\n");
    printf("\n");
}
/* Port of Python hermes_cli/setup.py:_sanitize_pasted_input().
 * Strips terminal bracketed-paste control markers and leading/trailing whitespace.
 * Python: re.compile(r"\x1b\[\s*200~|\x1b\[\s*201~") */
static void setup_sanitize_pasted_input(char *value) {
    if (!value || !*value) return;
    char *src = value;
    char *dst = value;
    while (*src) {
        /* Strip bracketed paste markers: \x1b[200~ and \x1b[201~ */
        if ((unsigned char)*src == 0x1b && *(src+1) == '[') {
            char *end = strchr(src, '~');
            if (end) {
                src = end + 1;
                continue;
            }
        }
        /* Strip leading whitespace */
        if (dst == value && (*src == ' ' || *src == '\t' || *src == '\n' || *src == '\r')) {
            src++;
            continue;
        }
        *dst++ = *src++;
    }
    *dst = '\0';
    /* Strip trailing whitespace */
    while (dst > value && (*(dst-1) == ' ' || *(dst-1) == '\t' || *(dst-1) == '\n' || *(dst-1) == '\r'))
        *--dst = '\0';
}

/* Port of Python hermes_cli/setup.py:prompt_checklist() — multi-select checklist.
 * Uses curses_widget cw_checklist() with numbered fallback. */
static int setup_prompt_checklist(const char *title, const char *items[], int n_items,
                                  int out_selected[], int max_selected) {
    /* Pre-select all */
    int *initial = NULL;
    /* Build writeable items array */
    char **writable = calloc((size_t)n_items, sizeof(char *));
    if (!writable) return 0;
    for (int i = 0; i < n_items; i++)
        writable[i] = (char *)items[i];

    cw_selection_t sel = cw_checklist(
        title ? title : "Select items",
        writable, n_items, initial, 0, NULL);
    free(writable);

    int count = 0;
    for (size_t i = 0; i < sel.count && count < max_selected; i++)
        out_selected[count++] = sel.indices[i];
    cw_selection_free(&sel);
    return count;
}

/* ── Masked input for API keys (port of Python masked_secret_prompt) ── */

/* Read a line from stdin without echoing (uses termios to disable echo).
 * Returns the input in out (null-terminated). Falls back to plain fgets. */
static void setup_masked_input(const char *prompt, char *out, size_t out_size) {
    if (!prompt || !out || out_size < 1) return;
    out[0] = '\0';

    if (!isatty(fileno(stdin)) || !isatty(fileno(stdout))) {
        /* Non-interactive: use getpass-style fallback */
        printf("%s", prompt);
        fflush(stdout);
        if (fgets(out, out_size, stdin)) {
            char *nl = strchr(out, '\n');
            if (nl) *nl = '\0';
        }
        setup_sanitize_pasted_input(out);
        return;
    }

    /* Try termios-based masked input */
    struct termios old, new;
    if (tcgetattr(fileno(stdin), &old) != 0) {
        /* Fallback: plain input */
        printf("%s", prompt);
        fflush(stdout);
        if (fgets(out, out_size, stdin)) {
            char *nl = strchr(out, '\n');
            if (nl) *nl = '\0';
        }
        setup_sanitize_pasted_input(out);
        return;
    }
    new = old;
    new.c_lflag &= ~(ECHO | ICANON);
    new.c_cc[VMIN] = 1;
    new.c_cc[VTIME] = 0;
    tcsetattr(fileno(stdin), TCSANOW, &new);

    printf("%s", prompt);
    fflush(stdout);

    size_t pos = 0;
    while (pos < out_size - 1) {
        char c;
        if (read(fileno(stdin), &c, 1) != 1) break;

        if (c == '\n' || c == '\r') {
            break;
        } else if (c == 127 || c == '\b') {
            if (pos > 0) {
                pos--;
                printf("\b \b");
                fflush(stdout);
            }
        } else if (c == 3) { /* Ctrl+C */
            printf("^C\n");
            tcsetattr(fileno(stdin), TCSANOW, &old);
            exit(1);
        } else if (c == 26) { /* Ctrl+Z */
            tcsetattr(fileno(stdin), TCSANOW, &old);
            raise(SIGTSTP);
            tcsetattr(fileno(stdin), TCSANOW, &new);
            printf("%s", prompt);
            fflush(stdout);
        } else if ((unsigned char)c >= 32 && (unsigned char)c < 127) {
            out[pos++] = c;
            printf("*");
            fflush(stdout);
        }
        /* Ignore other control characters */
    }
    out[pos] = '\0';
    tcsetattr(fileno(stdin), TCSANOW, &old);
    printf("\n");
    setup_sanitize_pasted_input(out);
}

/* ── Formatted API key prompt (port of Python _prompt_api_key) ── */

/* Provider info struct for formatted prompts */
typedef struct {
    const char *name;         /* provider name (env var name) */
    const char *description;  /* display description */
    const char *url;          /* where to get the key */
    const char *prompt_text;  /* prompt text */
    const char *key_env;      /* env var to save to */
} setup_provider_key_info_t;

/* Show a formatted API key prompt with description and URL.
 * Reads the key into out, writing '*' for each character.
 * Port of Python hermes_cli/setup.py:_prompt_api_key(). */
static void setup_prompt_api_key(const setup_provider_key_info_t *info,
                                  char *out, size_t out_size) {
    if (!info || !out || out_size < 1) return;
    out[0] = '\0';

    printf("\n");
    printf("  ─── %s ───\n", info->description);
    printf("\n");
    if (info->url && info->url[0])
        printf("  Get your key at: %s\n", info->url);

    /* Check if key exists in env */
    const char *existing = info->key_env ? getenv(info->key_env) : NULL;
    if (existing && existing[0]) {
        printf("  [already set in %s — Enter to keep, type to replace]\n", info->key_env);
    }

    printf("\n");
    printf("  %s: ", info->prompt_text ? info->prompt_text : info->name);
    fflush(stdout);

    setup_masked_input("", out, out_size);

    if (out[0] == '\0' && existing && existing[0]) {
        /* Keep existing */
        return;
    }
}

/* ── Provider key info lookup ── */

/* Key info for each SETUP_PROVIDERS entry (index must match) */
static const setup_provider_key_info_t SETUP_KEY_INFO[] = {
    {"NOUS_API_KEY",       "Nous Portal",           "https://nousresearch.com/portal",   "NOUS_API_KEY",      "NOUS_API_KEY"},
    {"OPENAI_API_KEY",     "OpenAI API Key",        "https://platform.openai.com/api-keys", "sk-...",        "OPENAI_API_KEY"},
    {"ANTHROPIC_API_KEY",  "Anthropic API Key",     "https://console.anthropic.com/",      "sk-ant-...",      "ANTHROPIC_API_KEY"},
    {"GOOGLE_API_KEY",     "Google AI API Key",     "https://aistudio.google.com/apikey",  "AIza...",         "GOOGLE_API_KEY"},
    {"DEEPSEEK_API_KEY",   "DeepSeek API Key",      "https://platform.deepseek.com/",      "sk-...",          "DEEPSEEK_API_KEY"},
    {"XAI_API_KEY",        "xAI API Key",           "https://console.x.ai/",               "xai-...",         "XAI_API_KEY"},
    {"OPENROUTER_API_KEY", "OpenRouter API Key",    "https://openrouter.ai/keys",          "sk-or-...",       "OPENROUTER_API_KEY"},
    {"AZURE_API_KEY",      "Azure OpenAI Key",      "https://portal.azure.com/",           "Azure API key",   "AZURE_API_KEY"},
    {"AWS_ACCESS_KEY_ID",  "AWS Credentials",       "https://aws.amazon.com/console/",     "AWS key ID",      "AWS_ACCESS_KEY_ID"},
    {"CODEX_API_KEY",      "GitHub Copilot Codex",  "https://github.com/settings/tokens",  "ghp_...",         "CODEX_API_KEY"},
    {"",                   "Custom Endpoint",       "",                                    "",                ""},
};

/* ── Base URL override prompt (port of Python's optional base URL override) ── */

/* Prompt for an optional base URL override. Shows current effective URL as default.
 * Port of Python hermes_cli/main.py:_model_flow_api_key_provider() base URL section. */
static void setup_prompt_base_url(const char *provider, char *out, size_t out_size) {
    out[0] = '\0';
    if (!provider || strcmp(provider, "custom") == 0) return;

    /* Get the provider's default base URL from metadata */
    const provider_metadata_t *meta = provider_metadata_find(provider);
    const char *effective = meta && meta->base_url ? meta->base_url : "";
    if (!effective[0]) return;

    printf("\n");
    printf("  Base URL [%s]: ", effective);
    fflush(stdout);

    char buf[1024];
    if (!fgets(buf, sizeof(buf), stdin)) return;
    char *nl = strchr(buf, '\n'); if (nl) *nl = '\0';
    setup_sanitize_pasted_input(buf);

    if (buf[0]) {
        if (strncmp(buf, "http://", 7) != 0 && strncmp(buf, "https://", 8) != 0) {
            printf("  Invalid URL — must start with http:// or https://. Using default.\n");
        } else {
            snprintf(out, out_size, "%s", buf);
        }
    }
}

/* Port of Python hermes_cli/setup.py:prompt() — with sanitization.
 * NOTE: setup_prompt() already used in setup flow. This is kept for
 * parity with Python's input(). Only use when you need inline
 * sanitization. */
__attribute__((unused)) static char *setup_prompt_sanitized(const char *question, const char *default_val) {
    char *result = setup_prompt(question, default_val);
    if (result) setup_sanitize_pasted_input(result);
    return result;
}

/* ── Config utility functions (Port of Python setup.py) ── */

/* Port of Python hermes_cli/setup.py:_model_config_dict().
 * Return the model config: if it's a dict return it, if string wrap in {"default": s}. */
__attribute__((unused)) static void setup_model_config_dict(const hermes_config_t *cfg,
                                     char *out_default, size_t out_size) {
    if (!cfg) return;
    if (cfg->model[0])
        strncpy(out_default, cfg->model, out_size - 1);
    else
        out_default[0] = '\0';
}

/* Port of Python hermes_cli/setup.py:_current_reasoning_effort().
 * Read reasoning_effort from config struct. Currently not stored in cfg,
 * always returns empty string (caller uses default). */
__attribute__((unused)) static const char *setup_current_reasoning_effort(void) {
    return ""; /* hermes_config_t does not have reasoning_effort field yet */
}

/* Port of Python hermes_cli/setup.py:_supports_same_provider_pool_setup().
 * Returns true for providers that support multi-key rotation. */
__attribute__((unused)) static bool setup_supports_same_provider_pool_setup(const char *provider) {
    if (!provider || !*provider || strcmp(provider, "custom") == 0)
        return false;
    /* Most cloud providers support pool setup. Classic OAuth porters like
     * Nous and Gemini CLI do not. */
    if (strcmp(provider, "nous") == 0) return false;
    if (strcmp(provider, "copilot") == 0) return false;
    return true;
}

/* Port of Python hermes_cli/setup.py:_gateway_platform_short_label().
 * Strip trailing parenthetical qualifiers from a label. */
__attribute__((unused)) static const char *setup_gateway_platform_short_label(const char *label) {
    if (!label) return "";
    const char *paren = strchr(label, '(');
    if (!paren) return label;
    /* Return the part before the paren */
    static char buf[256];
    size_t len = (size_t)(paren - label);
    /* Trim trailing space */
    while (len > 0 && label[len-1] == ' ') len--;
    if (len >= sizeof(buf)) len = sizeof(buf) - 1;
    memcpy(buf, label, len);
    buf[len] = '\0';
    return buf;
}

/* Port of Python hermes_cli/setup.py:_model_section_has_credentials().
 * Return True when any known inference provider has usable credentials. */
__attribute__((unused)) static bool setup_model_section_has_credentials(const hermes_config_t *cfg, const char *provider) {
    if (!cfg || !provider) return false;
    /* Check if provider env var is set */
    const char *key_env = NULL;
    if (strcmp(provider, "nous") == 0) key_env = "NOUS_API_KEY";
    else if (strcmp(provider, "openai") == 0) key_env = "OPENAI_API_KEY";
    else if (strcmp(provider, "anthropic") == 0) key_env = "ANTHROPIC_API_KEY";
    else if (strcmp(provider, "google") == 0) key_env = "GOOGLE_API_KEY";
    else if (strcmp(provider, "deepseek") == 0) key_env = "DEEPSEEK_API_KEY";
    else if (strcmp(provider, "xai") == 0) key_env = "XAI_API_KEY";
    else if (strcmp(provider, "openrouter") == 0) key_env = "OPENROUTER_API_KEY";
    else if (strcmp(provider, "azure") == 0) key_env = "AZURE_API_KEY";
    else if (strcmp(provider, "bedrock") == 0) key_env = "AWS_ACCESS_KEY_ID";
    if (key_env) {
        const char *val = getenv(key_env);
        if (val && *val) return true;
    }
    return false;
}

/* Port of Python hermes_cli/setup.py:_check_espeak_ng().
 * Check if espeak-ng or espeak is installed. */
__attribute__((unused)) static bool setup_check_espeak_ng(void) {
    FILE *fp = popen("which espeak-ng 2>/dev/null || which espeak 2>/dev/null", "r");
    if (!fp) return false;
    char buf[256];
    bool found = (fgets(buf, sizeof(buf), fp) != NULL);
    pclose(fp);
    return found;
}

/* Port of Python hermes_cli/setup.py:_xai_oauth_logged_in_for_setup().
 * Check if xAI OAuth token exists — probes XAI_API_KEY env. */
__attribute__((unused)) static bool setup_xai_oauth_logged_in(void) {
    const char *key = getenv("XAI_API_KEY");
    return key && *key;
}

/* ═══════════════════════════════════════════════
 * Gateway Platform Setup Functions
 * Each port of a Python _setup_{platform}() from hermes_cli/setup.py.
 * All follow the same pattern: check existing, prompt for creds, save to .env.
 * ═══════════════════════════════════════════════ */

/* Helper: save env value to .env file.
 * Opens ~/.slermes/.env for append, avoids duplicate keys. */
static bool setup_save_env(const char *key, const char *value) {
    if (!key || !value || !*value) return false;
    const char *home = getenv("SLERMES_HOME");
    if (!home) home = getenv("HERMES_HOME");
    if (!home) home = getenv("HOME");
    if (!home) return false;

    char path[4096];
    if (getenv("SLERMES_HOME") || getenv("HERMES_HOME"))
        snprintf(path, sizeof(path), "%s/.env", home);
    else
        snprintf(path, sizeof(path), "%s/.slermes/.env", home);

    /* Check if key already exists — read all lines, remove old entry */
    FILE *f = fopen(path, "r");
    char tmp[4096];
    char buf[8192] = {0};
    size_t pos = 0;
    if (f) {
        while (fgets(tmp, sizeof(tmp), f)) {
            /* Skip lines matching KEY= (existing definition to replace) */
            size_t klen = strlen(key);
            if (strncmp(tmp, key, klen) == 0 && tmp[klen] == '=') {
                continue;
            }
            size_t tlen = strlen(tmp);
            if (pos + tlen < sizeof(buf)) {
                memcpy(buf + pos, tmp, tlen);
                pos += tlen;
            }
        }
        fclose(f);
    }

    /* Append new value */
    size_t vlen = strlen(key) + 1 + strlen(value) + 1; /* key=value\n */
    if (pos + vlen < sizeof(buf)) {
        snprintf(buf + pos, sizeof(buf) - pos, "%s=%s\n", key, value);
    }

    f = fopen(path, "w");
    if (!f) return false;
    fwrite(buf, 1, strlen(buf), f);
    fclose(f);
    return true;
}

/* AG26: Port of Python hermes_cli/providers.py:setup_tts(). */
static void setup_tts(hermes_config_t *cfg) {
    const char *current = cfg->tts.provider[0] ? cfg->tts.provider : "edge";
    const char *labels[] = {
        "Edge TTS (free, cloud-based, no setup needed)",
        "ElevenLabs (premium quality, needs API key)",
        "OpenAI TTS (good quality, needs API key)",
        "xAI TTS (Grok voices — OAuth login or API key)",
        "MiniMax TTS (high quality with voice cloning, needs API key)",
        "Mistral Voxtral TTS (multilingual, native Opus, needs API key)",
        "Google Gemini TTS (30 prebuilt voices, needs API key)",
        "NeuTTS (local on-device, free, ~300MB model download)",
        "KittenTTS (local on-device, free, lightweight ~25-80MB ONNX)",
    };
    const char *providers[] = {
        "edge", "elevenlabs", "openai", "xai",
        "minimax", "mistral", "gemini", "neutts", "kittentts",
    };
    int n_providers = 9;

    printf("\n");
    setup_print_header("Text-to-Speech Provider (optional)");
    printf("  Current: %s\n\n", current);

    int keep_idx = n_providers; /* Keep current is last choice */
    const char *choices[11];
    for (int i = 0; i < n_providers; i++)
        choices[i] = labels[i];
    choices[n_providers] = "Keep current";
    choices[n_providers + 1] = NULL;

    int idx = setup_prompt_choice("Select TTS provider:", choices, n_providers + 1, keep_idx);
    if (idx == keep_idx) {
        printf("  TTS provider unchanged: %s\n", current);
        return;
    }

    const char *selected = providers[idx];
    char apikey[2048] = {0};

    if (strcmp(selected, "elevenlabs") == 0) {
        const char *existing = getenv("ELEVENLABS_API_KEY");
        if (!existing || !existing[0]) {
            printf("\n  ElevenLabs API key: ");
            fflush(stdout);
            if (fgets(apikey, sizeof(apikey), stdin)) {
                char *nl = strchr(apikey, '\n'); if (nl) *nl = '\0';
                setup_sanitize_pasted_input(apikey);
            }
            if (apikey[0])
                setup_save_env("ELEVENLABS_API_KEY", apikey);
            else
                selected = "edge";
        }
    } else if (strcmp(selected, "openai") == 0) {
        const char *existing = getenv("OPENAI_API_KEY");
        if (!existing || !existing[0]) {
            printf("\n  OpenAI API key for TTS: ");
            fflush(stdout);
            if (fgets(apikey, sizeof(apikey), stdin)) {
                char *nl = strchr(apikey, '\n'); if (nl) *nl = '\0';
                setup_sanitize_pasted_input(apikey);
            }
            if (apikey[0])
                setup_save_env("VOICE_TOOLS_OPENAI_KEY", apikey);
            else
                selected = "edge";
        }
    } else if (strcmp(selected, "xai") == 0) {
        const char *existing = getenv("XAI_API_KEY");
        if (!existing || !existing[0]) {
            printf("\n  xAI API key for TTS (or blank to skip): ");
            fflush(stdout);
            if (fgets(apikey, sizeof(apikey), stdin)) {
                char *nl = strchr(apikey, '\n'); if (nl) *nl = '\0';
                setup_sanitize_pasted_input(apikey);
            }
            if (apikey[0])
                setup_save_env("XAI_API_KEY", apikey);
            else
                selected = "edge";
        }
        if (strcmp(selected, "xai") == 0) {
            printf("  xAI voice_id [eve]: ");
            fflush(stdout);
            char voice[64] = {0};
            if (fgets(voice, sizeof(voice), stdin)) {
                char *nl = strchr(voice, '\n'); if (nl) *nl = '\0';
                setup_sanitize_pasted_input(voice);
                if (voice[0])
                    snprintf(cfg->tts.xai_voice_id, sizeof(cfg->tts.xai_voice_id), "%s", voice);
            }
        }
    } else if (strcmp(selected, "minimax") == 0) {
        const char *existing = getenv("MINIMAX_API_KEY");
        if (!existing || !existing[0]) {
            printf("\n  MiniMax API key for TTS: ");
            fflush(stdout);
            if (fgets(apikey, sizeof(apikey), stdin)) {
                char *nl = strchr(apikey, '\n'); if (nl) *nl = '\0';
                setup_sanitize_pasted_input(apikey);
            }
            if (apikey[0])
                setup_save_env("MINIMAX_API_KEY", apikey);
            else
                selected = "edge";
        }
    } else if (strcmp(selected, "mistral") == 0) {
        const char *existing = getenv("MISTRAL_API_KEY");
        if (!existing || !existing[0]) {
            printf("\n  Mistral API key for TTS: ");
            fflush(stdout);
            if (fgets(apikey, sizeof(apikey), stdin)) {
                char *nl = strchr(apikey, '\n'); if (nl) *nl = '\0';
                setup_sanitize_pasted_input(apikey);
            }
            if (apikey[0])
                setup_save_env("MISTRAL_API_KEY", apikey);
            else
                selected = "edge";
        }
    } else if (strcmp(selected, "gemini") == 0) {
        const char *existing = getenv("GOOGLE_API_KEY");
        if (!existing || !existing[0]) {
            printf("\n  Gemini API key for TTS (get at https://aistudio.google.com/app/apikey): ");
            fflush(stdout);
            if (fgets(apikey, sizeof(apikey), stdin)) {
                char *nl = strchr(apikey, '\n'); if (nl) *nl = '\0';
                setup_sanitize_pasted_input(apikey);
            }
            if (apikey[0])
                setup_save_env("GOOGLE_API_KEY", apikey);
            else
                selected = "edge";
        }
    }

    /* Save TTS provider selection in config */
    snprintf(cfg->tts.provider, sizeof(cfg->tts.provider), "%s", selected);
    printf("  ✅ TTS provider set to: %s\n", selected);
}

/* Port of Python hermes_cli/setup.py:_setup_telegram().
 * Configure Telegram bot credentials and allowlist. */
static void setup_telegram(void) {
    setup_print_header("Telegram");
    const char *existing = getenv("TELEGRAM_BOT_TOKEN");
    if (existing && *existing) {
        printf("  Telegram: already configured\n");
        if (!setup_prompt_yes_no("Reconfigure Telegram?", false))
            return;
    }

    printf("  Create a bot via @BotFather on Telegram\n\n");

    char token[512];
    for (;;) {
        printf("  Telegram bot token: ");
        fflush(stdout);
        if (!fgets(token, sizeof(token), stdin)) return;
        char *nl = strchr(token, '\n'); if (nl) *nl = '\0';
        setup_sanitize_pasted_input(token);
        if (!token[0]) return;
        /* Basic format check: numeric_id:hash */
        if (strchr(token, ':') != NULL) break;
        printf("  Invalid format. Expected: <numeric_id>:<hash>\n");
    }
    setup_save_env("TELEGRAM_BOT_TOKEN", token);
    printf("  ✅ Telegram token saved\n\n");

    printf("  🔒 Security: Restrict who can use your bot\n");
    printf("     To find your Telegram user ID, message @userinfobot\n\n");
    printf("  Allowed user IDs (comma-separated, leave empty for open access): ");
    fflush(stdout);
    char allowed[512] = {0};
    if (fgets(allowed, sizeof(allowed), stdin)) {
        char *nl = strchr(allowed, '\n'); if (nl) *nl = '\0';
        setup_sanitize_pasted_input(allowed);
    }
    if (allowed[0]) {
        setup_save_env("TELEGRAM_ALLOWED_USERS", allowed);
        printf("  ✅ Telegram allowlist configured\n");
    } else {
        printf("  ⚠️  No allowlist set - anyone who finds your bot can use it!\n");
    }

    printf("\n  📬 Home Channel\n");
    if (allowed[0]) {
        char *first = allowed;
        char *comma = strchr(first, ',');
        if (comma) *comma = '\0';
        if (setup_prompt_yes_no("Use your user ID as the home channel?", true)) {
            setup_save_env("TELEGRAM_HOME_CHANNEL", first);
            printf("  ✅ Telegram home channel set\n");
        }
    }
}

/* Port of Python hermes_cli/setup.py:_setup_slack().
 * Configure Slack app credentials and guide user through manifest setup. */
static void setup_slack(void) {
    setup_print_header("Slack");
    const char *existing = getenv("SLACK_BOT_TOKEN");
    if (existing && *existing) {
        printf("  Slack: already configured\n");
        if (!setup_prompt_yes_no("Reconfigure Slack?", false))
            return;
    }

    printf("  Steps to create a Slack app:\n");
    printf("    1. Go to https://api.slack.com/apps -> Create New App\n");
    printf("    2. Enable Socket Mode: Settings -> Socket Mode -> Enable\n");
    printf("    3. Install to Workspace: Settings -> Install App\n");
    printf("    4. Invite the bot to channels: /invite @YourBot\n\n");

    printf("  Slack Bot Token (xoxb-...): ");
    fflush(stdout);
    char token[512] = {0};
    if (fgets(token, sizeof(token), stdin)) {
        char *nl = strchr(token, '\n'); if (nl) *nl = '\0';
        setup_sanitize_pasted_input(token);
    }
    if (!token[0]) return;
    setup_save_env("SLACK_BOT_TOKEN", token);

    printf("  Slack App Token (xapp-...): ");
    fflush(stdout);
    char app_token[512] = {0};
    if (fgets(app_token, sizeof(app_token), stdin)) {
        char *nl = strchr(app_token, '\n'); if (nl) *nl = '\0';
        setup_sanitize_pasted_input(app_token);
    }
    if (app_token[0])
        setup_save_env("SLACK_APP_TOKEN", app_token);
    printf("  ✅ Slack tokens saved\n\n");

    printf("  🔒 Security: Allowed user IDs (comma-separated, leave empty): ");
    fflush(stdout);
    char allowed[512] = {0};
    if (fgets(allowed, sizeof(allowed), stdin)) {
        char *nl = strchr(allowed, '\n'); if (nl) *nl = '\0';
        setup_sanitize_pasted_input(allowed);
    }
    if (allowed[0])
        setup_save_env("SLACK_ALLOWED_USERS", allowed);
}

/* Port of Python hermes_cli/setup.py:_setup_matrix().
 * Configure Matrix credentials. */
static void setup_matrix(void) {
    setup_print_header("Matrix");
    const char *existing = getenv("MATRIX_ACCESS_TOKEN");
    if (existing && *existing) {
        printf("  Matrix: already configured\n");
        if (!setup_prompt_yes_no("Reconfigure Matrix?", false))
            return;
    }

    printf("  Works with any Matrix homeserver (Synapse, Conduit, matrix.org).\n\n");

    printf("  Homeserver URL (e.g. https://matrix.example.org): ");
    fflush(stdout);
    char hs[512] = {0};
    if (fgets(hs, sizeof(hs), stdin)) {
        char *nl = strchr(hs, '\n'); if (nl) *nl = '\0';
        setup_sanitize_pasted_input(hs);
    }
    if (hs[0]) setup_save_env("MATRIX_HOMESERVER", hs);

    printf("  Access token (leave empty for password login): ");
    fflush(stdout);
    char token[512] = {0};
    if (fgets(token, sizeof(token), stdin)) {
        char *nl = strchr(token, '\n'); if (nl) *nl = '\0';
        setup_sanitize_pasted_input(token);
    }
    if (token[0]) {
        setup_save_env("MATRIX_ACCESS_TOKEN", token);
        printf("  User ID (@bot:server — optional): ");
        fflush(stdout);
        char uid[256] = {0};
        if (fgets(uid, sizeof(uid), stdin)) {
            char *nl = strchr(uid, '\n'); if (nl) *nl = '\0';
            if (uid[0]) setup_save_env("MATRIX_USER_ID", uid);
        }
    } else {
        printf("  User ID (@bot:server): ");
        fflush(stdout);
        char uid[256] = {0};
        if (fgets(uid, sizeof(uid), stdin)) {
            char *nl = strchr(uid, '\n'); if (nl) *nl = '\0';
            if (uid[0]) setup_save_env("MATRIX_USER_ID", uid);
        }
        printf("  Password: ");
        fflush(stdout);
        char pw[512] = {0};
        if (fgets(pw, sizeof(pw), stdin)) {
            char *nl = strchr(pw, '\n'); if (nl) *nl = '\0';
            if (pw[0]) setup_save_env("MATRIX_PASSWORD", pw);
        }
    }

    printf("\n  🔒 Security: Allowed user IDs (comma-separated, leave empty): ");
    fflush(stdout);
    char allowed[512] = {0};
    if (fgets(allowed, sizeof(allowed), stdin)) {
        char *nl = strchr(allowed, '\n'); if (nl) *nl = '\0';
        setup_sanitize_pasted_input(allowed);
    }
    if (allowed[0])
        setup_save_env("MATRIX_ALLOWED_USERS", allowed);
}

/* Port of Python hermes_cli/setup.py:_setup_webhooks().
 * Configure webhook URL for generic HTTP integration. */
static void setup_webhooks(void) {
    setup_print_header("Webhooks");
    const char *existing = getenv("WEBHOOK_SECRET");
    if (existing && *existing) {
        printf("  Webhooks: already configured\n");
        if (!setup_prompt_yes_no("Reconfigure Webhooks?", false))
            return;
    }

    printf("  Webhooks let external services send messages to Hermes.\n\n");

    printf("  Webhook URL path (e.g. /my-webhook): ");
    fflush(stdout);
    char path[256] = {0};
    if (fgets(path, sizeof(path), stdin)) {
        char *nl = strchr(path, '\n'); if (nl) *nl = '\0';
        setup_sanitize_pasted_input(path);
    }
    if (!path[0]) strncpy(path, "/webhook", sizeof(path) - 1);
    setup_save_env("WEBHOOK_PATH", path);

    printf("  Secret token (for HMAC verification, leave empty to skip): ");
    fflush(stdout);
    char secret[256] = {0};
    if (fgets(secret, sizeof(secret), stdin)) {
        char *nl = strchr(secret, '\n'); if (nl) *nl = '\0';
        setup_sanitize_pasted_input(secret);
    }
    if (secret[0])
        setup_save_env("WEBHOOK_SECRET", secret);
    printf("  ✅ Webhook configured\n");
    setup_save_env("WEBHOOK_ENABLED", "true");
    printf("  Webhooks enabled! Define routes in config.yaml.\n");
}

/* Port of Python hermes_cli/setup.py:_setup_bluebubbles().
 * Configure BlueBubbles iMessage gateway. */
static void setup_bluebubbles(void) {
    setup_print_header("BlueBubbles (iMessage)");
    const char *existing_url = getenv("BLUEBUBBLES_SERVER_URL");
    if (existing_url && *existing_url) {
        printf("  BlueBubbles: already configured\n");
        if (!setup_prompt_yes_no("Reconfigure BlueBubbles?", false))
            return;
    }
    printf("  Connects Hermes to iMessage via BlueBubbles.\n");
    printf("  Requires a Mac running BlueBubbles Server.\n");
    printf("  Download: https://bluebubbles.app/\n\n");
    printf("  BlueBubbles server URL (e.g. http://192.168.1.10:1234): ");
    fflush(stdout);
    char url[512] = {0};
    if (!fgets(url, sizeof(url), stdin)) return;
    char *nl = strchr(url, '\n'); if (nl) *nl = '\0';
    setup_sanitize_pasted_input(url);
    if (!url[0]) { printf("  Skipping — URL required\n"); return; }
    setup_save_env("BLUEBUBBLES_SERVER_URL", url);
    printf("  BlueBubbles server password: ");
    fflush(stdout);
    char pwd[512] = {0};
    if (!fgets(pwd, sizeof(pwd), stdin)) return;
    nl = strchr(pwd, '\n'); if (nl) *nl = '\0';
    setup_sanitize_pasted_input(pwd);
    if (!pwd[0]) { printf("  Skipping — password required\n"); return; }
    setup_save_env("BLUEBUBBLES_PASSWORD", pwd);
    printf("  ✅ BlueBubbles credentials saved\n\n");
    printf("  🔒 Allowed iMessage addresses (comma-separated, empty=open): ");
    fflush(stdout);
    char allowed[512] = {0};
    if (fgets(allowed, sizeof(allowed), stdin)) {
        nl = strchr(allowed, '\n'); if (nl) *nl = '\0';
        setup_sanitize_pasted_input(allowed);
    }
    if (allowed[0]) {
        char *src = allowed, *dst = allowed;
        while (*src) { if (*src != ' ') *dst++ = *src; src++; }
        *dst = '\0';
        setup_save_env("BLUEBUBBLES_ALLOWED_USERS", allowed);
        printf("  ✅ BlueBubbles allowlist configured\n");
    }
}

/* Port of Python hermes_cli/setup.py:_setup_qqbot().
 * Configure QQ Bot credentials. */
static void setup_qqbot(void) {
    setup_print_header("QQ Bot");
    const char *existing_id = getenv("QQ_APP_ID");
    const char *existing_secret = getenv("QQ_CLIENT_SECRET");
    if (existing_id && *existing_id && existing_secret && *existing_secret) {
        printf("  QQ Bot: already configured\n");
        if (!setup_prompt_yes_no("Reconfigure QQ Bot?", false))
            return;
    }
    printf("  Register at https://q.qq.com\n\n");
    printf("  App ID: "); fflush(stdout);
    char app_id[256] = {0};
    if (!fgets(app_id, sizeof(app_id), stdin)) return;
    char *nl = strchr(app_id, '\n'); if (nl) *nl = '\0';
    setup_sanitize_pasted_input(app_id);
    if (!app_id[0]) { printf("  Skipping — App ID required\n"); return; }
    printf("  App Secret: "); fflush(stdout);
    char app_secret[512] = {0};
    if (!fgets(app_secret, sizeof(app_secret), stdin)) return;
    nl = strchr(app_secret, '\n'); if (nl) *nl = '\0';
    setup_sanitize_pasted_input(app_secret);
    if (!app_secret[0]) { printf("  Skipping — App Secret required\n"); return; }
    setup_save_env("QQ_APP_ID", app_id);
    setup_save_env("QQ_CLIENT_SECRET", app_secret);
    printf("  ✅ QQ Bot credentials saved\n");
}

/* Port of Python hermes_cli/setup.py:setup_gateway().
 * Configure messaging platform integrations — multi-select from available platforms.
 * Sets cfg->gateway_platforms from selected platforms. */
static void setup_gateway(hermes_config_t *cfg) {
    setup_print_header("Messaging Platforms");
    printf("  Connect Hermes to messaging platforms.\n\n");

    const char *platforms[] = {
        "Telegram",
        "Slack",
        "Matrix",
        "Webhooks (HTTP)",
        "BlueBubbles (iMessage)",
        "QQ Bot",
    };
    const char *platform_keys[] = {
        "telegram", "slack", "matrix", "webhook", "bluebubbles", "qqbot",
    };
    int n_platforms = 6;
    int selected[16];
    int n_selected = setup_prompt_checklist(
        "Select platforms to configure:",
        platforms, n_platforms, selected, 16);

    /* Build comma-separated gateway_platforms string */
    char gw_platforms[256] = {0};
    for (int i = 0; i < n_selected; i++) {
        if (i > 0) strncat(gw_platforms, ",", sizeof(gw_platforms) - strlen(gw_platforms) - 1);
        strncat(gw_platforms, platform_keys[selected[i]],
                sizeof(gw_platforms) - strlen(gw_platforms) - 1);
    }

    for (int i = 0; i < n_selected; i++) {
        switch (selected[i]) {
            case 0: setup_telegram(); break;
            case 1: setup_slack(); break;
            case 2: setup_matrix(); break;
            case 3: setup_webhooks(); break;
            case 4: setup_bluebubbles(); break;
            case 5: setup_qqbot(); break;
        }
        printf("\n");
    }

    if (n_selected > 0) {
        printf("  ✅ Messaging platforms configured!\n");
        /* Set gateway_platforms in config and file */
        if (cfg) {
            hermes_config_set_platforms(cfg, gw_platforms);
        }
    } else {
        printf("  No platforms selected.\n");
    }
}

/* ── Provider endpoint configuration helper ────────────── */

/* Set base_url and api_mode from provider_metadata for known providers.
 * For "custom" providers the caller must set base_url separately. */
static void setup_set_provider_endpoint(hermes_config_t *cfg, const char *provider) {
    if (!provider || !*provider) return;

    /* Custom providers keep whatever base_url is already set */
    if (strcmp(provider, "custom") == 0) return;

    /* Look up provider metadata for base_url and api_mode */
    const provider_metadata_t *meta = provider_metadata_find(provider);
    if (meta && meta->base_url && *meta->base_url) {
        snprintf(cfg->provider_cfg.base_url, sizeof(cfg->provider_cfg.base_url), "%s", meta->base_url);
        snprintf(cfg->base_url, sizeof(cfg->base_url), "%s", meta->base_url);
    }

    /* Set api_mode: Anthropic uses 'messages', all others use 'chat_completions' */
    if (strcmp(provider, "anthropic") == 0) {
        snprintf(cfg->provider_cfg.api_mode, sizeof(cfg->provider_cfg.api_mode), "messages");
    } else if (strcmp(provider, "google") == 0) {
        snprintf(cfg->provider_cfg.api_mode, sizeof(cfg->provider_cfg.api_mode), "gemini");
    } else if (strcmp(provider, "bedrock") == 0) {
        snprintf(cfg->provider_cfg.api_mode, sizeof(cfg->provider_cfg.api_mode), "bedrock");
    } else {
        snprintf(cfg->provider_cfg.api_mode, sizeof(cfg->provider_cfg.api_mode), "chat_completions");
    }
}

static const char *SETUP_PROVIDERS[] = {
    "nous", "openai", "anthropic", "google", "deepseek", "xai",
    "openrouter", "azure", "bedrock", "codex", "custom", NULL
};
static const char *SETUP_LABELS[] = {
    "Nous Portal (free OAuth login, managed inference, no API keys)",
    "OpenAI (GPT-4o, GPT-4.1, o3, o4-mini, etc.)",
    "Anthropic (Claude Sonnet 4, Opus 4, Haiku 3.5, etc.)",
    "Google Gemini (Gemini 2.5 Pro, Gemini 3 Pro/Flash previews, etc.)",
    "DeepSeek (DeepSeek-V3, DeepSeek-R1, DeepSeek-Chat, etc.)",
    "xAI Grok (Grok 3, Grok 3 Mini, SuperGrok OAuth)",
    "OpenRouter (100+ models, single API key, route to any model)",
    "Azure OpenAI (Microsoft-hosted GPT-4o, o-series, etc.)",
    "AWS Bedrock (Claude, Llama, Mistral via AWS)",
    "GitHub Copilot Codex (Codespaces-native, GPT/Claude/Gemini via Copilot)",
    "Custom endpoint (any OpenAI-compatible API — Ollama, vLLM, etc.)",
};
static const char *SETUP_MODELS[] = {
    "deepseek/deepseek-v4-flash", "gpt-4o", "claude-sonnet-4", "gemini-2.5-pro", "deepseek-chat",
    "grok-3", "gpt-4o", "claude-sonnet-4", "claude-sonnet-4",
    "claude-sonnet-4", "custom", NULL
};

/* Port of Python hermes_cli/setup.py:run_setup_wizard() — core path.
 * Interactive setup wizard with section flow matching Python:
 * 1. Model & Provider  2. Terminal Backend  3. Agent Settings
 * 4. Gateway (basic)   5. Summary + Save.
 * Creates config.yaml + .env similar to Python's `hermes setup`. */

/* Nous model picker — fetch models from Nous Portal inference API or fall back
 * to curated list. Shows all available models for user selection.
 * Uses dynamic allocation — no cap on number of models.
 * Called after successful Nous OAuth login.
 * Port of Python
 * Port of Python hermes_cli/timeouts.py (timeout config fields).
 * Port of Python hermes_cli/fallback_config.py (fallback config defaults).
 * Port of Python hermes_cli/env_loader.py (dotenv / .env file loading). hermes_cli/models.py:fetch_models_with_pricing(). */
static void setup_nous_model_picker(char *out_model, size_t out_size) {
    const char *fallback_models[] = {
        "deepseek/deepseek-v4-flash",
        "deepseek/deepseek-v4",
        "claude-sonnet-4-20250514",
        "gpt-4.1",
        "gemini-2.5-pro",
        "grok-3",
        "Custom model (type name manually)",
    };
    int n_fallback = sizeof(fallback_models) / sizeof(fallback_models[0]);

    printf("\n");
    setup_print_header("Fetching Models");
    printf("  Querying Nous Portal for available models...\n");
    fflush(stdout);

    /* Dynamically allocated model list — no cap */
    char **scraped = NULL;
    int n_scraped = 0;

    /* Try to fetch models from Nous inference API */
    http_t *h = http_new(10);
    http_resp_t *resp = NULL;
    if (h) {
        resp = http_get(h, "https://inference-api.nousresearch.com/v1/models", NULL);
    }

    if (resp && resp->status == 200 && resp->body) {
        json_node_t *data = json_parse(resp->body, NULL);
        if (data && data->type == JSON_OBJECT) {
            json_node_t *models_arr = json_object_get(data, "data");
            if (models_arr && models_arr->type == JSON_ARRAY) {
                size_t arr_len = json_array_count(models_arr);
                /* Use calloc — grows with actual count, no cap */
                scraped = calloc(arr_len + 1, sizeof(char *));
                if (scraped) {
                    for (size_t i = 0; i < arr_len; i++) {
                        json_node_t *item = json_array_get(models_arr, (int)i);
                        if (item && item->type == JSON_OBJECT) {
                            const char *mid = json_object_get_string(item, "id", NULL);
                            if (mid)
                                scraped[n_scraped++] = strdup(mid);
                        }
                    }
                    printf("  Found %d models.\n", n_scraped);
                }
            }
        }
        json_free(data);
    }

    if (resp) http_resp_free(resp);
    if (h) http_free(h);

    /* Build final list: scraped models + custom entry */
    char **models;
    int n_models;
    int custom_idx;

    if (n_scraped > 0) {
        /* Realloc to add custom entry */
        char **with_custom = realloc(scraped, (size_t)(n_scraped + 2) * sizeof(char *));
        if (!with_custom) {
            for (int i = 0; i < n_scraped; i++) free(scraped[i]);
            free(scraped);
            goto fallback_list;
        }
        scraped = with_custom;
        custom_idx = n_scraped;
        scraped[n_scraped] = NULL; /* placeholder — we use custom_idx logic */
        scraped[n_scraped + 1] = NULL;
        models = scraped;
        n_models = n_scraped + 1; /* +1 for custom option */
    } else {
fallback_list:
        printf("  API unavailable — using curated model list.\n");
        /* Build dynamic copy of fallback + custom */
        models = calloc((size_t)n_fallback + 1, sizeof(char *));
        if (!models) {
            /* Last resort: prompt directly */
            printf("\nModel [deepseek/deepseek-v4-flash]: ");
            fflush(stdout);
            if (fgets(out_model, out_size, stdin)) {
                char *nl = strchr(out_model, '\n');
                if (nl) *nl = '\0';
                setup_sanitize_pasted_input(out_model);
            }
            if (out_model[0] == '\0')
                strncpy(out_model, "deepseek/deepseek-v4-flash", out_size - 1);
            return;
        }
        for (int i = 0; i < n_fallback; i++)
            models[i] = (char *)fallback_models[i];
        custom_idx = n_fallback - 1;
        n_models = n_fallback;
    }

    /* Use searchable cw_radiolist for model selection */
    {
        /* Build display array: handle NULL placeholder at custom_idx */
        char **display = calloc((size_t)n_models + 1, sizeof(char *));
        if (!display) {
            strncpy(out_model, models[0] ? models[0] : "deepseek/deepseek-v4-flash", out_size - 1);
            goto cleanup_models;
        }
        for (int i = 0; i < n_models; i++) {
            if (i == custom_idx)
                display[i] = (char *)"Custom model (type name manually)";
            else
                display[i] = models[i] ? models[i] : (char *)"";
        }
        display[n_models] = NULL;

        int idx = cw_radiolist("Select Model", display, n_models,
                               0, -1, "Type / to search, ↑↓ to navigate, Enter to select", true);
        if (idx < 0) idx = 0;

        if (idx == custom_idx) {
            /* Custom model */
            printf("\n  Enter model name: ");
            fflush(stdout);
            if (!fgets(out_model, out_size, stdin)) {
                strncpy(out_model, models[0] ? models[0] : "deepseek/deepseek-v4-flash", out_size - 1);
            } else {
                char *nl2 = strchr(out_model, '\n');
                if (nl2) *nl2 = '\0';
                setup_sanitize_pasted_input(out_model);
                if (!out_model[0])
                    strncpy(out_model, models[0] ? models[0] : "deepseek/deepseek-v4-flash", out_size - 1);
            }
        } else if (models[idx]) {
            strncpy(out_model, models[idx], out_size - 1);
        } else {
            strncpy(out_model, "deepseek/deepseek-v4-flash", out_size - 1);
        }
        printf("  Selected model: %s\n", out_model);
        free(display);
    }

cleanup_models:
    /* Free dynamically allocated model names (only strdup'd ones from scraped) */
    if (n_scraped > 0 && models) {
        for (int i = 0; i < n_scraped; i++) {
            if (models[i]) free(models[i]);
        }
    }
    free(models);
}

/* Universal provider model fetcher — fetch model IDs from any provider's
 * /v1/models endpoint. Returns malloc'd array of model ID strings
 * (caller must free each + the array). Sets *out_count to number fetched.
 * Returns NULL on failure (API unreachable, no models).
 /* ── Provider model fetching ────────────────────────────── */

 /* Per-provider curated fallback model lists (port of Python's _DEFAULT_PROVIDER_MODELS).
  * Used when the live /v1/models API is unreachable. NULL-terminated. */
 static const char *SETUP_FALLBACK_NOUS[] = {
     "deepseek/deepseek-v4-flash", "deepseek/deepseek-v4",
     "claude-sonnet-4-20250514", "gpt-4.1", "gemini-2.5-pro", "grok-3", NULL
 };
 static const char *SETUP_FALLBACK_OPENAI[] = {
     "gpt-4.1", "gpt-4o", "gpt-4o-mini", "o3-mini", "o1", "gpt-4-turbo", NULL
 };
 static const char *SETUP_FALLBACK_ANTHROPIC[] = {
     "claude-sonnet-4-20250514", "claude-opus-4", "claude-haiku-3.5", "claude-3.5-sonnet", NULL
 };
 static const char *SETUP_FALLBACK_GOOGLE[] = {
     "gemini-3.1-pro-preview", "gemini-3-pro-preview",
     "gemini-3-flash-preview", "gemini-3.1-flash-lite-preview", NULL
 };
 static const char *SETUP_FALLBACK_DEEPSEEK[] = {
     "deepseek-chat", "deepseek-reasoner", "deepseek/deepseek-v4", "deepseek/deepseek-v4-flash", NULL
 };
 static const char *SETUP_FALLBACK_XAI[] = {
     "grok-3", "grok-3-mini", "grok-3-reasoner", "grok-3-mini-reasoner", NULL
 };
 static const char *SETUP_FALLBACK_OPENROUTER[] = {
     "openai/gpt-4.1", "anthropic/claude-sonnet-4-20250514",
     "google/gemini-2.5-pro", "deepseek/deepseek-v4",
     "meta-llama/llama-4-scout", "mistral/mistral-large-2", NULL
 };
 static const char *SETUP_FALLBACK_AZURE[] = {
     "gpt-4o", "gpt-4o-mini", "gpt-4.1", "o3-mini", NULL
 };
 static const char *SETUP_FALLBACK_BEDROCK[] = {
     "anthropic.claude-sonnet-4-20250514", "anthropic.claude-3-5-sonnet",
     "meta.llama4-scout-17b", "mistral.mistral-large-2407", NULL
 };
 static const char *SETUP_FALLBACK_CODEX[] = {
     "gpt-5.4-codex", "gpt-5.3-codex", "gpt-5.2-codex",
     "claude-sonnet-4.6-codex", "gemini-3-flash-codex", NULL
 };

 /* Map provider index to its fallback list */
 static const char **SETUP_FALLBACKS[] = {
     SETUP_FALLBACK_NOUS,      /* 0 */
     SETUP_FALLBACK_OPENAI,     /* 1 */
     SETUP_FALLBACK_ANTHROPIC,  /* 2 */
     SETUP_FALLBACK_GOOGLE,     /* 3 */
     SETUP_FALLBACK_DEEPSEEK,   /* 4 */
     SETUP_FALLBACK_XAI,        /* 5 */
     SETUP_FALLBACK_OPENROUTER, /* 6 */
     SETUP_FALLBACK_AZURE,      /* 7 */
     SETUP_FALLBACK_BEDROCK,    /* 8 */
     SETUP_FALLBACK_CODEX,      /* 9 */
     NULL,                      /* 10 = custom */
 };

/* ── OpenRouter curated model list (port of Python OPENROUTER_MODELS) ── */
/* Curated preferences used to filter live API results. */
static const char *SETUP_OPENROUTER_CURATED[] = {
    /* Anthropic */
    "anthropic/claude-opus-4.8",
    "anthropic/claude-opus-4.8-fast",
    "anthropic/claude-sonnet-4.6",
    "anthropic/claude-haiku-4.5",
    /* OpenAI */
    "openai/gpt-5.5",
    "openai/gpt-5.5-pro",
    "openai/gpt-5.4-mini",
    /* Google */
    "google/gemini-3-pro-preview",
    "google/gemini-3.1-pro-preview",
    "google/gemini-3.5-flash",
    /* xAI */
    "x-ai/grok-4.3",
    /* DeepSeek */
    "deepseek/deepseek-v4-pro",
    "deepseek/deepseek-v4-flash",
    /* Qwen */
    "qwen/qwen3.7-max",
    "qwen/qwen3.7-plus",
    "qwen/qwen3.6-35b-a3b",
    /* MoonshotAI */
    "moonshotai/kimi-k2.6",
    /* MiniMax */
    "minimax/minimax-m3",
    /* Z-AI */
    "z-ai/glm-5.1",
    /* Xiaomi */
    "xiaomi/mimo-v2.5-pro",
    /* Tencent */
    "tencent/hy3-preview",
    /* StepFun */
    "stepfun/step-3.7-flash",
    /* NVIDIA */
    "nvidia/nemotron-3-super-120b-a12b",
    /* OpenRouter routers */
    "openrouter/pareto-code",
    /* Free tier */
    "openrouter/elephant-alpha",
    "openrouter/owl-alpha",
    "tencent/hy3-preview:free",
    "nvidia/nemotron-3-super-120b-a12b:free",
    "inclusionai/ring-2.6-1t:free",
    NULL,
};

/* Port of Python hermes_cli/models.py:_openrouter_model_supports_tools().
 * Returns true when the model item either has no supported_parameters list
 * or the list explicitly includes "tools". Permissive when field is absent. */
static bool openrouter_model_supports_tools(const json_node_t *item) {
    if (!item || item->type != JSON_OBJECT) return true;
    json_node_t *params = json_object_get(item, "supported_parameters");
    if (!params || params->type != JSON_ARRAY) return true;
    size_t n = json_array_count(params);
    for (size_t i = 0; i < n; i++) {
        json_node_t *p = json_array_get(params, (int)i);
        if (p && p->type == JSON_STRING && strcmp(p->str_val, "tools") == 0)
            return true;
    }
    return false;
}

/* Port of Python hermes_cli/models.py:fetch_openrouter_models().
 * Fetches live OpenRouter catalog, filters by curated preference list +
 * tool-calling support. Returns models in curated list order with
 * descriptions where applicable. Always returns at least the raw curated
 * list (unfiltered) on failure. */
static char **setup_fetch_openrouter_models(const char *api_key,
                                             int *out_count) {
    *out_count = 0;

    /* Step 1: Count curated entries */
    int n_curated = 0;
    while (SETUP_OPENROUTER_CURATED[n_curated]) n_curated++;

    /* Step 2: Fetch live catalog from OpenRouter */
    const char *url = "https://openrouter.ai/api/v1/models";
    char hdrs[512] = {0};
    if (api_key && *api_key) {
        snprintf(hdrs, sizeof(hdrs),
                 "Authorization: Bearer %s\r\nAccept: application/json", api_key);
    } else {
        snprintf(hdrs, sizeof(hdrs), "Accept: application/json");
    }

    /* Build live model lookup: model_id → JSON item */
    typedef struct {
        char *id;
        json_node_t *item;   /* borrowed ref into json_tree — keep tree alive */
    } live_entry_t;
    live_entry_t *live_models = NULL;
    int n_live = 0;
    json_node_t *json_data = NULL;  /* keep alive until filtering done */

    http_t *h = http_new(8);
    if (h) {
        http_resp_t *resp = http_get(h, url, hdrs);
        if (resp && resp->status == 200 && resp->body) {
            json_data = json_parse(resp->body, NULL);
            if (json_data && json_data->type == JSON_OBJECT) {
                json_node_t *arr = json_object_get(json_data, "data");
                if (arr && arr->type == JSON_ARRAY) {
                    size_t alen = json_array_count(arr);
                    if (alen > 0) {
                        live_models = calloc(alen, sizeof(live_entry_t));
                        if (live_models) {
                            for (size_t i = 0; i < alen; i++) {
                                json_node_t *item = json_array_get(arr, (int)i);
                                if (item && item->type == JSON_OBJECT) {
                                    const char *mid = json_object_get_string(item, "id", NULL);
                                    if (mid)
                                        live_models[n_live++] = (live_entry_t){
                                            .id = strdup(mid),
                                            .item = item,
                                        };
                                }
                            }
                        }
                    }
                }
            }
            /* Don't json_free(json_data) yet — live_models[i].item refs into it */
        }
        if (resp) http_resp_free(resp);
        http_free(h);
    }

    /* Step 3: Filter curated list by live data (tool support + availability)
     * matching Python's permissive approach: keep model if live data says it
     * supports tools OR if no supported_parameters field exists. */
    char **result = calloc((size_t)(n_curated + 1), sizeof(char *));
    if (!result) {
        if (json_data) json_free(json_data);
        if (live_models) { for (int i = 0; i < n_live; i++) free(live_models[i].id); free(live_models); }
        return NULL;
    }
    int count = 0;

    for (int i = 0; i < n_curated; i++) {
        const char *preferred = SETUP_OPENROUTER_CURATED[i];
        /* Find in live data */
        json_node_t *live_item = NULL;
        bool found = false;
        for (int j = 0; j < n_live; j++) {
            if (strcmp(live_models[j].id, preferred) == 0) {
                live_item = live_models[j].item;
                found = true;
                break;
            }
        }
        if (!found) continue;  /* Model not available in live catalog — skip */
        /* Check tool support (permissive: keep if supported_parameters absent) */
        if (!openrouter_model_supports_tools(live_item))
            continue;
        result[count++] = strdup(preferred);
    }

    /* Clean up live model lookup */
    if (live_models) { for (int i = 0; i < n_live; i++) free(live_models[i].id); free(live_models); }
    if (json_data) json_free(json_data);

    /* Step 4: If filtering yielded nothing, return raw curated list (unfiltered) */
    if (count == 0) {
        for (int i = 0; i < n_curated; i++)
            result[count++] = strdup(SETUP_OPENROUTER_CURATED[i]);
    }

    result[count] = NULL;
    *out_count = count;
    return result;
}

/* Port of Python hermes_cli/models.py:fetch_models_with_pricing().
 *
 * Generic providers: fetches /v1/models via live API, merges with curated
 * fallback (live models first, then missing curated entries).
 *
 * OpenRouter specifically: applies curated-preference filtering matching
 * Python's fetch_openrouter_models() — only models from the curated list
 * that advertise tool-calling support are kept, returned in curated order.
 *
 * Always returns at least the curated list. Caller must free each string + array. */
static char **setup_fetch_provider_models(const char *provider_name,
                                           const char *api_key,
                                           int *out_count) {
    *out_count = 0;
    if (!provider_name) return NULL;

    /* Determine provider index for curated fallback lookup */
    int provider_idx = -1;
    for (int i = 0; SETUP_PROVIDERS[i]; i++) {
        if (strcasecmp(provider_name, SETUP_PROVIDERS[i]) == 0) {
            provider_idx = i;
            break;
        }
    }

    /* Curated fallback list (port of Python _PROVIDER_MODELS) */
    const char **fallback = NULL;
    if (provider_idx >= 0 && provider_idx < (int)(sizeof(SETUP_FALLBACKS)/sizeof(SETUP_FALLBACKS[0])))
        fallback = SETUP_FALLBACKS[provider_idx];

    /* ── OpenRouter special path: curated-preference filtering ── */
    if (strcmp(provider_name, "openrouter") == 0) {
        return setup_fetch_openrouter_models(api_key, out_count);
    }

    /* ── Generic provider path: live API fetch + dedup with curated ── */
    const provider_metadata_t *meta = provider_metadata_find(provider_name);
    char **live_models = NULL;
    int n_live = 0;

    if (meta && meta->base_url && *meta->base_url) {
        char url[1024];
        int uses_xapi_key = 0;

        if (strcmp(provider_name, "anthropic") == 0) {
            snprintf(url, sizeof(url), "https://api.anthropic.com/v1/models");
            uses_xapi_key = 1;
        } else if (strcmp(provider_name, "google") == 0) {
            snprintf(url, sizeof(url), "https://generativelanguage.googleapis.com/v1beta/models");
        } else {
            /* Generic: strip trailing slash, append /models.
             * This handles all OpenAI-compatible providers correctly:
             *   https://api.openai.com/v1       → https://api.openai.com/v1/models
             *   https://openrouter.ai/api/v1     → https://openrouter.ai/api/v1/models
             *   https://api.groq.com/openai/v1   → https://api.groq.com/openai/v1/models
             *   https://api.deepseek.com/v1      → https://api.deepseek.com/v1/models
             * Fixes bug where /v1 stripping produced wrong URLs (e.g. missing /v1
             * for OpenAI, double /v1 for Groq). */
            const char *base = meta->base_url;
            size_t blen = strlen(base);
            while (blen > 0 && base[blen-1] == '/') blen--;
            /* Just append /models to the base URL — don't strip /v1 */
            snprintf(url, sizeof(url), "%.*s/models", (int)blen, base);
        }

        char hdrs[1024] = {0};
        if (uses_xapi_key && api_key && *api_key) {
            snprintf(hdrs, sizeof(hdrs),
                     "x-api-key: %s\r\n"
                     "anthropic-version: 2023-06-01\r\n"
                     "Content-Type: application/json", api_key);
        } else if (api_key && *api_key) {
            snprintf(hdrs, sizeof(hdrs),
                     "Authorization: Bearer %s\r\n"
                     "Content-Type: application/json", api_key);
        } else {
            snprintf(hdrs, sizeof(hdrs), "Content-Type: application/json");
        }

        http_t *h = http_new(10);
        if (h) {
            http_resp_t *resp = http_get(h, url, hdrs);
            if (resp && resp->status == 200 && resp->body) {
                json_node_t *data = json_parse(resp->body, NULL);
                if (data && data->type == JSON_OBJECT) {
                    json_node_t *arr = json_object_get(data, "data");
                    if (!arr) arr = json_object_get(data, "models");
                    if (arr && arr->type == JSON_ARRAY) {
                        size_t alen = json_array_count(arr);
                        if (alen > 0) {
                            live_models = calloc(alen + 1, sizeof(char *));
                            if (live_models) {
                                for (size_t i = 0; i < alen; i++) {
                                    json_node_t *item = json_array_get(arr, (int)i);
                                    if (item && item->type == JSON_OBJECT) {
                                        const char *mid = json_object_get_string(item, "id", NULL);
                                        if (mid) live_models[n_live++] = strdup(mid);
                                    } else if (item && item->type == JSON_STRING) {
                                        live_models[n_live++] = strdup(item->str_val);
                                    }
                                }
                            }
                        }
                    }
                }
                if (data) json_free(data);
            }
            if (resp) http_resp_free(resp);
            http_free(h);
        }
    }

    /* Build final list: curated fallback + live models (dedup'd, live first) */
    {
        /* Count curated entries */
        int n_curated = 0;
        if (fallback) { while (fallback[n_curated]) n_curated++; }

        /* Build set of model IDs for dedup — put live models in a hash set */
        char **all = calloc((size_t)(n_curated + n_live + 1), sizeof(char *));
        if (!all) {
            if (live_models) { for (int i=0; i<n_live; i++) free(live_models[i]); free(live_models); }
            return NULL;
        }
        int count = 0;

        /* Live models first */
        for (int i = 0; i < n_live; i++) {
            bool dup = false;
            for (int j = 0; j < count; j++) {
                if (strcasecmp(all[j], live_models[i]) == 0) { dup = true; break; }
            }
            if (!dup) all[count++] = live_models[i];
            else free(live_models[i]);
        }
        free(live_models);

        /* Then curated models (dedup'ed against live) */
        if (fallback) {
            for (int i = 0; i < n_curated; i++) {
                bool dup = false;
                for (int j = 0; j < count; j++) {
                    if (strcasecmp(all[j], fallback[i]) == 0) { dup = true; break; }
                }
                if (!dup) all[count++] = strdup(fallback[i]);
            }
        }

        all[count] = NULL;
        *out_count = count;

        /* If we got nothing from either source, return NULL */
        if (count == 0) { free(all); return NULL; }
        return all;
    }
}

/* Universal paginated model picker — shows fetched models with n/p navigation.
 * Handles selection and writes to out_model. Falls back to prompt on empty list. */
static void setup_pick_model(char *out_model, size_t out_size,
                              char **models, int n_models,
                              const char *default_model) {
    if (n_models <= 0) {
        /* No models — prompt manually */
        printf("\nModel [%s]: ", default_model ? default_model : "");
        fflush(stdout);
        if (fgets(out_model, out_size, stdin)) {
            char *nl = strchr(out_model, '\n');
            if (nl) *nl = '\0';
            setup_sanitize_pasted_input(out_model);
        }
        if (out_model[0] == '\0' && default_model)
            strncpy(out_model, default_model, out_size - 1);
        return;
    }

    /* Build display list: model names + custom entry at end */
    int display_count = n_models + 1;
    char **display = calloc((size_t)(display_count + 1), sizeof(char *));
    if (!display) {
        strncpy(out_model, models[0], out_size - 1);
        return;
    }
    for (int i = 0; i < n_models; i++)
        display[i] = models[i]; /* borrowed reference */
    display[n_models] = "Custom model (type name manually)";
    display[n_models + 1] = NULL;

    int idx = cw_radiolist("Select Model", display, display_count,
                           0, -1, "Type / to search, ↑↓ to navigate, Enter to select", true);
    if (idx < 0) idx = 0;

    if (idx == n_models) {
        /* Custom — prompt for name */
        printf("\nModel name: ");
        fflush(stdout);
        if (fgets(out_model, out_size, stdin)) {
            char *nl = strchr(out_model, '\n');
            if (nl) *nl = '\0';
            setup_sanitize_pasted_input(out_model);
        }
        if (out_model[0] == '\0' && default_model)
            strncpy(out_model, default_model, out_size - 1);
    } else {
        strncpy(out_model, display[idx], out_size - 1);
    }
    printf("  Selected: %s\n", out_model);
    free(display);
}

/* ── Public setup entry points ──────────────────────────────────── */

/* Port of Python hermes_cli/setup.py:cmd_setup(non_interactive=True). */
bool hermes_config_setup_noninteractive(const char *config_dir) {
    char dir[4096];
    if (config_dir && config_dir[0])
        snprintf(dir, sizeof(dir), "%s", config_dir);
    else {
        const char *home = getenv("SLERMES_HOME");
        if (!home) home = getenv("HERMES_HOME");
        if (!home) home = getenv("HOME");
        if (getenv("SLERMES_HOME") || getenv("HERMES_HOME"))
            snprintf(dir, sizeof(dir), "%s", home ? home : "");
        else
            snprintf(dir, sizeof(dir), "%s/.slermes", home ? home : ".");
    }
    if (!dir[0]) { fprintf(stderr, "Error: cannot determine home.\n"); return false; }

    printf("\n=== Non-Interactive Setup ===\n\n");
    struct { const char *var; const char *provider; const char *key; } probes[] = {
        {"NOUS_API_KEY",       "nous",      "NOUS_API_KEY"},
        {"OPENAI_API_KEY",     "openai",    "OPENAI_API_KEY"},
        {"ANTHROPIC_API_KEY",  "anthropic", "ANTHROPIC_API_KEY"},
        {"DEEPSEEK_API_KEY",   "deepseek",  "DEEPSEEK_API_KEY"},
        {"GOOGLE_API_KEY",     "google",    "GOOGLE_API_KEY"},
        {"XAI_API_KEY",        "xai",       "XAI_API_KEY"},
        {"OPENROUTER_API_KEY","openrouter","OPENROUTER_API_KEY"},
        {NULL, NULL, NULL}
    };

    const char *provider = NULL;
    const char *api_key = NULL;
    const char *key_var = NULL;
    for (int i = 0; probes[i].var; i++) {
        const char *val = getenv(probes[i].var);
        if (val && val[0]) { provider = probes[i].provider; api_key = val; key_var = probes[i].key; break; }
    }

    if (!provider) {
        printf("  No API keys found. Set: NOUS_API_KEY, OPENAI_API_KEY, etc.\n");
        return false;
    }

    printf("  Found %s\n", key_var);
    const char *model = getenv("HERMES_MODEL");
    if (!model) model = getenv("SLERMES_MODEL");
    if (!model) model = "claude-sonnet-4";

    char path[4096];
    snprintf(path, sizeof(path), "%s/config.yaml", dir);
    struct stat st;
    if (stat(dir, &st) != 0) mkdir(dir, 0700);
    FILE *fp = fopen(path, "w");
    if (!fp) { fprintf(stderr, "Error: cannot write %s\n", path); return false; }
    fprintf(fp, "# Hermes Agent Configuration\n# Generated by non-interactive setup\n\n");
    fprintf(fp, "provider: \"%s\"\ndefault_model: \"%s\"\n\nmodel:\n  default: \"%s\"\n",
            provider, model, model);
    fclose(fp);
    setup_save_env(key_var, api_key);
    printf("✅ Non-interactive setup complete. Provider: %s, Model: %s\n", provider, model);
    return true;
}

/* Run a specific setup section. Port of Python hermes_cli/setup.py:run_setup_section(). */
bool hermes_config_setup_section(const char *config_dir, const char *section) {
    hermes_config_t cfg;
    hermes_config_defaults(&cfg);
    char path[4096];
    if (config_dir && config_dir[0]) snprintf(path, sizeof(path), "%s/config.yaml", config_dir);
    else { const char *home = getenv("HOME") ? getenv("HOME") : ".";
           snprintf(path, sizeof(path), "%s/.slermes/config.yaml", home); }

    struct stat st;
    if (stat(path, &st) == 0) {
        yaml_doc_t *doc = yaml_parse_file(path, NULL);
        if (doc) {
            const char *p = yaml_get_string(doc, "provider");
            if (p) snprintf(cfg.provider, sizeof(cfg.provider), "%s", p);
            const char *m = yaml_get_string(doc, "default_model");
            if (m) snprintf(cfg.model, sizeof(cfg.model), "%s", m);
            yaml_free(doc);
        }
    }

    if (strcmp(section, "model") == 0) {
        printf("\n=== Model & Provider ===\nCurrent: %s / %s\n\n", cfg.provider, cfg.model);
        char apikey[2048] = {0}, provider[64] = {0}, model[128] = {0};
        snprintf(provider, sizeof(provider), "%s", cfg.provider[0] ? cfg.provider : "openai");
        int nprov = 0;
        for (int i = 0; SETUP_PROVIDERS[i]; i++) nprov++;

        /* Build display labels and use searchable radiolist */
        char **labels = calloc((size_t)nprov + 1, sizeof(char *));
        int default_idx = 0;
        if (labels) {
            for (int i = 0; i < nprov; i++) {
                labels[i] = (char *)SETUP_LABELS[i];
                if (strcmp(provider, SETUP_PROVIDERS[i]) == 0) default_idx = i;
            }
            labels[nprov] = NULL;
        }
        int idx = cw_radiolist("Select provider:", labels ? labels : (char **)SETUP_PROVIDERS,
                               nprov, default_idx, -1,
                               "Type / to search, ↑↓ to navigate, Enter to select", true);
        if (idx < 0) idx = default_idx;
        if (labels) free(labels);
        snprintf(provider, sizeof(provider), "%s", SETUP_PROVIDERS[idx]);
        bool is_nous = (idx == 0);
        if (is_nous) {
            int a = 0;
            while (a < 3) { a++;
                oauth_token_t *tok = nous_device_code_login(300);
                if (tok && tok->access_token) {
                    snprintf(apikey, sizeof(apikey), "%s", tok->access_token);
                    if (tok->refresh_token && tok->refresh_token[0])
                        setup_save_env("NOUS_REFRESH_TOKEN", tok->refresh_token);
                    oauth_token_free(tok); break;
                }
            }
            setup_nous_model_picker(model, sizeof(model));
        } else {
            /* Formatted API key prompt (masked input) */
            if (idx >= 0 && idx < (int)(sizeof(SETUP_KEY_INFO)/sizeof(SETUP_KEY_INFO[0]))) {
                char key[2048] = {0};
                setup_prompt_api_key(&SETUP_KEY_INFO[idx], key, sizeof(key));
                if (key[0]) {
                    snprintf(apikey, sizeof(apikey), "%s", key);
                    setup_save_env(SETUP_KEY_INFO[idx].key_env, key);
                }
            } else {
                printf("API key: "); fflush(stdout);
                if (fgets(apikey, sizeof(apikey), stdin)) {
                    char *nl = strchr(apikey, '\n'); if (nl) *nl = '\0';
                }
            }
            /* Optional base URL override */
            char base_override[1024] = {0};
            setup_prompt_base_url(provider, base_override, sizeof(base_override));
            if (base_override[0]) {
                snprintf(cfg.provider_cfg.base_url, sizeof(cfg.provider_cfg.base_url), "%s", base_override);
                snprintf(cfg.base_url, sizeof(cfg.base_url), "%s", base_override);
            }
            int nm = 0;
            char **pm = setup_fetch_provider_models(provider, apikey, &nm);
            setup_pick_model(model, sizeof(model), pm, nm, SETUP_MODELS[idx]);
            if (pm) { for (int i=0;i<nm;i++) free(pm[i]); free(pm); }
        }
        snprintf(cfg.provider, sizeof(cfg.provider), "%s", provider);
        snprintf(cfg.model, sizeof(cfg.model), "%s", model);
        snprintf(cfg.provider_cfg.provider, sizeof(cfg.provider_cfg.provider), "%s", provider);
        snprintf(cfg.provider_cfg.model, sizeof(cfg.provider_cfg.model), "%s", model);
        setup_set_provider_endpoint(&cfg, provider);
        if (apikey[0]) {
            const char *kv = "OPENAI_API_KEY";
            if (is_nous) kv = "NOUS_API_KEY";
            else if (strcmp(provider,"anthropic")==0) kv = "ANTHROPIC_API_KEY";
            else if (strcmp(provider,"google")==0) kv = "GOOGLE_API_KEY";
            else if (strcmp(provider,"deepseek")==0) kv = "DEEPSEEK_API_KEY";
            else if (strcmp(provider,"xai")==0) kv = "XAI_API_KEY";
            else if (strcmp(provider,"openrouter")==0) kv = "OPENROUTER_API_KEY";
            setup_save_env(kv, apikey);
        }
        FILE *fp = fopen(path, "w");
        if (fp) { hermes_config_export(&cfg, path); fclose(fp); }
        return true;
    }
    if (strcmp(section, "tts") == 0) { setup_tts(&cfg); goto SAVE; }
    if (strcmp(section, "terminal") == 0) { setup_terminal_backend(&cfg); goto SAVE; }
    if (strcmp(section, "gateway") == 0) { setup_gateway(&cfg); goto SAVE; }
    if (strcmp(section, "tools") == 0) {
        printf("Tools: use 'slermes config set web_search.provider' / image_gen.provider / tts.provider\n");
        return true;
    }
    if (strcmp(section, "agent") == 0) { setup_agent_settings(&cfg); goto SAVE; }
    fprintf(stderr, "Unknown section: '%s'. Use: model, tts, terminal, gateway, tools, agent\n", section);
    return false;
SAVE:
    FILE *fp = fopen(path, "w");
    if (fp) { hermes_config_export(&cfg, path); fclose(fp); }
    return true;
}

/* Only prompt for missing items. Port of Python hermes_cli/setup.py:quick_setup(). */
bool hermes_config_setup_quick(const char *config_dir) {
    char path[4096];
    if (config_dir && config_dir[0]) snprintf(path, sizeof(path), "%s/config.yaml", config_dir);
    else { const char *home = getenv("HOME") ? getenv("HOME") : ".";
           snprintf(path, sizeof(path), "%s/.slermes/config.yaml", home); }
    struct stat st;
    bool has_provider = false, has_model = false;
    if (stat(path, &st) == 0) {
        yaml_doc_t *doc = yaml_parse_file(path, NULL);
        if (doc) {
            const char *p = yaml_get_string(doc, "provider");
            if (p && p[0]) has_provider = true;
            const char *m = yaml_get_string(doc, "default_model");
            if (m && m[0]) has_model = true;
            yaml_free(doc);
        }
    }
    if (!has_provider || !has_model) {
        printf("\n=== Quick Setup ===\nMissing provider/model — configuring now.\n\n");
        hermes_config_setup_section(config_dir, "model");
    } else {
        printf("✅ All required settings found. Run 'slermes setup' for full wizard.\n");
    }
    return true;
}

/* One-shot Nous Portal OAuth login and setup. Port of Python --portal flag. */
bool hermes_config_setup_portal(void) {
    printf("\n=== Nous Portal Setup ===\n\n");
    printf("Logging in to Nous Research...\n");
    oauth_token_t *tok = nous_device_code_login(300);
    if (tok && tok->access_token) {
        setup_save_env("NOUS_API_KEY", tok->access_token);
        if (tok->refresh_token && tok->refresh_token[0])
            setup_save_env("NOUS_REFRESH_TOKEN", tok->refresh_token);
        printf("✅ Nous Portal login successful!\n");
        oauth_token_free(tok);
    } else {
        printf("⚠️  OAuth login failed. Use 'slermes config set provider nous' and set NOUS_API_KEY.\n");
    }
    return true;
}

/* Interactive setup wizard — creates config.yaml + .env.
 * Port of Python hermes_cli/setup.py:run_setup_wizard(). */
bool hermes_config_setup_interactive(const char *config_dir) {
    char dir[4096];
    if (config_dir && config_dir[0]) {
        snprintf(dir, sizeof(dir), "%s", config_dir);
    } else {
        const char *home = getenv("SLERMES_HOME");
        if (!home) home = getenv("HERMES_HOME");
        if (!home) home = getenv("HOME");
        if (!home) { fprintf(stderr, "Error: cannot determine home.\n"); return false; }
        if (getenv("SLERMES_HOME") || getenv("HERMES_HOME"))
            snprintf(dir, sizeof(dir), "%s", home);
        else
            snprintf(dir, sizeof(dir), "%s/.slermes", home);
    }

    /* Check for non-interactive mode first */
    if (!setup_is_interactive()) {
        setup_print_noninteractive_guidance(
            "Non-interactive stdin detected (CI/CD, headless SSH, pipe).");
        return false;
    }

    /* ── Welcome header ── */
    printf("\n=== Slermes Setup ===\n\n");
    printf("Configure your Slermes Agent installation.\n");
    printf("Press Ctrl+C at any time to exit.\n\n");

    /* ── Check for existing config ── */
    struct stat st;
    char path[4096];
    snprintf(path, sizeof(path), "%s/config.yaml", dir);
    bool is_existing = (stat(path, &st) == 0);

    /* ── Setup mode: Quick (Nous Portal) vs Full ── */
    int setup_mode = 0;
    if (!is_existing) {
        /* First-time setup — offer quick setup */
        const char *mode_choices[] = {
            "Quick Setup (Nous Portal) — free OAuth login, no API keys (recommended)",
            "Full setup — configure every provider & option yourself",
        };
        setup_mode = setup_prompt_choice(
            "How would you like to set up Slermes?",
            mode_choices, 2, 0);
    }
    /* For existing installs, always go to full setup */

    /* Variables for all sections */
    hermes_config_t cfg;
    hermes_config_defaults(&cfg);
    snprintf(cfg.config_path, sizeof(cfg.config_path), "%s", path);
    char apikey[2048] = {0};
    char provider[64] = {0};
    char model[128] = {0};
    bool is_nous = false;

    /* ── Quick Setup: Nous Portal + defaults ── */
    if (!is_existing && setup_mode == 0) {
        printf("\n");
        setup_print_header("Nous Portal");

        /* Nous Portal OAuth login */
        int attempts = 0;
        while (attempts < 3) {
            attempts++;
            oauth_token_t *tok = nous_device_code_login(300);
            if (tok && tok->access_token) {
                strncpy(apikey, tok->access_token, sizeof(apikey) - 1);
                printf("\n✅ Nous Portal login successful!\n");
                if (tok->refresh_token && tok->refresh_token[0]) {
                    /* Save refresh_token for auto-refresh */
                    setup_save_env("NOUS_REFRESH_TOKEN", tok->refresh_token);
                    printf("  ✅ Token auto-refresh enabled.\n");
                } else {
                    printf("  ⚠️  No refresh token received — token will need manual renewal.\n");
                }
                oauth_token_free(tok);
                break;
            }
            if (attempts >= 3) {
                printf("\n⚠️  OAuth login failed after %d attempts.\n", attempts);
                printf("Options:\n");
                printf("  1) Retry OAuth login\n");
                printf("  2) Enter NOUS_API_KEY manually\n");
                printf("  3) Skip — configure later\n");
                printf("  Choice [1-3]: ");
                fflush(stdout);
                char choice[16];
                if (!fgets(choice, sizeof(choice), stdin)) break;
                int c = atoi(choice);
                if (c == 1) { attempts = 0; continue; }
                if (c == 2) break;
                break;
            }
        }

        /* Manual API key fallback */
        if (!apikey[0]) {
            printf("\nEnter your NOUS_API_KEY (leave blank to set later): ");
            fflush(stdout);
            if (fgets(apikey, sizeof(apikey), stdin)) {
                char *nl = strchr(apikey, '\n');
                if (nl) *nl = '\0';
                setup_sanitize_pasted_input(apikey);
            }
        }
        strncpy(provider, "nous", sizeof(provider) - 1);
        setup_nous_model_picker(model, sizeof(model));
        is_nous = true;

        /* Apply agent defaults silently (Python: _apply_default_agent_settings) */
        setup_apply_default_agent_settings(&cfg);

        /* ── Terminal Backend ── */
        setup_terminal_backend(&cfg);

        /* ── Gateway: offer messaging setup ── */
        const char *gw_choices[] = {
            "Set up messaging now (recommended)",
            "Skip — configure later",
        };
        int gw_choice = setup_prompt_choice(
            "Connect a messaging platform?",
            gw_choices, 2, 0);

        if (gw_choice == 0) {
            setup_gateway(&cfg);
        }

        goto SAVE_AND_FINISH;
    }

    /* ═══════════════════════════════════════════
     * Full Setup — all sections
     * ═══════════════════════════════════════════ */

    /* ── Section 1: Model & Provider ── */
    setup_print_header("Model & Provider");
    printf("Choose how to connect to your main chat model.\n\n");

    int nprov = 0;
    for (int i = 0; SETUP_PROVIDERS[i]; i++) nprov++;

    /* Build display labels and use searchable radiolist */
    char **labels = calloc((size_t)nprov + 1, sizeof(char *));
    int default_idx = 0;
    if (labels) {
        for (int i = 0; i < nprov; i++) {
            labels[i] = (char *)SETUP_LABELS[i];
        }
        labels[nprov] = NULL;
    }
    int provider_idx = cw_radiolist("Select an AI provider:",
        labels ? labels : (char **)SETUP_PROVIDERS,
        nprov, default_idx, -1,
        "Type / to search, ↑↓ to navigate, Enter to select", true);
    if (provider_idx < 0) provider_idx = 1;
    if (labels) free(labels);

    strncpy(provider, SETUP_PROVIDERS[provider_idx], sizeof(provider) - 1);
    const char *default_model = SETUP_MODELS[provider_idx];
    is_nous = (provider_idx == 0);

    /* Model + API key */
    if (is_nous) {
        /* Nous Portal OAuth */
        setup_print_header("Nous Portal Login");
        int attempts = 0;
        while (attempts < 3) {
            attempts++;
            oauth_token_t *tok = nous_device_code_login(300);
            if (tok && tok->access_token) {
                strncpy(apikey, tok->access_token, sizeof(apikey) - 1);
                printf("\n✅ Nous Portal login successful!\n");
                if (tok->refresh_token && tok->refresh_token[0]) {
                    /* Save refresh_token for auto-refresh */
                    setup_save_env("NOUS_REFRESH_TOKEN", tok->refresh_token);
                    printf("  ✅ Token auto-refresh enabled.\n");
                } else {
                    printf("  ⚠️  No refresh token received — token will need manual renewal.\n");
                }
                oauth_token_free(tok);
                break;
            }
            if (attempts >= 3) {
                printf("\n⚠️  OAuth login failed after %d attempts.\n", attempts);
                printf("Options:\n");
                printf("  1) Retry OAuth login\n");
                printf("  2) Enter NOUS_API_KEY manually\n");
                printf("  3) Skip — configure later\n");
                printf("  Choice [1-3]: ");
                fflush(stdout);
                char choice[16];
                if (!fgets(choice, sizeof(choice), stdin)) break;
                int c = atoi(choice);
                if (c == 1) { attempts = 0; continue; }
                if (c == 2) break;
                break;
            }
        }
        if (!apikey[0]) {
            printf("\nEnter your NOUS_API_KEY (leave blank to set later): ");
            fflush(stdout);
            if (fgets(apikey, sizeof(apikey), stdin)) {
                char *nl = strchr(apikey, '\n');
                if (nl) *nl = '\0';
                setup_sanitize_pasted_input(apikey);
            }
        }
        setup_nous_model_picker(model, sizeof(model));
    } else {
        /* Formatted API key prompt (masked input) */
        if (provider_idx >= 0 && provider_idx < (int)(sizeof(SETUP_KEY_INFO)/sizeof(SETUP_KEY_INFO[0]))) {
            char key[2048] = {0};
            setup_prompt_api_key(&SETUP_KEY_INFO[provider_idx], key, sizeof(key));
            if (key[0]) {
                snprintf(apikey, sizeof(apikey), "%s", key);
                setup_save_env(SETUP_KEY_INFO[provider_idx].key_env, key);
            }
        } else {
            printf("\nAPI key (leave blank to set later in .env): ");
            fflush(stdout);
            if (fgets(apikey, sizeof(apikey), stdin)) {
                char *nl = strchr(apikey, '\n');
                if (nl) *nl = '\0';
                setup_sanitize_pasted_input(apikey);
            }
        }

        /* Optional base URL override */
        char base_override[1024] = {0};
        setup_prompt_base_url(provider, base_override, sizeof(base_override));
        if (base_override[0]) {
            snprintf(cfg.provider_cfg.base_url, sizeof(cfg.provider_cfg.base_url), "%s", base_override);
            snprintf(cfg.base_url, sizeof(cfg.base_url), "%s", base_override);
        }

        /* Fetch models from provider API (works without key for public endpoints) */
        int n_models = 0;
        char **provider_models = setup_fetch_provider_models(provider, apikey, &n_models);
        setup_pick_model(model, sizeof(model), provider_models, n_models, default_model);
        if (provider_models) {
            for (int i = 0; i < n_models; i++) free(provider_models[i]);
            free(provider_models);
        }
    }

    /* ── Section 2: Terminal Backend ── */
    setup_terminal_backend(&cfg);

    /* ── Section 3: Agent Settings (only for first install) ── */
    if (!is_existing) {
        setup_apply_default_agent_settings(&cfg);
    } else {
        setup_agent_settings(&cfg);
    }

    /* ── Section 4: Messaging Platforms ── */
    const char *gw_choices[] = {
        "Configure messaging now (recommended)",
        "Skip — configure later",
    };
    int gw_choice = setup_prompt_choice(
        "Connect a messaging platform? (Telegram, Slack, etc.)",
        gw_choices, 2, 0);

    if (gw_choice == 0) {
        setup_gateway(&cfg);
    }

    /* ── Section 5: TTS Provider (optional) ── */
    const char *tts_choices[] = {
        "Configure Text-to-Speech now",
        "Skip — use default (Edge TTS)",
    };
    int tts_choice = setup_prompt_choice(
        "Set up Text-to-Speech?",
        tts_choices, 2, 1);
    if (tts_choice == 0) {
        setup_tts(&cfg);
    }

    /* ── Section 6: Web Search + Image Gen + Credential Pool (optional) ── */
    const char *tools_choices[] = {
        "Configure tools now (web search, image gen, credential pool)",
        "Skip — configure later via 'slermes config'",
    };
    int tools_choice = setup_prompt_choice(
        "Set up tool providers?",
        tools_choices, 2, 1);
    if (tools_choice == 0) {
        /* Web search */
        if (setup_prompt_yes_no("Configure web search provider?", false)) {
            const char *ws_labels[] = {
                "Tavily (search API, needs API key)",
                "Firecrawl (crawl + search, needs API key)",
                "Exa (semantic search, needs API key)",
                "None / Skip",
                NULL,
            };
            int ws = setup_prompt_choice("Select web search provider:", ws_labels, 4, 3);
            const char *ws_keys[] = {"tavily", "firecrawl", "exa", "none"};
            snprintf(cfg.tools.web_search_backend, sizeof(cfg.tools.web_search_backend), "%s", ws_keys[ws]);
            if (ws < 3) {
                const char *ws_envs[] = {"TAVILY_API_KEY", "FIRECRAWL_API_KEY", "EXA_API_KEY"};
                const char *existing = getenv(ws_envs[ws]);
                if (!existing || !existing[0]) {
                    char key[512];
                    printf("  %s API key: ", ws_labels[ws]);
                    fflush(stdout);
                    if (fgets(key, sizeof(key), stdin)) {
                        char *nl = strchr(key, '\n'); if (nl) *nl = '\0';
                        setup_sanitize_pasted_input(key);
                        if (key[0]) setup_save_env(ws_envs[ws], key);
                    }
                }
            }
        }
        /* Image gen */
        if (setup_prompt_yes_no("Configure image generation provider?", false)) {
            const char *ig_labels[] = {
                "Fal.ai (fast inference, needs API key)",
                "Stability AI (Stable Diffusion, needs API key)",
                "OpenAI DALL-E (needs API key)",
                "None / Skip",
                NULL,
            };
            int ig = setup_prompt_choice("Select image generation provider:", ig_labels, 4, 3);
            const char *ig_keys[] = {"fal", "stability", "dalle", "none"};
            printf("  Image gen: %s (configure via config.yaml)\n", ig_keys[ig]);
        }
    }

SAVE_AND_FINISH:
    /* Apply provider/model/key to both provider_cfg (export/use path) and flat fields (summary path) */
    strncpy(cfg.provider_cfg.provider, provider, sizeof(cfg.provider_cfg.provider) - 1);
    snprintf(cfg.provider_cfg.model, sizeof(cfg.provider_cfg.model), "%s", model);
    strncpy(cfg.provider, provider, sizeof(cfg.provider) - 1);
    snprintf(cfg.model, sizeof(cfg.model), "%s", model);

    /* Set correct base_url and api_mode from provider_metadata for known providers */
    setup_set_provider_endpoint(&cfg, provider);

    /* ── Write config.yaml ── */
    if (stat(dir, &st) != 0) mkdir(dir, 0700);
    snprintf(path, sizeof(path), "%s/config.yaml", dir);
    hermes_config_export(&cfg, path);
    printf("  Created: %s\n", path);

    /* ── Write .env — use setup_save_env to preserve gateway tokens already saved ── */
    snprintf(path, sizeof(path), "%s/.env", dir);
    if (apikey[0]) {
        const char *key_var = "OPENAI_API_KEY";
        if (is_nous) key_var = "NOUS_API_KEY";
        else if (strcmp(provider, "anthropic") == 0) key_var = "ANTHROPIC_API_KEY";
        else if (strcmp(provider, "google") == 0) key_var = "GOOGLE_API_KEY";
        else if (strcmp(provider, "deepseek") == 0) key_var = "DEEPSEEK_API_KEY";
        else if (strcmp(provider, "xai") == 0) key_var = "XAI_API_KEY";
        else if (strcmp(provider, "openrouter") == 0) key_var = "OPENROUTER_API_KEY";
        else if (strcmp(provider, "azure") == 0) key_var = "AZURE_API_KEY";
        else if (strcmp(provider, "bedrock") == 0) key_var = "AWS_ACCESS_KEY_ID";
        setup_save_env(key_var, apikey);
    }
    /* Ensure .env exists with commented defaults if empty */
    {
        FILE *f = fopen(path, "a");
        if (f) {
            /* Only append default comments if file looks empty or has only whitespace */
            fseek(f, 0, SEEK_END);
            if (ftell(f) <= 1) {
                fprintf(f, "#NOUS_API_KEY= (set via 'slermes portal')\n");
                fprintf(f, "#OPENAI_API_KEY=sk-...\n");
                fprintf(f, "#ANTHROPIC_API_KEY=sk-ant-...\n");
                fprintf(f, "#GOOGLE_API_KEY=AIza...\n");
                fprintf(f, "#DEEPSEEK_API_KEY=sk-...\n");
                fprintf(f, "#XAI_API_KEY=xai-...\n");
            }
            fclose(f);
        }
    }
    printf("  Created: %s\n", path);

    /* ── Print summary ── */
    if (is_nous && apikey[0])
        printf("\n✅ Setup complete! You're ready to go.\n");
    else
        printf("\nSetup complete! Edit .env to add API keys.\n");

    setup_print_summary(&cfg);
    return true;
}

/* ================================================================
 *  Platform config store (mirrors Python's Dict[Platform, PlatformConfig])
 * ================================================================ */

static hermes_platform_cfg_t g_platform_cfgs[HERMES_MAX_PLATFORM_CFG];
static int g_platform_cfg_count = 0;

void hermes_config_load_platforms(void *yaml_doc) {
    yaml_doc_t *doc = (yaml_doc_t *)yaml_doc;
    if (!doc) return;

    g_platform_cfg_count = 0;
    size_t key_count = 0;
    char **keys = yaml_map_keys(doc, "gateway.platforms", &key_count);
    if (!keys) return;

    for (size_t i = 0; i < key_count && g_platform_cfg_count < HERMES_MAX_PLATFORM_CFG; i++) {
        const char *name = keys[i];
        if (!name || !name[0]) continue;

        char path[576];
        hermes_platform_cfg_t *cfg = &g_platform_cfgs[g_platform_cfg_count];
        memset(cfg, 0, sizeof(*cfg));
        (void)snprintf(cfg->name, sizeof(cfg->name), "%s", name);

        snprintf(path, sizeof(path), "gateway.platforms.%s.token", name);
        const char *token = yaml_get_string(doc, path);
        if (token) snprintf(cfg->token, sizeof(cfg->token), "%s", token);

        snprintf(path, sizeof(path), "gateway.platforms.%s.api_key", name);
        const char *api_key = yaml_get_string(doc, path);
        if (api_key) snprintf(cfg->api_key, sizeof(cfg->api_key), "%s", api_key);

        snprintf(path, sizeof(path), "gateway.platforms.%s.home_channel", name);
        const char *hc = yaml_get_string(doc, path);
        if (hc) snprintf(cfg->home_channel, sizeof(cfg->home_channel), "%s", hc);

        snprintf(path, sizeof(path), "gateway.platforms.%s.enabled", name);
        cfg->enabled = yaml_get_bool(doc, path, false);

        /* ═══ Telegram-specific config fields ═══
         * Port of Python TelegramAdapter config fields (telegram.py).
         * Loaded from gateway.platforms.<name>.<field> YAML keys.
         * These mirror TELEGRAM_* env vars with same names (lowercase). */

        cfg->telegram_fields_loaded = false;

        snprintf(path, sizeof(path), "gateway.platforms.%s.require_mention", name);
        cfg->require_mention = yaml_get_bool(doc, path, false);
        if (!cfg->require_mention) {
            /* Fallback: TELEGRAM_REQUIRE_MENTION env var */
            const char *env_val = getenv("TELEGRAM_REQUIRE_MENTION");
            if (env_val) cfg->require_mention = (strcmp(env_val, "true") == 0 ||
                strcmp(env_val, "1") == 0 || strcasecmp(env_val, "yes") == 0);
        }

        snprintf(path, sizeof(path), "gateway.platforms.%s.exclusive_bot_mentions", name);
        cfg->exclusive_bot_mentions = yaml_get_bool(doc, path, true);
        {
            const char *env_val = getenv("TELEGRAM_EXCLUSIVE_BOT_MENTIONS");
            if (env_val) cfg->exclusive_bot_mentions = (strcmp(env_val, "true") == 0 ||
                strcmp(env_val, "1") == 0 || strcasecmp(env_val, "yes") == 0);
        }

        snprintf(path, sizeof(path), "gateway.platforms.%s.guest_mode", name);
        cfg->guest_mode = yaml_get_bool(doc, path, false);
        {
            const char *env_val = getenv("TELEGRAM_GUEST_MODE");
            if (env_val) cfg->guest_mode = (strcmp(env_val, "true") == 0 ||
                strcmp(env_val, "1") == 0 || strcasecmp(env_val, "yes") == 0);
        }

        snprintf(path, sizeof(path), "gateway.platforms.%s.observe_unmentioned_group_messages", name);
        cfg->observe_unmentioned = yaml_get_bool(doc, path, false);
        {
            const char *env_val = getenv("TELEGRAM_OBSERVE_UNMENTIONED_GROUP_MESSAGES");
            if (env_val) cfg->observe_unmentioned = (strcmp(env_val, "true") == 0 ||
                strcmp(env_val, "1") == 0 || strcasecmp(env_val, "yes") == 0);
        }

        /* String list fields (comma-separated) */
        snprintf(path, sizeof(path), "gateway.platforms.%s.allowed_chats", name);
        {
            const char *val = yaml_get_string(doc, path);
            if (!val) val = getenv("TELEGRAM_ALLOWED_CHATS");
            if (val) snprintf(cfg->allowed_chats, sizeof(cfg->allowed_chats), "%s", val);
        }

        snprintf(path, sizeof(path), "gateway.platforms.%s.group_allowed_chats", name);
        {
            const char *val = yaml_get_string(doc, path);
            if (!val) val = getenv("TELEGRAM_GROUP_ALLOWED_CHATS");
            if (val) snprintf(cfg->group_allowed_chats, sizeof(cfg->group_allowed_chats), "%s", val);
        }

        snprintf(path, sizeof(path), "gateway.platforms.%s.allowed_topics", name);
        {
            const char *val = yaml_get_string(doc, path);
            if (!val) val = getenv("TELEGRAM_ALLOWED_TOPICS");
            if (val) snprintf(cfg->allowed_topics, sizeof(cfg->allowed_topics), "%s", val);
        }

        snprintf(path, sizeof(path), "gateway.platforms.%s.ignored_threads", name);
        {
            const char *val = yaml_get_string(doc, path);
            if (!val) val = getenv("TELEGRAM_IGNORED_THREADS");
            if (val) snprintf(cfg->ignored_threads, sizeof(cfg->ignored_threads), "%s", val);
        }

        snprintf(path, sizeof(path), "gateway.platforms.%s.free_response_chats", name);
        {
            const char *val = yaml_get_string(doc, path);
            if (!val) val = getenv("TELEGRAM_FREE_RESPONSE_CHATS");
            if (val) snprintf(cfg->free_response_chats, sizeof(cfg->free_response_chats), "%s", val);
        }

        snprintf(path, sizeof(path), "gateway.platforms.%s.mention_patterns", name);
        {
            const char *val = yaml_get_string(doc, path);
            if (!val) val = getenv("TELEGRAM_MENTION_PATTERNS");
            if (val) snprintf(cfg->mention_patterns, sizeof(cfg->mention_patterns), "%s", val);
        }

        /* Compute observe_allowed_chats = intersection of allowed_chats and group_allowed_chats.
         * If group_allowed_chats is empty, use allowed_chats.
         * If both are empty, observe_allowed_chats is empty. */
        if (cfg->group_allowed_chats[0]) {
            if (cfg->allowed_chats[0]) {
                /* Use the intersection: just use group_allowed_chats for observe
                 * (the poll loop checks individual fields separately) */
                snprintf(cfg->observe_allowed_chats, sizeof(cfg->observe_allowed_chats),
                    "%s", cfg->group_allowed_chats);
            } else {
                snprintf(cfg->observe_allowed_chats, sizeof(cfg->observe_allowed_chats),
                    "%s", cfg->group_allowed_chats);
            }
        } else if (cfg->allowed_chats[0]) {
            snprintf(cfg->observe_allowed_chats, sizeof(cfg->observe_allowed_chats),
                "%s", cfg->allowed_chats);
        }

        cfg->telegram_fields_loaded = true;
        g_platform_cfg_count++;
    }

    for (size_t i = 0; i < key_count; i++) free(keys[i]);
    free(keys);
}

const hermes_platform_cfg_t *hermes_config_get_platform(const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < g_platform_cfg_count; i++) {
        if (strcmp(g_platform_cfgs[i].name, name) == 0)
            return &g_platform_cfgs[i];
    }
    return NULL;
}

const char *hermes_platform_token_env(const char *platform_name) {
    if (!platform_name) return NULL;
    if (strcmp(platform_name, "telegram") == 0) return "TELEGRAM_BOT_TOKEN";
    if (strcmp(platform_name, "discord") == 0) return "DISCORD_BOT_TOKEN";
    if (strcmp(platform_name, "slack") == 0) return "SLACK_BOT_TOKEN";
    if (strcmp(platform_name, "whatsapp") == 0) return "WHATSAPP_TOKEN";
    if (strcmp(platform_name, "matrix") == 0) return "MATRIX_ACCESS_TOKEN";
    if (strcmp(platform_name, "signal") == 0) return "SIGNAL_NUMBER";
    if (strcmp(platform_name, "mattermost") == 0) return "MATTERMOST_TOKEN";
    if (strcmp(platform_name, "email") == 0) return "EMAIL_FROM";
    return NULL;
}

/* ===========================================================================
 *  .env value parsing/quoting — ported from hermes_cli/config.py
 *  These were REAL_GAP.
 * =========================================================================== */

/*
 * PoP: _parse_env_value @ hermes_cli/config.py:_parse_env_value
 * Parse the small .env value subset Hermes writes itself. Returns malloc'd
 * string. Caller frees. */
char *parse_env_value(const char *raw_value)
{
    if (!raw_value) return strdup("");
    /* strip leading/trailing whitespace */
    while (*raw_value == ' ' || *raw_value == '\t') raw_value++;
    size_t len = strlen(raw_value);
    while (len > 0 && (raw_value[len-1]==' '||raw_value[len-1]=='\t')) len--;

    if (len >= 2 && raw_value[0] == '"' && raw_value[len-1] == '"') {
        const char *q = raw_value + 1;
        size_t ql = len - 2;
        char *out = malloc(ql + 1);
        size_t o = 0;
        for (size_t i = 0; i < ql; i++) {
            if (q[i] == '\\' && i + 1 < ql && (q[i+1]=='"' || q[i+1]=='\\')) {
                out[o++] = q[i+1]; i++;
            } else {
                out[o++] = q[i];
            }
        }
        out[o] = '\0';
        return out;
    }
    if (len >= 2 && raw_value[0] == '\047' && raw_value[len-1] == '\047') {
        char *out = malloc(len - 1);
        memcpy(out, raw_value + 1, len - 2);
        out[len-2] = '\0';
        return out;
    }
    char *out = malloc(len + 1);
    memcpy(out, raw_value, len);
    out[len] = '\0';
    return out;
}

/*
 * PoP: _quote_env_value @ hermes_cli/config.py:_quote_env_value
 * Quote .env values containing characters with special dotenv meaning.
 * Returns malloc'd string. Caller frees. */
char *quote_env_value(const char *value)
{
    if (!value) value = "";
    if (value[0] == '\0') return strdup("");
    int needs = 0;
    for (const char *p = value; *p; p++) {
        if (*p == '#' || *p == '"' || *p == '\047') { needs = 1; break; }
        if (*p == ' ' || *p == '\t') { needs = 1; break; }
    }
    if (!needs) return strdup(value);
    /* escape backslashes and double-quotes */
    size_t cap = strlen(value) * 2 + 3;
    char *out = malloc(cap);
    size_t o = 0;
    out[o++] = '"';
    for (const char *p = value; *p; p++) {
        if (*p == '\\' || *p == '"') out[o++] = '\\';
        out[o++] = *p;
    }
    out[o++] = '"';
    out[o] = '\0';
    return out;
}

/* PoP: _looks_like_structured_value @ hermes_cli/config.py:_looks_like_structured_value */
int looks_like_structured_value(const char *value)
{
    if (!value) return 0;
    static const char *markers[] = {"://", "?", "&", NULL};
    for (int i = 0; markers[i]; i++)
        if (strstr(value, markers[i])) return 1;
    for (const char *p = value; *p; p++)
        if (*p == ' ' || *p == '\t') return 1;
    return 0;
}
