/*
 * setup_wizard_select.c - setup-wizard provider/backend selectors.
 *
 * Split from setup_wizard.c (soul.md: self-contained modules). Each
 * selector prompts for one setting; the PROVIDERS / BACKENDS tables move
 * with them. No file-scope state.
 */
#include "setup_wizard_select.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>


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


/* ── Config backup ─────────────────────────────────────────────── */ void backup_config(const char *path) {
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


/* ── Section 1: Provider + API key ─────────────────────────────── */ int select_provider(char *env_var, size_t env_sz, char *base_url, size_t url_sz) {
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
 void enter_api_key(const char *env_var, const char *base_url) {
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
    wiz_ensure_dir(hermes_dir);

    FILE *fp = fopen(env_path, "a");
    if (!fp) { wiz_err("Cannot write .env"); return; }
    fprintf(fp, "%s=%s\n", env_var, key);
    fclose(fp);
    wiz_ok("API key saved");
}


/* ── Section 2: Model selector ─────────────────────────────────── */ void select_model(char *model_out, size_t model_sz) {
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
 void select_terminal_backend(char *backend, size_t sz,
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
 void select_platforms(char *out, size_t sz) {
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


/* ── Section 5: Web search provider ────────────────────────────── */ void select_web_search(char *provider, size_t sz) {
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


/* ── Section 6: Image generation provider ──────────────────────── */ void select_image_gen(char *provider, size_t sz) {
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


/* ── Section 7: TTS provider ────────────────────────────────────── */ void select_tts(char *tts_provider, size_t sz) {
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


/* ── Section 8: Credential pool strategy ───────────────────────── */ void select_credential_pool(char *strategy, size_t sz) {
    wiz_banner("Credential Pool Strategy");
    printf("  Controls how API keys are shared across providers:\n\n");
    printf("  %s1)%s Share (default) — one set of keys shared across providers\n", WIZ_YELLOW, WIZ_RESET);
    printf("  %s2)%s Isolate — each provider uses its own credentials\n\n", WIZ_YELLOW, WIZ_RESET);

    int choice = prompt_int("Select strategy", 1, 2, 1);
    snprintf(strategy, sz, "%s", choice == 1 ? "share" : "isolate");
    wiz_ok("Credential pool configured");
}
