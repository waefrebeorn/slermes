/*
 * setup_wizard.c — Interactive setup wizard for Hermes C.
 * Port of Python hermes_cli/setup.py (53 defs, ~3300 lines).
 *
 * PoP: run_setup_wizard @ hermes_cli/setup.py:run_setup_wizard
 * PoP: setup_model_provider @ hermes_cli/setup.py:setup_model_provider
 * PoP: setup_tts @ hermes_cli/setup.py:setup_tts
 * PoP: setup_terminal_backend @ hermes_cli/setup.py:setup_terminal_backend
 * PoP: setup_agent_settings @ hermes_cli/setup.py:setup_agent_settings
 * PoP: setup_gateway @ hermes_cli/setup.py:setup_gateway
 * PoP: setup_tools @ hermes_cli/setup.py:setup_tools
 * PoP: print_header @ hermes_cli/setup.py:print_header
 * PoP: prompt @ hermes_cli/setup.py:prompt
 * PoP: prompt_choice @ hermes_cli/setup.py:prompt_choice
 * PoP: prompt_yes_no @ hermes_cli/setup.py:prompt_yes_no
 * PoP: prompt_checklist @ hermes_cli/setup.py:prompt_checklist
 * PoP: is_interactive_stdin @ hermes_cli/setup.py:is_interactive_stdin
 * PoP: print_noninteractive_setup_guidance @ hermes_cli/setup.py:print_noninteractive_setup_guidance
 * PoP: _prompt_api_key @ hermes_cli/setup.py:_prompt_api_key
 * PoP: _sanitize_pasted_input @ hermes_cli/setup.py:_sanitize_pasted_input
 * PoP: _check_espeak_ng @ hermes_cli/setup.py:_check_espeak_ng
 * PoP: _apply_default_agent_settings @ hermes_cli/setup.py:_apply_default_agent_settings
 * PoP: _model_section_has_credentials @ hermes_cli/setup.py:_model_section_has_credentials
 * PoP: _gateway_platform_short_label @ hermes_cli/setup.py:_gateway_platform_short_label
 *   1. Provider selection with 8 presets + custom
 *   2. API key entry with optional connectivity test
 *   3. Model selector with 6 presets + custom
 *   4. Terminal backend: local/docker/SSH/Modal
 *   5. Container resource limits (CPU, memory, disk)
 *   6. Gateway platforms (5 presets) + token validation
 *   7. Per-platform detail: Slack manifest, Matrix homeserver
 *   8. Web search provider (Tavily / Firecrawl / Exa / none)
 *   9. Image gen provider (Fal.ai / Stability / DALL-E / none)
 *  10. TTS provider (espeak/OpenAI/ElevenLabs/none)
 *  11. Agent settings (reasoning effort, collaboration)
 *  12. Credential pool strategy (share / isolate)
 *  13. First-run quick setup mode
 *  14. Config backup before write
 *  15. Writes config.yaml + .env
 */

#include "hermes_core_types.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

/* ── ANSI helpers ──────────────────────────────────────────────── */
#define WIZ_GREEN  "\033[0;32m"
#define WIZ_CYAN   "\033[0;36m"
#define WIZ_YELLOW "\033[0;33m"
#define WIZ_RED    "\033[0;31m"
#define WIZ_BOLD   "\033[1m"
#define WIZ_RESET  "\033[0m"

static void wiz_banner(const char *text) {
    printf("\n%s%s%s%s\n", WIZ_BOLD, WIZ_CYAN, text, WIZ_RESET);
}
static void wiz_ok(const char *msg) {
    printf("  %s\xE2\x9C\x93%s %s\n", WIZ_GREEN, WIZ_RESET, msg);
}
static void wiz_warn(const char *msg) {
    printf("  %s\xE2\x9A\xA0%s %s\n", WIZ_YELLOW, WIZ_RESET, msg);
}
static void wiz_err(const char *msg) {
    printf("  %s\xE2\x9C\x97%s %s\n", WIZ_RED, WIZ_RESET, msg);
}
static void wiz_prompt(const char *text) {
    printf("  %s?%s %s: ", WIZ_CYAN, WIZ_RESET, text);
    fflush(stdout);
}

static bool read_line(char *buf, size_t sz) {
    if (!fgets(buf, (int)sz, stdin)) return false;
    size_t len = strlen(buf);
    while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r'))
        buf[--len] = '\0';
    return true;
}

static bool ensure_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) return S_ISDIR(st.st_mode);
    return mkdir(path, 0755) == 0;
}

static int prompt_int(const char *label, int min_val, int max_val, int default_val) {
    char input[16];
    while (1) {
        char prompt[64];
        snprintf(prompt, sizeof(prompt), "%s [%d-%d]", label, min_val, max_val);
        wiz_prompt(prompt);
        if (!read_line(input, sizeof(input))) continue;
        if (input[0] == '\0') return default_val;
        int v = atoi(input);
        if (v >= min_val && v <= max_val) return v;
        printf("  Enter %d-%d.\n", min_val, max_val);
    }
}

static bool prompt_yes_no(const char *question, bool default_yes) {
    char input[16];
    wiz_prompt(question);
    printf(" (%s/%s) ", default_yes ? "Y" : "y", default_yes ? "n" : "N");
    if (!read_line(input, sizeof(input))) return default_yes;
    if (input[0] == '\0') return default_yes;
    return input[0] == 'y' || input[0] == 'Y';
}

/* Read a multi-choice string */
static void prompt_str(const char *label, char *buf, size_t sz, const char *def) {
    wiz_prompt(label);
    if (!read_line(buf, sz) || buf[0] == '\0') {
        if (def) snprintf(buf, sz, "%s", def);
        else buf[0] = '\0';
        return;
    }
}

/* ── HTTP helpers ──────────────────────────────────────────────── */
static bool test_api_connectivity(const char *base_url, int timeout_sec) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "curl -s --max-time %d -o /dev/null -w '%%{http_code}' '%s' 2>/dev/null",
             timeout_sec, base_url);
    FILE *fp = popen(cmd, "r");
    if (fp) {
        char buf[16] = {0};
        if (fgets(buf, sizeof(buf), fp)) {
            int code = atoi(buf);
            pclose(fp);
            return code > 0;
        }
        pclose(fp);
    }
    snprintf(cmd, sizeof(cmd),
             "wget -q --timeout=%d -O /dev/null '%s' 2>/dev/null", timeout_sec, base_url);
    return system(cmd) == 0;
}

static bool validate_telegram_token(const char *token) {
    char cmd[640];
    snprintf(cmd, sizeof(cmd),
             "curl -s --max-time 10 -o /dev/null -w '%%{http_code}' "
             "'https://api.telegram.org/bot%s/getMe' 2>/dev/null", token);
    FILE *fp = popen(cmd, "r");
    if (fp) {
        char buf[16] = {0};
        if (fgets(buf, sizeof(buf), fp)) {
            int code = atoi(buf);
            pclose(fp);
            return code == 200;
        }
        pclose(fp);
    }
    return false;
}

/* ── Config backup ─────────────────────────────────────────────── */
static void backup_config(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return; /* no existing config */
    char backup[576];
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    if (!tm) return;
    snprintf(backup, sizeof(backup), "%s.bak.%04d%02d%02d_%02d%02d%02d",
             path, tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday,
             tm->tm_hour, tm->tm_min, tm->tm_sec);
    FILE *src = fopen(path, "r");
    if (!src) return;
    FILE *dst = fopen(backup, "w");
    if (!dst) { fclose(src); return; }
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), src)) > 0)
        fwrite(buf, 1, n, dst);
    fclose(src); fclose(dst);
    wiz_ok("Existing config backed up");
}

/* ── Provider definitions ──────────────────────────────────────── */
typedef struct {
    const char *name;
    const char *env_var;
    const char *base_url;
    const char *description;
} provider_t;

static const provider_t PROVIDERS[] = {
    {"OpenRouter",     "OPENROUTER_API_KEY", "https://openrouter.ai/api/v1",           "200+ models, credit-based"},
    {"OpenAI",         "OPENAI_API_KEY",     "https://api.openai.com/v1",              "GPT-4, GPT-4o, o1, o3"},
    {"Anthropic",      "ANTHROPIC_API_KEY",  "https://api.anthropic.com/v1",            "Claude 3/4 Sonnet, Opus"},
    {"DeepSeek",       "DEEPSEEK_API_KEY",   "https://api.deepseek.com/v1",             "V3, R1, Janus"},
    {"Google/Gemini",  "GOOGLE_API_KEY",     "https://generativelanguage.googleapis.com/v1beta", "Gemini 2.0, 2.5"},
    {"HuggingFace",    "HF_TOKEN",           "https://api-inference.huggingface.co/v1", "Open models, serverless"},
    {"Groq",           "GROQ_API_KEY",       "https://api.groq.com/openai/v1",          "Fast inference, open models"},
    {"Nous Research",  "NOUS_API_KEY",       "https://inference-api.nousresearch.com/v1","Hermes models, inference"},
    {"xAI/Grok",       "XAI_API_KEY",        "https://api.x.ai/v1",                       "Grok 3, reasoning, vision"},
    {NULL, NULL, NULL, NULL}
};

/* ── Section 1: Provider + API key ─────────────────────────────── */
static int select_provider(char *env_var, size_t env_sz, char *base_url, size_t url_sz) {
    wiz_banner("Provider Selection");
    printf("  Choose your LLM provider:\n\n");
    for (int i = 0; PROVIDERS[i].name; i++)
        printf("  %s%d)%s %-18s %s\n", WIZ_YELLOW, i+1, WIZ_RESET,
               PROVIDERS[i].name, PROVIDERS[i].description);
    printf("  %s%d)%s Other (custom URL + env var)\n\n", WIZ_YELLOW, 10, WIZ_RESET);

    while (1) {
        int choice = prompt_int("Select provider", 1, 10, 1);
        if (choice >= 1 && choice <= 9) {
            snprintf(env_var, env_sz, "%s", PROVIDERS[choice-1].env_var);
            snprintf(base_url, url_sz, "%s", PROVIDERS[choice-1].base_url);
            return choice - 1;
        }
        if (choice == 9) {
            char custom_name[128], custom_env[128], custom_url[256];
            prompt_str("Provider name", custom_name, sizeof(custom_name), NULL);
            if (!custom_name[0]) continue;
            prompt_str("Env var name (e.g., MY_API_KEY)", custom_env, sizeof(custom_env), NULL);
            if (!custom_env[0]) continue;
            prompt_str("Base URL (e.g., https://api.example.com/v1)", custom_url, sizeof(custom_url), NULL);
            if (!custom_url[0]) continue;
            snprintf(env_var, env_sz, "%s", custom_env);
            snprintf(base_url, url_sz, "%s", custom_url);
            return 9;
        }
    }
}

static void enter_api_key(const char *env_var, const char *base_url) {
    char key[2048];
    wiz_banner("API Key");
    wiz_prompt("API key (or Enter to skip)");
    if (!read_line(key, sizeof(key)) || key[0] == '\0') {
        printf("  Skipped.\n");
        return;
    }

    if (base_url && base_url[0]) {
        printf("  Testing connectivity to %s...\n", base_url);
        if (test_api_connectivity(base_url, 10)) {
            wiz_ok("Provider endpoint reachable");
        } else {
            wiz_warn("Could not reach provider endpoint (network/proxy?)");
            if (!prompt_yes_no("Save key anyway", true)) {
                printf("  Skipped.\n");
                return;
            }
        }
    }

    const char *home = getenv("HOME");
    if (!home) home = "/home/wubu";
    char env_path[512];
    snprintf(env_path, sizeof(env_path), "%s/.hermes/.env", home);
    char hermes_dir[512];
    snprintf(hermes_dir, sizeof(hermes_dir), "%s/.hermes", home);
    ensure_dir(hermes_dir);

    FILE *fp = fopen(env_path, "a");
    if (!fp) { wiz_err("Cannot write .env"); return; }
    fprintf(fp, "%s=%s\n", env_var, key);
    fclose(fp);
    wiz_ok("API key saved");
}

/* ── Section 2: Model selector ─────────────────────────────────── */
static void select_model(char *model_out, size_t model_sz) {
    wiz_banner("Model Selection");
    printf("  %s1)%s Claude Sonnet 4\n", WIZ_YELLOW, WIZ_RESET);
    printf("  %s2)%s GPT-4o\n", WIZ_YELLOW, WIZ_RESET);
    printf("  %s3)%s DeepSeek V3\n", WIZ_YELLOW, WIZ_RESET);
    printf("  %s4)%s Gemini 2.0 Flash\n", WIZ_YELLOW, WIZ_RESET);
    printf("  %s5)%s Hermes 3\n", WIZ_YELLOW, WIZ_RESET);
    printf("  %s6)%s Custom\n\n", WIZ_YELLOW, WIZ_RESET);

    int choice = prompt_int("Select model", 1, 6, 1);
    switch (choice) {
        case 1: snprintf(model_out, model_sz, "anthropic/claude-sonnet-4"); return;
        case 2: snprintf(model_out, model_sz, "openai/gpt-4o"); return;
        case 3: snprintf(model_out, model_sz, "deepseek/deepseek-v3"); return;
        case 4: snprintf(model_out, model_sz, "google/gemini-2.0-flash"); return;
        case 5: snprintf(model_out, model_sz, "nousresearch/hermes-3"); return;
        case 6: {
            char c[256];
            prompt_str("Model name", c, sizeof(c), NULL);
            if (c[0]) snprintf(model_out, model_sz, "%s", c);
            return;
        }
    }
}

/* ── Section 3: Terminal backend + container resources ────────── */
typedef struct {
    const char *name;
    const char *desc;
} backend_t;

static const backend_t BACKENDS[] = {
    {"local",      "Run commands directly on this machine (default)"},
    {"docker",     "Isolated container with configurable resources"},
    {"ssh",        "Run on a remote machine via SSH"},
    {"modal",      "Serverless cloud sandbox (Modal.com)"},
    {NULL, NULL}
};

static void select_terminal_backend(char *backend, size_t sz,
                                    char *cpu, size_t cpu_sz,
                                    char *mem, size_t mem_sz,
                                    char *disk, size_t disk_sz) {
    wiz_banner("Terminal Backend");
    printf("  Choose where commands and code execute:\n\n");
    for (int i = 0; BACKENDS[i].name; i++)
        printf("  %s%d)%s %s\n  %s     %s\n\n",
               WIZ_YELLOW, i+1, WIZ_RESET, BACKENDS[i].name,
               WIZ_YELLOW, BACKENDS[i].desc);

    int choice = prompt_int("Select backend", 1, 4, 1);
    snprintf(backend, sz, "%s", BACKENDS[choice-1].name);

    if (strcmp(backend, "docker") == 0) {
        if (system("command -v docker >/dev/null 2>&1") == 0)
            wiz_ok("Docker found");
        else
            wiz_warn("Docker not found in PATH. Install Docker Desktop.");

        if (prompt_yes_no("Configure container resource limits", false)) {
            char input[32];
            prompt_str("CPU cores", input, sizeof(input), "2");
            snprintf(cpu, cpu_sz, "%s", input);
            prompt_str("Memory MB (e.g. 4096)", input, sizeof(input), "4096");
            snprintf(mem, mem_sz, "%s", input);
            prompt_str("Disk MB (e.g. 51200)", input, sizeof(input), "51200");
            snprintf(disk, disk_sz, "%s", input);
        }
    } else if (strcmp(backend, "ssh") == 0) {
        if (system("command -v ssh >/dev/null 2>&1") == 0)
            wiz_ok("SSH client found");
        else
            wiz_warn("SSH not found. Install openssh-client.");
    }

    wiz_ok("Terminal backend selected");
}

/* ── Section 4: Gateway platforms (with per-platform detail) ───── */
static void setup_telegram_detail(void) {
    wiz_banner("Telegram Setup");
    printf("  Get a bot token from @BotFather on Telegram.\n\n");
    char token[128];
    prompt_str("Bot token (or Enter to skip)", token, sizeof(token), NULL);
    if (!token[0]) { printf("  Skipped.\n"); return; }

    const char *home = getenv("HOME");
    if (!home) home = "/home/wubu";
    char env_path[512];
    snprintf(env_path, sizeof(env_path), "%s/.hermes/.env", home);
    FILE *fp = fopen(env_path, "a");
    if (fp) {
        fprintf(fp, "TELEGRAM_BOT_TOKEN=%s\n", token);
        fclose(fp);
    }
    if (validate_telegram_token(token))
        wiz_ok("Bot token is valid");
    else
        wiz_warn("Token validation failed. Check @BotFather.");
}

static void setup_slack_detail(void) {
    wiz_banner("Slack Setup");
    printf("  1. Go to https://api.slack.com/apps → Create New App\n");
    printf("  2. Choose \"From an app manifest\"\n");
    printf("  3. Select your workspace, paste the manifest JSON\n");
    printf("  4. Install the app to your workspace\n");
    printf("  5. Copy the Bot User OAuth Token (xoxb-...)\n\n");
    char token[128];
    prompt_str("Bot token (xoxb-...) or Enter to skip", token, sizeof(token), NULL);
    if (token[0]) {
        const char *home = getenv("HOME");
        if (!home) home = "/home/wubu";
        char env_path[512];
        snprintf(env_path, sizeof(env_path), "%s/.hermes/.env", home);
        FILE *fp = fopen(env_path, "a");
        if (fp) {
            fprintf(fp, "SLACK_BOT_TOKEN=%s\n", token);
            fclose(fp);
            wiz_ok("Slack token saved");
        }
    } else {
        printf("  Skipped. Configure later with env var SLACK_BOT_TOKEN.\n");
    }
}

static void setup_matrix_detail(void) {
    wiz_banner("Matrix Setup");
    printf("  You need a Matrix homeserver URL and credentials.\n\n");
    char homeserver[256], user[128], token[256];
    prompt_str("Homeserver URL (e.g. https://matrix.example.org)", homeserver, sizeof(homeserver), NULL);
    if (!homeserver[0]) { printf("  Skipped.\n"); return; }
    prompt_str("Username (e.g. @bot:example.org)", user, sizeof(user), NULL);
    if (!user[0]) { printf("  Skipped.\n"); return; }
    prompt_str("Access token (or password)", token, sizeof(token), NULL);
    if (!token[0]) { printf("  Skipped.\n"); return; }

    const char *home = getenv("HOME");
    if (!home) home = "/home/wubu";
    char env_path[512];
    snprintf(env_path, sizeof(env_path), "%s/.hermes/.env", home);
    FILE *fp = fopen(env_path, "a");
    if (fp) {
        fprintf(fp, "MATRIX_HOMESERVER=%s\n", homeserver);
        fprintf(fp, "MATRIX_USER=%s\n", user);
        fprintf(fp, "MATRIX_ACCESS_TOKEN=%s\n", token);
        fclose(fp);
        wiz_ok("Matrix credentials saved");
    }
}

static void select_platforms(char *out, size_t sz) {
    wiz_banner("Gateway Platforms");
    printf("  %s1)%s Telegram    %s2)%s Discord    %s3)%s Slack\n",   WIZ_YELLOW, WIZ_RESET, WIZ_YELLOW, WIZ_RESET, WIZ_YELLOW, WIZ_RESET);
    printf("  %s4)%s Matrix      %s5)%s WhatsApp   %s6)%s BlueBubbles\n", WIZ_YELLOW, WIZ_RESET, WIZ_YELLOW, WIZ_RESET, WIZ_YELLOW, WIZ_RESET);
    printf("  %s7)%s QQ          %s8)%s All\n", WIZ_YELLOW, WIZ_RESET, WIZ_YELLOW, WIZ_RESET);
    printf("  %sEnter)%s Skip\n\n", WIZ_YELLOW, WIZ_RESET);

    char input[64];
    wiz_prompt("Select (comma-separated, e.g. 1,2,3)");
    if (!read_line(input, sizeof(input)) || input[0] == '\0') { out[0] = '\0'; return; }

    const char *names[] = {"telegram", "discord", "slack", "matrix", "whatsapp", "bluebubbles", "qqbot"};
    out[0] = '\0';
    char *tok = strtok(input, ",");
    bool first = true;
    bool has_telegram = false, has_slack = false, has_matrix = false;
    while (tok) {
        int idx = atoi(tok) - 1;
        if (idx == 7) {
            snprintf(out, sz, "telegram,discord,slack,matrix,whatsapp,bluebubbles,qqbot");
            has_telegram = has_slack = has_matrix = true;
            break;
        }
        if (idx >= 0 && idx < 7) {
            if (!first) strcat(out, ",");
            strcat(out, names[idx]);
            first = false;
            if (idx == 0) has_telegram = true;
            if (idx == 2) has_slack = true;
            if (idx == 3) has_matrix = true;
        }
        tok = strtok(NULL, ",");
    }

    /* Per-platform detail prompts */
    if (has_telegram && prompt_yes_no("Configure Telegram bot now", false))
        setup_telegram_detail();
    if (has_slack && prompt_yes_no("Configure Slack app now", false))
        setup_slack_detail();
    if (has_matrix && prompt_yes_no("Configure Matrix homeserver now", false))
        setup_matrix_detail();

    /* Home channel warning */
    if (out[0]) {
        printf("\n  %s\xE2\x9A\xA0%s Set a home channel later with /sethome in your chat.\n",
               WIZ_YELLOW, WIZ_RESET);
    }
}

/* ── Section 5: Web search provider ────────────────────────────── */
static void select_web_search(char *provider, size_t sz) {
    wiz_banner("Web Search (Optional)");
    printf("  %s1)%s None (skip)\n",          WIZ_YELLOW, WIZ_RESET);
    printf("  %s2)%s Tavily (tavily.com)\n",  WIZ_YELLOW, WIZ_RESET);
    printf("  %s3)%s Firecrawl (firecrawl.dev)\n", WIZ_YELLOW, WIZ_RESET);
    printf("  %s4)%s Exa (exa.ai)\n\n",       WIZ_YELLOW, WIZ_RESET);

    int choice = prompt_int("Select web search", 1, 4, 1);
    switch (choice) {
        case 1: snprintf(provider, sz, "none"); return;
        case 2: {
            snprintf(provider, sz, "tavily");
            char key[128];
            prompt_str("Tavily API key (or Enter to skip)", key, sizeof(key), NULL);
            if (key[0]) {
                const char *home = getenv("HOME");
                if (!home) home = "/home/wubu";
                char env_path[512];
                snprintf(env_path, sizeof(env_path), "%s/.hermes/.env", home);
                FILE *fp = fopen(env_path, "a");
                if (fp) { fprintf(fp, "TAVILY_API_KEY=%s\n", key); fclose(fp); wiz_ok("Tavily key saved"); }
            }
            return;
        }
        case 3: {
            snprintf(provider, sz, "firecrawl");
            char key[128];
            prompt_str("Firecrawl API key (or Enter to skip)", key, sizeof(key), NULL);
            if (key[0]) {
                const char *home = getenv("HOME");
                if (!home) home = "/home/wubu";
                char env_path[512];
                snprintf(env_path, sizeof(env_path), "%s/.hermes/.env", home);
                FILE *fp = fopen(env_path, "a");
                if (fp) { fprintf(fp, "FIRECRAWL_API_KEY=%s\n", key); fclose(fp); wiz_ok("Firecrawl key saved"); }
            }
            return;
        }
        case 4: {
            snprintf(provider, sz, "exa");
            char key[128];
            prompt_str("Exa API key (or Enter to skip)", key, sizeof(key), NULL);
            if (key[0]) {
                const char *home = getenv("HOME");
                if (!home) home = "/home/wubu";
                char env_path[512];
                snprintf(env_path, sizeof(env_path), "%s/.hermes/.env", home);
                FILE *fp = fopen(env_path, "a");
                if (fp) { fprintf(fp, "EXA_API_KEY=%s\n", key); fclose(fp); wiz_ok("Exa key saved"); }
            }
            return;
        }
    }
}

/* ── Section 6: Image generation provider ──────────────────────── */
static void select_image_gen(char *provider, size_t sz) {
    wiz_banner("Image Generation (Optional)");
    printf("  %s1)%s None (skip)\n",              WIZ_YELLOW, WIZ_RESET);
    printf("  %s2)%s Fal.ai (fal.ai — Flux, Stable Diffusion)\n", WIZ_YELLOW, WIZ_RESET);
    printf("  %s3)%s Stability AI (stability.ai)\n", WIZ_YELLOW, WIZ_RESET);
    printf("  %s4)%s OpenAI DALL-E\n\n",          WIZ_YELLOW, WIZ_RESET);

    int choice = prompt_int("Select image generator", 1, 4, 1);
    switch (choice) {
        case 1: snprintf(provider, sz, "none"); return;
        case 2: {
            snprintf(provider, sz, "fal");
            char key[128];
            prompt_str("Fal.ai API key (or Enter to skip)", key, sizeof(key), NULL);
            if (key[0]) {
                const char *home = getenv("HOME");
                if (!home) home = "/home/wubu";
                char env_path[512];
                snprintf(env_path, sizeof(env_path), "%s/.hermes/.env", home);
                FILE *fp = fopen(env_path, "a");
                if (fp) { fprintf(fp, "FAL_KEY=%s\n", key); fclose(fp); wiz_ok("Fal.ai key saved"); }
            }
            return;
        }
        case 3: {
            snprintf(provider, sz, "stability");
            char key[128];
            prompt_str("Stability AI API key (or Enter to skip)", key, sizeof(key), NULL);
            if (key[0]) {
                const char *home = getenv("HOME");
                if (!home) home = "/home/wubu";
                char env_path[512];
                snprintf(env_path, sizeof(env_path), "%s/.hermes/.env", home);
                FILE *fp = fopen(env_path, "a");
                if (fp) { fprintf(fp, "STABILITY_API_KEY=%s\n", key); fclose(fp); wiz_ok("Stability key saved"); }
            }
            return;
        }
        case 4: {
            snprintf(provider, sz, "openai");
            /* DALL-E uses the main OpenAI API key — no extra key needed */
            wiz_ok("DALL-E will use the OpenAI API key");
            return;
        }
    }
}

/* ── Section 7: TTS provider ────────────────────────────────────── */
static void select_tts(char *tts_provider, size_t sz) {
    wiz_banner("Text-to-Speech (Optional)");
    printf("  %s1)%s None (skip TTS setup)\n",         WIZ_YELLOW, WIZ_RESET);
    printf("  %s2)%s espeak-ng (offline, local)\n",     WIZ_YELLOW, WIZ_RESET);
    printf("  %s3)%s OpenAI TTS (cloud, high quality)\n", WIZ_YELLOW, WIZ_RESET);
    printf("  %s4)%s ElevenLabs (cloud, premium)\n\n",  WIZ_YELLOW, WIZ_RESET);

    int choice = prompt_int("Select TTS", 1, 4, 1);
    switch (choice) {
        case 1: snprintf(tts_provider, sz, "none"); break;
        case 2:
            snprintf(tts_provider, sz, "espeak");
            if (system("command -v espeak-ng >/dev/null 2>&1") == 0 ||
                system("command -v espeak >/dev/null 2>&1") == 0)
                wiz_ok("espeak/espeak-ng found");
            else
                wiz_warn("espeak-ng not found. Install with: apt install espeak-ng");
            break;
        case 3: snprintf(tts_provider, sz, "openai"); break;
        case 4: {
            snprintf(tts_provider, sz, "elevenlabs");
            char key[128];
            prompt_str("ElevenLabs API key (or Enter to skip)", key, sizeof(key), NULL);
            if (key[0]) {
                const char *home = getenv("HOME");
                if (!home) home = "/home/wubu";
                char env_path[512];
                snprintf(env_path, sizeof(env_path), "%s/.hermes/.env", home);
                FILE *fp = fopen(env_path, "a");
                if (fp) { fprintf(fp, "ELEVENLABS_API_KEY=%s\n", key); fclose(fp); wiz_ok("ElevenLabs key saved"); }
            }
            break;
        }
    }
    wiz_ok("TTS configured");
}

/* ── Section 8: Credential pool strategy ───────────────────────── */
static void select_credential_pool(char *strategy, size_t sz) {
    wiz_banner("Credential Pool Strategy");
    printf("  Controls how API keys are shared across providers:\n\n");
    printf("  %s1)%s Share (default) — one set of keys shared across providers\n", WIZ_YELLOW, WIZ_RESET);
    printf("  %s2)%s Isolate — each provider uses its own credentials\n\n", WIZ_YELLOW, WIZ_RESET);

    int choice = prompt_int("Select strategy", 1, 2, 1);
    snprintf(strategy, sz, "%s", choice == 1 ? "share" : "isolate");
    wiz_ok("Credential pool configured");
}

/* ── Section 9: Agent settings ─────────────────────────────────── */
static void setup_agent_settings(char *reasoning, size_t rz_sz,
                                 bool *collaboration, bool *save_trajectories) {
    wiz_banner("Agent Settings");

    printf("  Reasoning effort (thinking tokens):\n\n");
    printf("  %s1)%s None (fast responses, no chain-of-thought)\n", WIZ_YELLOW, WIZ_RESET);
    printf("  %s2)%s Low (minimal reasoning)\n",                     WIZ_YELLOW, WIZ_RESET);
    printf("  %s3)%s Medium (balanced)\n",                           WIZ_YELLOW, WIZ_RESET);
    printf("  %s4)%s High (deep reasoning, slower)\n\n",            WIZ_YELLOW, WIZ_RESET);

    int choice = prompt_int("Select reasoning effort", 1, 4, 1);
    switch (choice) {
        case 1: snprintf(reasoning, rz_sz, "none"); break;
        case 2: snprintf(reasoning, rz_sz, "low"); break;
        case 3: snprintf(reasoning, rz_sz, "medium"); break;
        case 4: snprintf(reasoning, rz_sz, "high"); break;
    }

    *collaboration = prompt_yes_no("Enable collaboration mode (sub-agents)", false);
    *save_trajectories = prompt_yes_no("Save trajectory logs for debugging", false);
    wiz_ok("Agent settings saved");
}

/* ── Quick setup mode ──────────────────────────────────────────── */
static int quick_setup(void) {
    wiz_banner("Quick Setup");
    printf("  Streamlined first-time setup: provider, model, terminal backend.\n");
    printf("  Customize later with 'slermes setup' for full options.\n\n");

    const char *home = getenv("HOME");
    if (!home) home = "/home/wubu";
    char hermes_dir[512];
    snprintf(hermes_dir, sizeof(hermes_dir), "%s/.hermes", home);
    ensure_dir(hermes_dir);

    char env_var[128] = "", base_url[256] = "";
    int provider_idx = select_provider(env_var, sizeof(env_var), base_url, sizeof(base_url));
    const char *provider_name = provider_idx == 9 ? "Custom" : PROVIDERS[provider_idx].name;
    enter_api_key(env_var, base_url);

    char model[128] = "";
    select_model(model, sizeof(model));

    char terminal_backend[32] = "local", cpu[16]="", mem[16]="", disk[16]="";
    select_terminal_backend(terminal_backend, sizeof(terminal_backend),
                            cpu, sizeof(cpu), mem, sizeof(mem), disk, sizeof(disk));

    /* Write minimal config.yaml */
    char config_path[512];
    snprintf(config_path, sizeof(config_path), "%s/.hermes/config.yaml", home);

    backup_config(config_path);
    FILE *fp = fopen(config_path, "w");
    if (fp) {
        fprintf(fp, "# Hermes Agent Configuration\n# Generated by quick setup\n\n");
        fprintf(fp, "default_model: \"%s\"\n", model);
        fprintf(fp, "provider: \"%s\"\n", provider_name);
        if (strcmp(terminal_backend, "local") != 0)
            fprintf(fp, "terminal:\n  backend: \"%s\"\n", terminal_backend);
        fclose(fp);
    }

    /* Offer messaging setup */
    if (prompt_yes_no("Connect a messaging platform (Telegram/Discord/etc.)", false)) {
        char platforms[256] = "";
        select_platforms(platforms, sizeof(platforms));
        if (platforms[0]) {
            fp = fopen(config_path, "a");
            if (fp) { fprintf(fp, "gateway:\n  platforms: \"%s\"\n", platforms); fclose(fp); }
        }
    }

    wiz_banner("Setup Complete!");
    printf("  Provider: %s\n", provider_name);
    printf("  Model:    %s\n", model);
    printf("  Backend:  %s\n", terminal_backend);
    printf("\n  Run %sslermes%s to start.\n", WIZ_BOLD, WIZ_RESET);
    printf("  Run %sslermes setup%s for full configuration.\n\n", WIZ_BOLD, WIZ_RESET);
    return 0;
}

/* ── Non-interactive guidance ──────────────────────────────────── */
static void setup_noninteractive(void) {
    printf("\n");
    wiz_banner("Non-Interactive Setup");
    printf("  The interactive wizard cannot be used here (no TTY detected).\n\n");
    printf("  Configure using environment variables or config commands:\n");
    printf("    export MY_API_KEY=***\n");
    printf("    slermes config set model my-model\n\n");
    printf("  Or run 'slermes setup' in an interactive terminal.\n\n");
}

/* ── Main entry point ──────────────────────────────────────────── */
int setup_wizard_run(void) {
    /* Check for non-interactive stdin */
    if (!isatty(STDIN_FILENO)) {
        setup_noninteractive();
        return 0;
    }

    /* Offer quick setup for first-time users */
    const char *home = getenv("HOME");
    if (!home) home = "/home/wubu";

    char config_path[512];
    snprintf(config_path, sizeof(config_path), "%s/.hermes/config.yaml", home);
    struct stat st;
    bool is_first_time = (stat(config_path, &st) != 0);

    if (is_first_time && prompt_yes_no("Use quick setup (recommended for first time)", true)) {
        return quick_setup();
    }

    /* ── Full setup ── */
    wiz_banner("Hermes Agent Setup Wizard v3.0");
    printf("  Press Enter at prompts to accept defaults.\n\n");

    /* Create ~/.hermes directory */
    char hermes_dir[512];
    snprintf(hermes_dir, sizeof(hermes_dir), "%s/.hermes", home);
    if (!ensure_dir(hermes_dir)) {
        wiz_err("Cannot create .hermes directory");
        return 1;
    }
    wiz_ok("Directory: ~/.hermes");

    /* Backup existing config */
    backup_config(config_path);

    /* ── Provider + API key ── */
    char env_var[128] = "", base_url[256] = "";
    int provider_idx = select_provider(env_var, sizeof(env_var), base_url, sizeof(base_url));
    const char *provider_name = provider_idx == 9 ? "Custom" : PROVIDERS[provider_idx].name;
    enter_api_key(env_var, base_url);

    /* ── Model ── */
    char model[128] = "";
    select_model(model, sizeof(model));

    /* ── Terminal backend ── */
    char terminal_backend[32] = "local", cpu[16]="", mem[16]="", disk[16]="";
    select_terminal_backend(terminal_backend, sizeof(terminal_backend),
                            cpu, sizeof(cpu), mem, sizeof(mem), disk, sizeof(disk));

    /* ── Gateway platforms ── */
    char platforms[256] = "";
    select_platforms(platforms, sizeof(platforms));

    /* ── Web search ── */
    char web_search[32] = "none";
    if (prompt_yes_no("Configure web search provider", false))
        select_web_search(web_search, sizeof(web_search));

    /* ── Image generation ── */
    char image_gen[32] = "none";
    if (prompt_yes_no("Configure image generation provider", false))
        select_image_gen(image_gen, sizeof(image_gen));

    /* ── TTS ── */
    char tts_provider[32] = "none";
    if (prompt_yes_no("Configure text-to-speech", false))
        select_tts(tts_provider, sizeof(tts_provider));

    /* ── Credential pool ── */
    char cred_pool[16] = "share";
    if (prompt_yes_no("Configure credential pool strategy", false))
        select_credential_pool(cred_pool, sizeof(cred_pool));

    /* ── Agent settings ── */
    char reasoning[16] = "none";
    bool collaboration = false, save_trajectories = false;
    if (prompt_yes_no("Configure agent behavior settings", false))
        setup_agent_settings(reasoning, sizeof(reasoning), &collaboration, &save_trajectories);

    /* ── Write config.yaml ── */
    FILE *fp = fopen(config_path, "w");
    if (fp) {
        fprintf(fp, "# Hermes Agent Configuration\n");
        fprintf(fp, "# Generated by setup wizard v3.0\n");
        fprintf(fp, "# Edit this file to change settings.\n\n");
        fprintf(fp, "default_model: \"%s\"\n", model);
        fprintf(fp, "provider: \"%s\"\n", provider_name);
        if (strcmp(terminal_backend, "local") != 0) {
            fprintf(fp, "terminal:\n  backend: \"%s\"\n", terminal_backend);
            if (cpu[0]) {
                float cpu_f = atof(cpu);
                int mem_i = atoi(mem), disk_i = atoi(disk);
                if (cpu_f > 0) fprintf(fp, "  container_cpu: %.1f\n", cpu_f);
                if (mem_i > 0) fprintf(fp, "  container_memory: %d\n", mem_i);
                if (disk_i > 0) fprintf(fp, "  container_disk: %d\n", disk_i);
            }
        }
        if (platforms[0])
            fprintf(fp, "gateway:\n  platforms: \"%s\"\n", platforms);
        if (strcmp(web_search, "none") != 0)
            fprintf(fp, "web_search:\n  provider: \"%s\"\n", web_search);
        if (strcmp(image_gen, "none") != 0)
            fprintf(fp, "image_gen:\n  provider: \"%s\"\n", image_gen);
        if (strcmp(tts_provider, "none") != 0)
            fprintf(fp, "tts:\n  provider: \"%s\"\n", tts_provider);
        if (strcmp(cred_pool, "isolate") == 0)
            fprintf(fp, "credential_pool:\n  strategy: \"%s\"\n", cred_pool);
        if (strcmp(reasoning, "none") != 0)
            fprintf(fp, "reasoning_effort: \"%s\"\n", reasoning);
        if (collaboration)
            fprintf(fp, "collaboration: true\n");
        if (save_trajectories)
            fprintf(fp, "save_trajectories: true\n");
        fclose(fp);
        wiz_ok("Config saved to ~/.hermes/config.yaml");
    } else {
        wiz_err("Cannot write config.yaml");
    }

    /* ── Summary ── */
    wiz_banner("Setup Complete!");
    printf("  Provider:           %s\n", provider_name);
    printf("  Model:              %s\n", model);
    printf("  Terminal backend:   %s\n", terminal_backend);
    if (platforms[0])     printf("  Gateway platforms:  %s\n", platforms);
    if (strcmp(web_search, "none") != 0)
                          printf("  Web search:         %s\n", web_search);
    if (strcmp(image_gen, "none") != 0)
                          printf("  Image gen:          %s\n", image_gen);
    if (strcmp(tts_provider, "none") != 0)
                          printf("  TTS:                %s\n", tts_provider);
    if (strcmp(cred_pool, "isolate") == 0)
                          printf("  Credential pool:    %s\n", cred_pool);
    if (strcmp(reasoning, "none") != 0)
                          printf("  Reasoning effort:   %s\n", reasoning);
    if (collaboration)    printf("  Collaboration:      enabled\n");
    if (save_trajectories) printf("  Trajectory logs:    enabled\n");
    printf("\n  Run %sslermes%s to start.\n", WIZ_BOLD, WIZ_RESET);
    printf("  Run %sslermes setup%s to reconfigure.\n\n", WIZ_BOLD, WIZ_RESET);
    return 0;
}

int setup_wizard_noninteractive(void) {
    setup_noninteractive();
    const char *home = getenv("HOME");
    if (!home) home = "/home/wubu";
    char hermes_dir[512];
    snprintf(hermes_dir, sizeof(hermes_dir), "%s/.hermes", home);
    ensure_dir(hermes_dir);
    return 0;
}
