/*
 * config_setup_wizard.c — interactive config-setup wizard.
 *
 * Split from config_setup.c (soul.md: self-contained modules). The wizard
 * steps and their prompt helpers; the exported setup entries stay in
 * config_setup.c and call in through config_setup_wizard.h.
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

void setup_apply_default_agent_settings(hermes_config_t *cfg) {
    if (!cfg) return;
    cfg->max_turns = 90;
    cfg->verbose = 1;       /* "all" mode */
    cfg->compress_enabled = true;
}

void setup_print_header(const char *title) {
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

char *setup_prompt(const char *question, const char *default_val) {
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

bool setup_prompt_yes_no(const char *question, bool default_yes) {
    const char *msg = default_yes ? " (Y/n)" : " (y/N)";
    char buf[128];
    snprintf(buf, sizeof(buf), "%s%s", question, msg);
    return cw_confirm(buf, NULL);
}

int setup_prompt_choice(const char *question, const char *choices[],
                                int n_choices, int default_idx) {
    char **items = (char **)choices;
    int result = cw_radiolist(question, items, n_choices,
                               default_idx, default_idx, NULL, false);
    if (result < 0) result = default_idx;
    return result;
}

void setup_agent_settings(hermes_config_t *cfg) {
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

void setup_terminal_backend(hermes_config_t *cfg) {
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

void setup_print_summary(const hermes_config_t *cfg) {
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

bool setup_is_interactive(void) {
    return isatty(fileno(stdin));
}

void setup_print_noninteractive_guidance(const char *reason) {
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

void setup_sanitize_pasted_input(char *value) {
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

void setup_prompt_api_key(const setup_provider_key_info_t *info,
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

void setup_prompt_base_url(const char *provider, char *out, size_t out_size) {
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

bool setup_save_env(const char *key, const char *value) {
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

void setup_tts(hermes_config_t *cfg) {
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

void setup_gateway(hermes_config_t *cfg) {
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

void setup_set_provider_endpoint(hermes_config_t *cfg, const char *provider) {
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

void setup_nous_model_picker(char *out_model, size_t out_size) {
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

char **setup_fetch_provider_models(const char *provider_name,
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

void setup_pick_model(char *out_model, size_t out_size,
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

const char *SETUP_OPENROUTER_CURATED[] = {
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

const char *SETUP_PROVIDERS[] = {
    "nous", "openai", "anthropic", "google", "deepseek", "xai",
    "openrouter", "azure", "bedrock", "codex", "custom", NULL
};

 const char *SETUP_FALLBACK_NOUS[] = {
     "deepseek/deepseek-v4-flash", "deepseek/deepseek-v4",
     "claude-sonnet-4-20250514", "gpt-4.1", "gemini-2.5-pro", "grok-3", NULL
 };

 const char *SETUP_FALLBACK_OPENAI[] = {
     "gpt-4.1", "gpt-4o", "gpt-4o-mini", "o3-mini", "o1", "gpt-4-turbo", NULL
 };

 const char *SETUP_FALLBACK_ANTHROPIC[] = {
     "claude-sonnet-4-20250514", "claude-opus-4", "claude-haiku-3.5", "claude-3.5-sonnet", NULL
 };

 const char *SETUP_FALLBACK_GOOGLE[] = {
     "gemini-3.1-pro-preview", "gemini-3-pro-preview",
     "gemini-3-flash-preview", "gemini-3.1-flash-lite-preview", NULL
 };

 const char *SETUP_FALLBACK_DEEPSEEK[] = {
     "deepseek-chat", "deepseek-reasoner", "deepseek/deepseek-v4", "deepseek/deepseek-v4-flash", NULL
 };

 const char *SETUP_FALLBACK_XAI[] = {
     "grok-3", "grok-3-mini", "grok-3-reasoner", "grok-3-mini-reasoner", NULL
 };

 const char *SETUP_FALLBACK_OPENROUTER[] = {
     "openai/gpt-4.1", "anthropic/claude-sonnet-4-20250514",
     "google/gemini-2.5-pro", "deepseek/deepseek-v4",
     "meta-llama/llama-4-scout", "mistral/mistral-large-2", NULL
 };

 const char *SETUP_FALLBACK_AZURE[] = {
     "gpt-4o", "gpt-4o-mini", "gpt-4.1", "o3-mini", NULL
 };

 const char *SETUP_FALLBACK_BEDROCK[] = {
     "anthropic.claude-sonnet-4-20250514", "anthropic.claude-3-5-sonnet",
     "meta.llama4-scout-17b", "mistral.mistral-large-2407", NULL
 };

 const char *SETUP_FALLBACK_CODEX[] = {
     "gpt-5.4-codex", "gpt-5.3-codex", "gpt-5.2-codex",
     "claude-sonnet-4.6-codex", "gemini-3-flash-codex", NULL
 };

 const char **SETUP_FALLBACKS[] = {
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

