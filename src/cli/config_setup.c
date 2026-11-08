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

#include "hermes_http.h"
#include "hermes_core_types.h"
#include "config_setup_wizard.h"
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

static void config_sighup_handler(int sig);

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

/* Port of Python hermes_cli/setup.py:print_header(). */

/* Prompt for a string with optional default */
/* Port of Python hermes_cli/setup.py:prompt(). */

/* Prompt for yes/no. Returns true for yes, false for no. */
/* Port of Python hermes_cli/setup.py:prompt_yes_no(). */

/* Prompt for a choice from a list. Returns index or -1 on cancel/error. */
/* PoP: prompt_choice @ hermes_cli/moa_cmd.py:_prompt_choice */
/* PoP: prompt_choice @ hermes_cli/tools_config.py:_prompt_choice */
/* Port of Python hermes_cli/setup.py:prompt_choice(). */

/* Port of Python hermes_cli/setup.py:setup_agent_settings().
 * Configure agent behavior: max iterations, tool progress, compression. */

/* Port of Python hermes_cli/setup.py:setup_terminal_backend().
 * Configure where shell commands run. */

/* Port of Python hermes_cli/setup.py:_print_setup_summary().
 * Print a summary of what was configured. */

/* Port of Python hermes_cli/setup.py:is_interactive_stdin(). */

/* Port of Python hermes_cli/setup.py:print_noninteractive_setup_guidance(). */

/* Port of Python hermes_cli/setup.py:_sanitize_pasted_input().
 * Strips terminal bracketed-paste control markers and leading/trailing whitespace.
 * Python: re.compile(r"\x1b\[\s*200~|\x1b\[\s*201~") */

/* Port of Python hermes_cli/setup.py:prompt_checklist() — multi-select checklist.
 * Uses curses_widget cw_checklist() with numbered fallback. */

/* ── Masked input for API keys (port of Python masked_secret_prompt) ── */

/* Read a line from stdin without echoing (uses termios to disable echo).
 * Returns the input in out (null-terminated). Falls back to plain fgets. */

/* ── Formatted API key prompt (port of Python _prompt_api_key) ── */

/* Provider info struct for formatted prompts */

/* Show a formatted API key prompt with description and URL.
 * Reads the key into out, writing '*' for each character.
 * Port of Python hermes_cli/setup.py:_prompt_api_key(). */

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
 * The C side carries the model as a plain string (the Python struct wraps a
 * dict), so we faithfully reproduce the dict form the Python returns:
 *   - non-empty model string  -> {"default": "<model>"}
 *   - empty/missing model     -> {}
 * `out_default` receives the compact JSON form (mirrors json.dumps with
 * separators=(",",":")). */
void setup_model_config_dict(const hermes_config_t *cfg,
                                     char *out_default, size_t out_size) {
    if (!out_default || out_size == 0) return;
    if (!cfg || !cfg->model[0]) {
        out_default[0] = '\0';
        strncpy(out_default, "{}", out_size - 1);
        out_default[out_size - 1] = '\0';
        return;
    }
    int need = snprintf(NULL, 0, "{\"default\":\"%s\"}", cfg->model);
    if (need < 0 || (size_t)need >= out_size) {
        out_default[0] = '\0';
        return;
    }
    snprintf(out_default, out_size, "{\"default\":\"%s\"}", cfg->model);
}

/* Port of Python hermes_cli/setup.py:_current_reasoning_effort().
 * Read reasoning_effort from config struct. Currently not stored in cfg,
 * always returns empty string (caller uses default). */
const char *setup_current_reasoning_effort(void) {
    return ""; /* hermes_config_t does not have reasoning_effort field yet */
}

/* Port of Python hermes_cli/setup.py:_supports_same_provider_pool_setup().
 * Returns true for providers that support multi-key rotation (a "same
 * provider pool"), i.e. those whose PROVIDER_REGISTRY entry has auth_type
 * in {api_key, oauth_device_code}. "custom" -> never; "openrouter" -> always.
 * Providers absent from the registry are not supported.
 *
 * The lookup is delegated to the self-contained C provider-auth registry
 * (lib/libproviderauth) — no hardcoded snapshot here. The oracle
 * (tests/sta_oracle_provider_pool_setup.py) diffs the C registry table
 * against the LIVE Python PROVIDER_REGISTRY, so any drift is caught.
 *
 * Forward-declared to avoid pulling the registry module's header chain. */
bool provider_auth_supports_pool(const char *provider);

bool config_setup_supports_same_provider_pool_setup(const char *provider) {
    return provider_auth_supports_pool(provider);
}

/* Port of Python hermes_cli/setup.py:_gateway_platform_short_label().
 * Strip trailing parenthetical qualifiers from a gateway platform label:
 * base = label.split("(", 1)[0].strip(); return base or label.
 * Returns a malloc'd string the caller must free (was a static buffer,
 * which was both thread-unsafe and wrong: for a label starting with "("
 * it returned "" instead of the original label). */
char *setup_gateway_platform_short_label(const char *label) {
    if (!label) return strdup("");
    const char *paren = strchr(label, '(');
    if (!paren) return strdup(label);
    /* base = label[:paren].strip()  (leading + trailing whitespace) */
    size_t start = 0;
    while (label[start] == ' ' || label[start] == '\t') start++;
    size_t end = (size_t)(paren - label);
    while (end > start && (label[end - 1] == ' ' || label[end - 1] == '\t')) end--;
    if (end <= start) {
        /* base was empty after strip -> Python returns the original label */
        return strdup(label);
    }
    size_t len = end - start;
    char *buf = (char *)malloc(len + 1);
    if (!buf) return strdup(label);
    memcpy(buf, label + start, len);
    buf[len] = '\0';
    return buf;
}

/* Port of Python hermes_cli/setup.py:_model_section_has_credentials().
 * Return True when the named provider has usable credentials (its key env
 * var is set). The provider is passed explicitly; cfg is unused by the
 * env-key lookup (kept for API symmetry with the Python signature, which
 * reads the provider from config["model"]["provider"]). */
bool setup_model_section_has_credentials(const hermes_config_t *cfg, const char *provider) {
    (void)cfg; /* env-key lookup only needs `provider` */
    if (!provider) return false;
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
bool setup_check_espeak_ng(void) {
    FILE *fp = popen("which espeak-ng 2>/dev/null || which espeak 2>/dev/null", "r");
    if (!fp) return false;
    char buf[256];
    bool found = (fgets(buf, sizeof(buf), fp) != NULL);
    pclose(fp);
    return found;
}

/* Port of Python hermes_cli/setup.py:_xai_oauth_logged_in_for_setup().
 * Check if xAI OAuth token exists — probes XAI_API_KEY env. */
bool setup_xai_oauth_logged_in(void) {
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

/* AG26: Port of Python hermes_cli/providers.py:setup_tts(). */

/* Port of Python hermes_cli/setup.py:_setup_telegram().
 * Configure Telegram bot credentials and allowlist. */

/* Port of Python hermes_cli/setup.py:_setup_slack().
 * Configure Slack app credentials and guide user through manifest setup. */

/* Port of Python hermes_cli/setup.py:_setup_matrix().
 * Configure Matrix credentials. */

/* Port of Python hermes_cli/setup.py:_setup_webhooks().
 * Configure webhook URL for generic HTTP integration. */

/* Port of Python hermes_cli/setup.py:_setup_bluebubbles().
 * Configure BlueBubbles iMessage gateway. */

/* Port of Python hermes_cli/setup.py:_setup_qqbot().
 * Configure QQ Bot credentials. */

/* Port of Python hermes_cli/setup.py:setup_gateway().
 * Configure messaging platform integrations — multi-select from available platforms.
 * Sets cfg->gateway_platforms from selected platforms. */

/* ── Provider endpoint configuration helper ────────────── */

/* Set base_url and api_mode from provider_metadata for known providers.
 * For "custom" providers the caller must set base_url separately. */

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

/* Universal provider model fetcher — fetch model IDs from any provider's
 * /v1/models endpoint. Returns malloc'd array of model ID strings
 * (caller must free each + the array). Sets *out_count to number fetched.
 * Returns NULL on failure (API unreachable, no models).
 /* ── Provider model fetching ────────────────────────────── */

 /* Per-provider curated fallback model lists (port of Python's _DEFAULT_PROVIDER_MODELS).
  * Used when the live /v1/models API is unreachable. NULL-terminated. */

 /* Map provider index to its fallback list */

/* ── OpenRouter curated model list (port of Python OPENROUTER_MODELS) ── */
/* Curated preferences used to filter live API results. */

/* Port of Python hermes_cli/models.py:_openrouter_model_supports_tools().
 * Returns true when the model item either has no supported_parameters list
 * or the list explicitly includes "tools". Permissive when field is absent. */

/* Port of Python hermes_cli/models.py:fetch_openrouter_models().
 * Fetches live OpenRouter catalog, filters by curated preference list +
 * tool-calling support. Returns models in curated list order with
 * descriptions where applicable. Always returns at least the raw curated
 * list (unfiltered) on failure. */

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

/* Universal paginated model picker — shows fetched models with n/p navigation.
 * Handles selection and writes to out_model. Falls back to prompt on empty list. */

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

/* PoP: get_platform_token @ hermes_cli/config.py:get_platform_token */
/* Token for a configured messaging platform, or "" when unconfigured.
 *
 * Reads the per-platform struct on hermes_config_t. Platforms without a
 * token field in the config (whatsapp) resolve through env only, so this
 * returns "". Never returns NULL for a non-NULL args pair, so callers can
 * compare directly. */
const char *get_platform_token(const hermes_config_t *cfg, const char *platform)
{
    if (!cfg || !platform) return "";

    if (strcmp(platform, "telegram") == 0)   return cfg->telegram.bot_token;
    if (strcmp(platform, "discord") == 0)    return cfg->discord.token;
    if (strcmp(platform, "slack") == 0)      return cfg->slack.token;
    if (strcmp(platform, "matrix") == 0)     return cfg->matrix.access_token;
    if (strcmp(platform, "mattermost") == 0) return cfg->mattermost.token;
    if (strcmp(platform, "signal") == 0)     return cfg->signal_number;
    return "";
}

void config_sighup_handler(int sig) {
    (void)sig;
    g_config_reload_requested = 1;
}
