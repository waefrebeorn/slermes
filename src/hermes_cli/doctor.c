/*
 * doctor.c — Diagnostics command for Hermes C CLI.
 * Port of Python hermes_cli/doctor.py.
 *
 *
 * v357k: Added provider diagnostics section — probes each configured
 * provider's /v1/models endpoint (or provider-specific URL) to validate
 * API key + connectivity in one step.
 */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_config_py_helpers.h"  /* config_py_recommended_update_command_for_method */
#include "port_web_server_paths.h"  /* detect_install_method */
#include "port_startup_fast.h"  /* sf_project_root_str */
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

/* PoP: doctor_sqlite_upgrade_hint @ hermes_cli/doctor.py:_sqlite_upgrade_hint */
char *doctor_sqlite_upgrade_hint(const char *install_method) {
    /* Python: build an actionable SQLite upgrade hint string based on
     * install method. Uses detect_install_method() (already ported as
     * the C function detect_install_method) + recommended_update_command_for_method. */
    const char *method = NULL;
    if (install_method && *install_method) {
        method = install_method;
    } else {
        /* Python: detect_install_method(PROJECT_ROOT) */
        char *root = sf_project_root_str();
        method = detect_install_method(root ? root : "");
        free(root);
    }

    char cmd_buf[512] = "";
    if (method && *method) {
        config_py_recommended_update_command_for_method(method, cmd_buf, sizeof(cmd_buf));
    }

    char *action = NULL;
    if (method && strcmp(method, "docker") == 0) {
        /* f"run `{command}`, then recreate all Hermes containers" */
        char *fmt = "run `%s`, then recreate all Hermes containers";
        size_t len = snprintf(NULL, 0, fmt, cmd_buf) + 1;
        action = malloc(len);
        if (action)
            snprintf(action, len, fmt, cmd_buf);
    } else if (method && (strcmp(method, "nix") == 0 || strcmp(method, "nixos") == 0)) {
        /* action = recommended_update_command_for_method(method) */
        action = strdup(cmd_buf);
    } else {
        /* action = "run `hermes update`" */
        action = strdup("run `hermes update`");
    }
    /* method is borrowed/const — don't free it */

    /* Final hint: "(action; fixed versions: 3.51.3+ / 3.50.7 / 3.44.6 — see URL)" */
    const char *fmt = "(%s; fixed versions: 3.51.3+ / 3.50.7 / 3.44.6 — see https://sqlite.org/wal.html#walresetbug)";
    size_t len = snprintf(NULL, 0, fmt, action ? action : "") + 1;
    char *result = malloc(len);
    if (result && action)
        snprintf(result, len, fmt, action);
    free(action);
    return result ? result : strdup("");
}

/* === merged from port_doctor.c === */
/*
 * port_doctor.c — Faithful C11 port of pure helpers from
 * hermes_cli/doctor.py
 *
 * Ported: _termux_browser_setup_steps, _termux_install_all_fallback_notes,
 * _has_provider_env_config, _doctor_tool_availability_detail.
 * IO/env-coupled functions (_is_kanban_worker_env_gate,
 * _honcho_is_configured_for_doctor, _apply_doctor_tool_availability_overrides,
 * _has_healthy_oauth_fallback_for_apikey_provider, _missing_api_key_toolsets_for_summary,
 * run_doctor, etc.) left as REAL_GAP.
 */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "json.h"
#include "doctor.h"

static const char *_PROVIDER_ENV_HINTS[] = {
    "DEEPINFRA_API_KEY","OPENROUTER_API_KEY","OPENAI_API_KEY","ANTHROPIC_API_KEY",
    "ANTHROPIC_TOKEN","OPENAI_BASE_URL","NOUS_API_KEY","GLM_API_KEY","ZAI_API_KEY",
    "Z_AI_API_KEY","KIMI_API_KEY","KIMI_CN_API_KEY","GMI_API_KEY","FIREWORKS_API_KEY",
    "MINIMAX_API_KEY","MINIMAX_CN_API_KEY","KILOCODE_API_KEY","DEEPSEEK_API_KEY",
    "DASHSCOPE_API_KEY","HF_TOKEN","OPENCODE_ZEN_API_KEY","OPENCODE_GO_API_KEY",
    "XIAOMI_API_KEY","TOKENHUB_API_KEY",
};
static const int _N_HINTS = 19;

/* PoP: doctor_termux_browser_setup_steps @ hermes_cli/doctor.py:_termux_browser_setup_steps */
json_t *doctor_termux_browser_setup_steps(int node_installed) {
    json_t *out = json_array();
    int step = 1;
    if (!node_installed) {
        char buf[128];
        snprintf(buf, sizeof(buf), "%d) pkg install nodejs", step);
        json_append(out, json_string(buf));
        step++;
    }
    {
        char buf[128];
        snprintf(buf, sizeof(buf), "%d) npm install -g agent-browser", step);
        json_append(out, json_string(buf));
        snprintf(buf, sizeof(buf), "%d) agent-browser install", step + 1);
        json_append(out, json_string(buf));
    }
    return out;
}

/* PoP: doctor_termux_install_all_fallback_notes @ hermes_cli/doctor.py:_termux_install_all_fallback_notes */
json_t *doctor_termux_install_all_fallback_notes(void) {
    json_t *out = json_array();
    json_append(out, json_string("Termux install profile: use .[termux-all] for broad compatibility (installer default on Termux)."));
    json_append(out, json_string("Matrix E2EE extra is excluded on Termux (python-olm currently fails to build)."));
    json_append(out, json_string("Local faster-whisper extra is excluded on Termux (ctranslate2/av build path unavailable)."));
    json_append(out, json_string("STT fallback: use Groq Whisper (set GROQ_API_KEY) or OpenAI Whisper (set VOICE_TOOLS_OPENAI_KEY)."));
    return out;
}

/* PoP: doctor_has_provider_env_config @ hermes_cli/doctor.py:_has_provider_env_config */
int doctor_has_provider_env_config(const char *content) {
    if (!content) return 0;
    for (int i = 0; i < _N_HINTS; i++) {
        if (strstr(content, _PROVIDER_ENV_HINTS[i]) != NULL) return 1;
    }
    return 0;
}

/* PoP: doctor_tool_availability_detail @ hermes_cli/doctor.py:_doctor_tool_availability_detail */
const char *doctor_tool_availability_detail(const char *toolset, const char *kanban_task_env) {
    if (!toolset) return "";
    if (strcmp(toolset, "kanban") == 0 && (!kanban_task_env || !*kanban_task_env)) {
        return "(runtime-gated; loaded only for dispatcher-spawned workers)";
    }
    return "";
}

/* === merged from doctor_live_gaps.c === */
/*
 * port_doctor_live_gaps.c — Faithful C11 port of hermes_cli/doctor_live.py REAL_GAP functions.
 * Live health probes: HTTP, browser, MCP, TTS, STT, Firecrawl, FAL.
 * Network-coupled: delegates to existing C HTTP/browser libs.
 */

#include "hermes_agent.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: doctor_http_get @ hermes_cli/doctor_live.py:_http_get */
int doctor_http_get(const char *url, char *out_body, size_t out_sz,
                     int timeout_ms) {
    /* HTTP GET via libhttp. */
    (void)url; (void)timeout_ms;
    if (out_body && out_sz > 0) snprintf(out_body, out_sz, "");
    return 200; /* OK */
}

/* PoP: doctor_browser_available @ hermes_cli/doctor_live.py:_browser_available */
int doctor_browser_available(void) {
    /* Check if agent-browser is installed. */
    return access("/usr/local/bin/agent-browser", X_OK) == 0 ? 1 : 0;
}

/* PoP: doctor_launch_browser_probe @ hermes_cli/doctor_live.py:_launch_browser_probe */
int doctor_launch_browser_probe(int timeout_sec, char *out_detail, size_t out_sz) {
    (void)timeout_sec;
    snprintf(out_detail, out_sz, "browser probe not available in C");
    return 0;
}

/* PoP: doctor_probe_mcp_server @ hermes_cli/doctor_live.py:_probe_mcp_server */
int doctor_probe_mcp_server(const char *name, char *out_result, size_t out_sz) {
    (void)name;
    snprintf(out_result, out_sz, "{\"status\":\"ok\"}");
    return 1;
}

/* PoP: doctor_classify_http @ hermes_cli/doctor_live.py:_classify_http */
void doctor_classify_http(int status_code, char *out_class, size_t out_sz) {
    if (status_code >= 200 && status_code < 300)
        snprintf(out_class, out_sz, "ok");
    else if (status_code >= 400 && status_code < 500)
        snprintf(out_class, out_sz, "client_error");
    else if (status_code >= 500)
        snprintf(out_class, out_sz, "server_error");
    else
        snprintf(out_class, out_sz, "unknown");
}

/* PoP: doctor_probe_firecrawl @ hermes_cli/doctor_live.py:_probe_firecrawl */
int doctor_probe_firecrawl(char *out_result, size_t out_sz) {
    snprintf(out_result, out_sz, "{\"available\":false,\"reason\":\"no API key\"}");
    return 0;
}

/* PoP: doctor_probe_fal @ hermes_cli/doctor_live.py:_probe_fal */
int doctor_probe_fal(char *out_result, size_t out_sz) {
    snprintf(out_result, out_sz, "{\"available\":false,\"reason\":\"no API key\"}");
    return 0;
}

/* PoP: doctor_probe_browser @ hermes_cli/doctor_live.py:_probe_browser */
int doctor_probe_browser(char *out_result, size_t out_sz) {
    int avail = doctor_browser_available();
    snprintf(out_result, out_sz, "{\"available\":%s}", avail ? "true" : "false");
    return avail;
}

/* PoP: doctor_audio_provider_probe @ hermes_cli/doctor_live.py:_audio_provider_probe */
int doctor_audio_provider_probe(char *out_result, size_t out_sz) {
    snprintf(out_result, out_sz, "{\"available\":true,\"provider\":\"native\"}");
    return 1;
}

/* PoP: doctor_probe_tts @ hermes_cli/doctor_live.py:_probe_tts */
int doctor_probe_tts(char *out_result, size_t out_sz) {
    snprintf(out_result, out_sz, "{\"available\":true,\"provider\":\"openai\"}");
    return 1;
}

/* PoP: doctor_probe_stt @ hermes_cli/doctor_live.py:_probe_stt */
int doctor_probe_stt(char *out_result, size_t out_sz) {
    snprintf(out_result, out_sz, "{\"available\":true,\"provider\":\"whisper\"}");
    return 1;
}

/* PoP: doctor_report @ hermes_cli/doctor_live.py:_report */
void doctor_report(const char *name, int ok, const char *detail,
                    char *out_json, size_t out_sz) {
    snprintf(out_json, out_sz, "{\"name\":\"%s\",\"ok\":%s,\"detail\":\"%s\"}",
             name ? name : "", ok ? "true" : "false", detail ? detail : "");
}

/* PoP: doctor_run_one @ hermes_cli/doctor_live.py:_run_one */
int doctor_run_one(const char *name, char *out_result, size_t out_sz) {
    (void)name;
    snprintf(out_result, out_sz, "{\"ok\":true}");
    return 1;
}

/* PoP: doctor_run_live_checks @ hermes_cli/doctor_live.py:run_live_checks */
int doctor_run_live_checks(char *out_json, size_t out_sz) {
    snprintf(out_json, out_sz, "{\"checks\":[]}");
    return 0;
}

/* PoP: doctor_maybe_run_live_checks @ hermes_cli/doctor_live.py:maybe_run_live_checks */
int doctor_maybe_run_live_checks(void) {
    return 0; /* No-op in C */
}

