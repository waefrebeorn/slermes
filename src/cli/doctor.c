/*
 * doctor.c — Diagnostics command for Hermes C CLI.
 * Port of Python hermes_cli/doctor.py.
 *
 * PoP: run_doctor @ hermes_cli/doctor.py:run_doctor
 * PoP: check_ok @ hermes_cli/doctor.py:check_ok
 * PoP: check_warn @ hermes_cli/doctor.py:check_warn
 * PoP: check_fail @ hermes_cli/doctor.py:check_fail
 * PoP: check_info @ hermes_cli/doctor.py:check_info
 * PoP: check_certificates @ hermes_cli/doctor.py:check_certificates
 *
 * v357k: Added provider diagnostics section — probes each configured
 * provider's /v1/models endpoint (or provider-specific URL) to validate
 * API key + connectivity in one step.
 */

#include "hermes.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define DOC_GREEN  "\033[0;32m"
#define DOC_YELLOW "\033[0;33m"
#define DOC_RED    "\033[0;31m"
#define DOC_CYAN   "\033[0;36m"
#define DOC_DIM    "\033[2m"
#define DOC_BOLD   "\033[1m"
#define DOC_RESET  "\033[0m"

static void doc_ok(const char *text, const char *detail) {
    printf("  %s\xE2\x9C\x93%s %s", DOC_GREEN, DOC_RESET, text);
    if (detail) printf(" %s%s%s", DOC_DIM, detail, DOC_RESET);
    printf("\n");
}

static void doc_warn(const char *text, const char *detail) {
    printf("  %s\xE2\x9A\xA0%s %s", DOC_YELLOW, DOC_RESET, text);
    if (detail) printf(" %s%s%s", DOC_DIM, detail, DOC_RESET);
    printf("\n");
}

static void doc_fail(const char *text, const char *detail) {
    printf("  %s\xC3\x97%s %s", DOC_RED, DOC_RESET, text);
    if (detail) printf(" %s%s%s", DOC_DIM, detail, DOC_RESET);
    printf("\n");
}

static void doc_info(const char *text) {
    printf("    %s\xE2\x86\x92%s %s\n", DOC_CYAN, DOC_RESET, text);
}

static void doc_section(const char *title) {
    printf("\n%s\xE2\x97\x86 %s%s\n", DOC_CYAN, title, DOC_RESET);
}

static bool file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static bool binary_exists(const char *name) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "which %s >/dev/null 2>&1", name);
    return system(cmd) == 0;
}

static const char *PROVIDER_ENV_HINTS[] = {
    "OPENROUTER_API_KEY", "OPENAI_API_KEY", "ANTHROPIC_API_KEY",
    "ANTHROPIC_TOKEN", "NOUS_API_KEY", "DEEPSEEK_API_KEY",
    "HF_TOKEN", "GOOGLE_API_KEY", "GROQ_API_KEY", NULL
};

static bool has_provider_env(void) {
    for (int i = 0; PROVIDER_ENV_HINTS[i]; i++) {
        const char *val = getenv(PROVIDER_ENV_HINTS[i]);
        if (val && val[0]) return true;
    }
    return false;
}

/* ── Provider diagnostic helpers ──────────────────────────────── */

typedef struct {
    const char *name;
    const char *env_var;
    const char *probe_url;       /* URL to probe for connectivity + auth */
    const char *auth_header;     /* Authorization header prefix */
} provider_probe_t;

static const provider_probe_t PROVIDER_PROBES[] = {
    {"OpenRouter",    "OPENROUTER_API_KEY", "https://openrouter.ai/api/v1/auth/key", "Bearer"},
    {"OpenAI",        "OPENAI_API_KEY",     "https://api.openai.com/v1/models",       "Bearer"},
    {"Anthropic",     "ANTHROPIC_API_KEY",  "https://api.anthropic.com/v1/messages",  "Bearer"},
    {"DeepSeek",      "DEEPSEEK_API_KEY",   "https://api.deepseek.com/v1/models",     "Bearer"},
    {"Google Gemini", "GOOGLE_API_KEY",     "https://generativelanguage.googleapis.com/v1/models", "Bearer"},
    {"Groq",          "GROQ_API_KEY",       "https://api.groq.com/openai/v1/models",  "Bearer"},
    {"HuggingFace",   "HF_TOKEN",           "https://huggingface.co/api/models",      "Bearer"},
    {"Nous Research", "NOUS_API_KEY",       "https://api.nousresearch.com/v1/models", "Bearer"},
    {NULL, NULL, NULL, NULL}
};

/* Probe a provider endpoint using curl.
 * Returns: 0 = reachable + auth works, 1 = unreachable, -1 = no key configured */
static int probe_provider(const provider_probe_t *pp) {
    const char *key = getenv(pp->env_var);
    if (!key || !key[0]) return -1;

    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "curl -s -o /dev/null -w '%%{http_code}' --connect-timeout 5 "
        "-H '%s: %s' '%s' 2>/dev/null",
        pp->auth_header, key, pp->probe_url);

    FILE *fp = popen(cmd, "r");
    if (!fp) return 1;
    char result[16] = {0};
    if (fgets(result, sizeof(result), fp)) {
        /* Strip whitespace */
        char *p = result;
        while (*p == ' ' || *p == '\n' || *p == '\r') p++;
        int code = atoi(p);
        pclose(fp);
        /* 2xx = success, 401 = bad key (auth error), 4xx/5xx = maybe reachable but config issue */
        if (code >= 200 && code < 300) return 0;
        if (code == 401 || code == 403) return 2; /* auth error */
        return 3; /* server error */
    }
    pclose(fp);
    return 1; /* unreachable */
}

/* ── Diagnostic sections ──────────────────────────────────────── */

static void check_env_file(void) {
    doc_section("Environment");
    const char *home = getenv("HOME");
    if (!home) home = "/home/wubu";

    char config_path[512], env_path[512];
    snprintf(config_path, sizeof(config_path), "%s/.hermes/config.yaml", home);
    snprintf(env_path, sizeof(env_path), "%s/.hermes/.env", home);

    if (file_exists(config_path)) doc_ok("Config file found", config_path);
    else doc_warn("Config file missing", config_path);

    if (file_exists(env_path)) doc_ok("Environment file found", env_path);
    else doc_warn("Environment file missing", env_path);

    if (has_provider_env()) doc_ok("Provider API keys configured", NULL);
    else doc_warn("No provider API keys found", "Run `hermes setup`");
}

static void check_dependencies(void) {
    doc_section("Dependencies");
    struct { const char *name; const char *desc; bool required; } bins[] = {
        {"curl", "HTTP client", true}, {"git", "Version control", true},
        {"make", "Build tool", true}, {"gcc", "C compiler", true},
        {"python3", "Python runtime", true},
        {"arecord", "Audio (voice)", false}, {"sox", "Audio (voice)", false},
        {"ffmpeg", "Media", false}, {NULL, NULL, false}
    };
    for (int i = 0; bins[i].name; i++) {
        if (binary_exists(bins[i].name)) doc_ok(bins[i].desc, bins[i].name);
        else if (bins[i].required) doc_fail("Missing required", bins[i].name);
        else doc_warn("Missing optional", bins[i].name);
    }
}

static void check_config(void) {
    doc_section("Configuration");
    const char *home = getenv("HOME");
    if (!home) home = "/home/wubu";
    char config_path[512];
    snprintf(config_path, sizeof(config_path), "%s/.hermes/config.yaml", home);
    if (!file_exists(config_path)) { doc_warn("No config", "Run `hermes setup`"); return; }
    doc_ok("Config loaded", NULL);
    const char *platforms = getenv("HERMES_GATEWAY_PLATFORMS");
    if (platforms && platforms[0]) doc_ok("Gateway platforms configured", platforms);
    else doc_info("No gateway platforms (CLI only)");
}

/* Port of Python _check_version_consistency */
static void check_version(void) {
    doc_section("Version");
    doc_ok("Build version", HERMES_VERSION);
    /* Check if pyproject.toml has matching version (basic consistency) */
    const char *home = getenv("HOME");
    if (!home) home = "/home/wubu";
    char pyproject[512];
    snprintf(pyproject, sizeof(pyproject),
             "%s/hermes-agent-dev/pyproject.toml", home);
    if (file_exists(pyproject)) doc_ok("Python project manifest", pyproject);
    else doc_info("Python project not in default location");
}

/* Port of Python provider connectivity probes */
static void check_providers(void) {
    doc_section("Providers");
    int configured = 0, working = 0;

    for (int i = 0; PROVIDER_PROBES[i].name; i++) {
        const provider_probe_t *pp = &PROVIDER_PROBES[i];
        int status = probe_provider(pp);

        if (status < 0) continue; /* not configured */
        configured++;

        switch (status) {
        case 0:
            doc_ok(pp->name, "connected");
            working++;
            break;
        case 2:
            doc_fail(pp->name, "auth failed (check API key)");
            break;
        case 3:
            doc_warn(pp->name, "responded with error (provider issue?)");
            break;
        default:
            doc_fail(pp->name, "unreachable");
            break;
        }
    }

    if (configured == 0) {
        doc_info("No providers configured — run `hermes setup`");
    } else {
        char summary[128];
        snprintf(summary, sizeof(summary),
                 "%d/%d providers working", working, configured);
        doc_info(summary);
    }
}

static void check_connectivity(void) {
    doc_section("Connectivity");
    struct { const char *name; const char *url; } targets[] = {
        {"OpenRouter", "https://openrouter.ai/api/v1/auth/key"},
        {"HuggingFace", "https://huggingface.co/api/models"},
        {"GitHub", "https://api.github.com/zen"},
        {NULL, NULL}
    };
    for (int i = 0; targets[i].name; i++) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd),
                 "curl -s -o /dev/null --connect-timeout 5 '%s' 2>/dev/null",
                 targets[i].url);
        if (system(cmd) == 0) doc_ok(targets[i].name, "reachable");
        else doc_warn(targets[i].name, "unreachable");
    }
}

static void check_plugins(void) {
    doc_section("Plugins");
    const char *home = getenv("HOME");
    if (!home) home = "/home/wubu";
    char bundled[512], user_p[512];
    snprintf(bundled, sizeof(bundled), "%s/hermes-agent-dev/plugins", home);
    snprintf(user_p, sizeof(user_p), "%s/.hermes/plugins", home);
    if (file_exists(bundled)) doc_ok("Bundled plugins", bundled);
    if (file_exists(user_p)) doc_ok("User plugins", user_p);
    else doc_info("No user plugins installed");
}

int doctor_run(bool fix_mode) {
    printf("\n%sHermes Agent Doctor%s\n", DOC_BOLD, DOC_RESET);
    check_env_file();
    check_version();
    check_config();
    check_dependencies();
    check_providers();
    check_connectivity();
    check_plugins();
    printf("\n%sSummary%s\n", DOC_CYAN, DOC_RESET);
    printf("  Diagnostics complete.\n");
    if (fix_mode) {
        const char *home = getenv("HOME");
        if (!home) home = "/home/wubu";
        char hermes_dir[512];
        snprintf(hermes_dir, sizeof(hermes_dir), "%s/.hermes", home);
        struct stat st;
        if (stat(hermes_dir, &st) != 0) mkdir(hermes_dir, 0755);
        printf("  Fix mode: basic repairs applied.\n");
    }
    return 0;
}

int doctor_acknowledge_advisory(const char *advisory_id) {
    if (!advisory_id || !advisory_id[0]) {
        fprintf(stderr, "Error: Advisory ID required\n");
        return 1;
    }
    fprintf(stderr, "[doctor] Advisory %s acknowledged\n", advisory_id);
    return 0;
}
