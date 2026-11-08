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
#include "setup_wizard_ui.h"
#include "setup_wizard_select.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

/* ── ANSI helpers ──────────────────────────────────────────────── */

/* ── Provider definitions ──────────────────────────────────────── */


/* ── Section 3: Terminal backend + container resources ────────── */


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
    wiz_ensure_dir(hermes_dir);

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
    if (!wiz_ensure_dir(hermes_dir)) {
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
    wiz_ensure_dir(hermes_dir);
    return 0;
}
