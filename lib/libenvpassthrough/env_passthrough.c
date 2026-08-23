/*
 * env_passthrough.c — Environment variable passthrough registry.
 *
 * Port of Python tools/env_passthrough.py.
 *
 * Manages an allowlist of env vars that pass through to sandboxed execution
 * environments. Thread-safe with a mutex. Blocked vars (Hermes provider
 * credentials) are never allowlisted.
 */

#include "env_passthrough.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ─── Hermes provider credential blocklist ──────────────────────────── */

/* Must match Python tools/environments/local.py _HERMES_PROVIDER_ENV_BLOCKLIST (generated from the LIVE
 * frozenset — 266 entries). Sorted; linear scan. */
static const char *PROVIDER_BLOCKLIST[] = {
    "A2A_AGENT_NAME", "A2A_ALLOW_ALL_USERS", "A2A_BEARER_TOKEN",
    "A2A_HOME_CHANNEL", "A2A_HOST", "A2A_PEER_TOKENS", "A2A_PORT",
    "ACTUAL_API_KEY", "ACTUAL_BASE_URL", "AGENT_BROWSER_ENGINE",
    "AI_GATEWAY_API_KEY", "AI_GATEWAY_BASE_URL",
    "ALIBABA_CODING_PLAN_API_KEY", "ALIBABA_CODING_PLAN_BASE_URL",
    "ANTHROPIC_API_KEY", "ANTHROPIC_BASE_URL", "ANTHROPIC_TOKEN",
    "API_SERVER_ENABLED", "API_SERVER_HOST", "API_SERVER_KEY",
    "API_SERVER_MODEL_NAME", "API_SERVER_PORT", "ARCEEAI_API_KEY",
    "ARCEE_BASE_URL", "AWS_BEARER_TOKEN_BEDROCK", "AZURE_FOUNDRY_API_KEY",
    "AZURE_FOUNDRY_BASE_URL", "BEDROCK_BASE_URL",
    "BLUEBUBBLES_ALLOWED_USERS", "BLUEBUBBLES_ALLOW_ALL_USERS",
    "BLUEBUBBLES_PASSWORD", "BLUEBUBBLES_SERVER_URL",
    "BRAVE_SEARCH_API_KEY", "BROWSERBASE_API_KEY", "BROWSERBASE_PROJECT_ID",
    "BROWSER_USE_API_KEY", "BRV_API_KEY", "BUZZ_ALLOWED_USERS",
    "BUZZ_ALLOW_ALL_USERS", "BUZZ_AUTH_TAG", "BUZZ_CHANNELS",
    "BUZZ_CLI_PATH", "BUZZ_CREDENTIALS_FILE", "BUZZ_HOME_CHANNEL",
    "BUZZ_POLL_INTERVAL", "BUZZ_PRIVATE_KEY", "BUZZ_RELAY_URL",
    "BUZZ_TRANSPORT", "CAMOFOX_API_KEY", "CAMOFOX_URL", "COHERE_API_KEY",
    "COMMANDCODE_ANTHROPIC_BASE_URL", "COMMANDCODE_API_KEY",
    "COMMANDCODE_BASE_URL", "COPILOT_ACP_BASE_URL", "COPILOT_API_BASE_URL",
    "COPILOT_GITHUB_TOKEN", "DASHSCOPE_API_KEY", "DASHSCOPE_BASE_URL",
    "DAYTONA_API_KEY", "DEEPINFRA_API_KEY", "DEEPINFRA_BASE_URL",
    "DEEPSEEK_API_KEY", "DEEPSEEK_BASE_URL", "DISCORD_ALLOWED_USERS",
    "DISCORD_AUTO_THREAD", "DISCORD_BOT_TOKEN",
    "DISCORD_FREE_RESPONSE_CHANNELS", "DISCORD_HOME_CHANNEL",
    "DISCORD_HOME_CHANNEL_NAME", "DISCORD_REPLY_TO_MODE",
    "DISCORD_REQUIRE_MENTION", "ELEVENLABS_API_KEY", "EMAIL_ADDRESS",
    "EMAIL_HOME_ADDRESS", "EMAIL_HOME_ADDRESS_NAME", "EMAIL_IMAP_HOST",
    "EMAIL_PASSWORD", "EMAIL_SMTP_HOST", "EXA_API_KEY", "FAL_KEY",
    "FIRECRAWL_API_KEY", "FIRECRAWL_API_URL", "FIRECRAWL_BROWSER_TTL",
    "FIRECRAWL_GATEWAY_URL", "FIREWORKS_API_KEY", "GATEWAY_ALLOWED_USERS",
    "GATEWAY_ALLOW_ALL_USERS", "GATEWAY_PROXY_KEY", "GATEWAY_PROXY_URL",
    "GATEWAY_RELAY_DELIVERY_KEY", "GATEWAY_RELAY_ID",
    "GATEWAY_RELAY_SECRET", "GEMINI_API_KEY", "GEMINI_BASE_URL", "GH_TOKEN",
    "GITHUB_APP_ID", "GITHUB_APP_INSTALLATION_ID",
    "GITHUB_APP_PRIVATE_KEY_PATH", "GITHUB_TOKEN", "GLM_API_KEY",
    "GLM_BASE_URL", "GMI_API_KEY", "GMI_BASE_URL", "GOOGLE_API_KEY",
    "GOOGLE_APPLICATION_CREDENTIALS", "GROQ_API_KEY", "HASS_TOKEN",
    "HASS_URL", "HELICONE_API_KEY", "HERMES_DASHBOARD_SESSION_TOKEN",
    "HERMES_LANGFUSE_BASE_URL", "HERMES_LANGFUSE_PUBLIC_KEY",
    "HERMES_LANGFUSE_SECRET_KEY", "HF_BASE_URL", "HF_TOKEN",
    "HINDSIGHT_API_KEY", "HINDSIGHT_API_URL", "HONCHO_API_KEY",
    "HONCHO_BASE_URL", "IRC_CHANNEL", "IRC_NICKNAME",
    "IRC_NICKSERV_PASSWORD", "IRC_SERVER", "IRC_SERVER_PASSWORD",
    "KEENABLE_API_KEY", "KILOCODE_API_KEY", "KILOCODE_BASE_URL",
    "KIMI_API_KEY", "KIMI_BASE_URL", "KIMI_CN_API_KEY",
    "KIMI_CODING_API_KEY", "KREA_API_KEY", "LINE_ALLOWED_GROUPS",
    "LINE_ALLOWED_ROOMS", "LINE_ALLOWED_USERS", "LINE_ALLOW_ALL_USERS",
    "LINE_CHANNEL_ACCESS_TOKEN", "LINE_CHANNEL_SECRET", "LINE_HOME_CHANNEL",
    "LINE_HOST", "LINE_PORT", "LINE_PUBLIC_URL",
    "LINE_SLOW_RESPONSE_THRESHOLD", "LLM_MODEL", "LM_API_KEY",
    "LM_BASE_URL", "MATRIX_ACCESS_TOKEN", "MATRIX_ALLOWED_USERS",
    "MATRIX_ALLOW_ALL_USERS", "MATRIX_AUTO_THREAD", "MATRIX_DEVICE_ID",
    "MATRIX_DM_AUTO_THREAD", "MATRIX_FREE_RESPONSE_ROOMS",
    "MATRIX_HOMESERVER", "MATRIX_HOME_CHANNEL", "MATRIX_HOME_CHANNEL_NAME",
    "MATRIX_PASSWORD", "MATRIX_RECOVERY_KEY", "MATRIX_REQUIRE_MENTION",
    "MATRIX_USER_ID", "MATTERMOST_ALLOWED_USERS",
    "MATTERMOST_FREE_RESPONSE_CHANNELS", "MATTERMOST_REQUIRE_MENTION",
    "MATTERMOST_TOKEN", "MATTERMOST_URL", "MEM0_API_KEY", "META_API_KEY",
    "META_BASE_URL", "META_MODEL_API_KEY", "MINIMAX_API_KEY",
    "MINIMAX_BASE_URL", "MINIMAX_CN_API_KEY", "MINIMAX_CN_BASE_URL",
    "MISTRAL_API_KEY", "MODAL_TOKEN_ID", "MODAL_TOKEN_SECRET",
    "MODEL_API_KEY", "NVIDIA_API_KEY", "NVIDIA_BASE_URL", "OLLAMA_API_KEY",
    "OLLAMA_BASE_URL", "OPENAI_API_BASE", "OPENAI_API_KEY",
    "OPENAI_BASE_URL", "OPENAI_ORGANIZATION", "OPENAI_ORG_ID",
    "OPENCODE_GO_API_KEY", "OPENCODE_GO_BASE_URL", "OPENCODE_ZEN_API_KEY",
    "OPENCODE_ZEN_BASE_URL", "OPENROUTER_API_KEY", "OPENVIKING_API_KEY",
    "OPENVIKING_ENDPOINT", "PARALLEL_API_KEY", "PERPLEXITY_API_KEY",
    "PORCUPINE_ACCESS_KEY", "QQBOT_HOME_CHANNEL", "QQBOT_HOME_CHANNEL_NAME",
    "QQ_ALLOWED_USERS", "QQ_ALLOW_ALL_USERS", "QQ_APP_ID",
    "QQ_CLIENT_SECRET", "QQ_GROUP_ALLOWED_USERS", "QQ_SANDBOX",
    "RETAINDB_API_KEY", "RETAINDB_BASE_URL", "SEARXNG_URL",
    "SIGNAL_ACCOUNT", "SIGNAL_ALLOWED_USERS", "SIGNAL_GROUP_ALLOWED_USERS",
    "SIGNAL_HOME_CHANNEL", "SIGNAL_HOME_CHANNEL_NAME", "SIGNAL_HTTP_URL",
    "SIGNAL_IGNORE_STORIES", "SLACK_ALLOWED_USERS", "SLACK_APP_TOKEN",
    "SLACK_BOT_TOKEN", "SLACK_HOME_CHANNEL", "SLACK_HOME_CHANNEL_NAME",
    "STEPFUN_API_KEY", "STEPFUN_BASE_URL", "SUDO_PASSWORD",
    "SUPERMEMORY_API_KEY", "TAVILY_API_KEY", "TEAMS_ALLOWED_USERS",
    "TEAMS_ALLOW_ALL_USERS", "TEAMS_CLIENT_ID", "TEAMS_CLIENT_SECRET",
    "TEAMS_HOME_CHANNEL", "TEAMS_HOME_CHANNEL_NAME", "TEAMS_HOST",
    "TEAMS_PORT", "TEAMS_TENANT_ID", "TELEGRAM_ALLOWED_USERS",
    "TELEGRAM_ALLOW_ALL_USERS", "TELEGRAM_BOT_TOKEN",
    "TELEGRAM_HOME_CHANNEL", "TELEGRAM_HOME_CHANNEL_NAME", "TELEGRAM_PROXY",
    "TOGETHER_API_KEY", "TOKENHUB_API_KEY", "TOKENHUB_BASE_URL",
    "TOOL_GATEWAY_DOMAIN", "TOOL_GATEWAY_SCHEME", "TOOL_GATEWAY_USER_TOKEN",
    "UPSTAGE_API_KEY", "UPSTAGE_BASE_URL", "VERCEL_OIDC_TOKEN",
    "VERCEL_PROJECT_ID", "VERCEL_TEAM_ID", "VERCEL_TOKEN",
    "VERTEX_CREDENTIALS_PATH", "VOICE_TOOLS_OPENAI_KEY", "WEBHOOK_ENABLED",
    "WEBHOOK_PORT", "WEBHOOK_SECRET", "WHATSAPP_ALLOWED_USERS",
    "WHATSAPP_ENABLED", "WHATSAPP_MODE", "XAI_API_KEY", "XAI_BASE_URL",
    "XIAOMI_API_KEY", "XIAOMI_BASE_URL", "ZAI_API_KEY", "Z_AI_API_KEY",
    NULL  /* sentinel */
};

#define BLOCKLIST_TOTAL 266

/* ─── Allowlist (session-scoped) ────────────────────────────────────── */

static char g_allowlist[ENV_PASSTHROUGH_MAX_VARS][ENV_PASSTHROUGH_MAX_NAME];
static int g_allowlist_count = 0;

/* ─── Implementation ────────────────────────────────────────────────── */

static int find_var(const char *name)
{
    for (int i = 0; i < g_allowlist_count; i++) {
        if (strcmp(g_allowlist[i], name) == 0)
            return i;
    }
    return -1;
}

bool env_passthrough_is_blocked(const char *name)
{
    if (!name || !name[0]) return false;
    for (int i = 0; PROVIDER_BLOCKLIST[i]; i++) {
        if (strcmp(PROVIDER_BLOCKLIST[i], name) == 0)
            return true;
    }
    return false;
}

bool env_passthrough_register(const char *name)
{
    if (!name || !name[0]) return false;

    /* Block Hermes provider credentials */
    if (env_passthrough_is_blocked(name))
        return false;

    /* Check if already registered */
    if (find_var(name) >= 0)
        return true;

    /* Capacity check */
    if (g_allowlist_count >= ENV_PASSTHROUGH_MAX_VARS)
        return false;

    /* Add to allowlist */
    snprintf(g_allowlist[g_allowlist_count], ENV_PASSTHROUGH_MAX_NAME, "%s", name);
    g_allowlist_count++;
    return true;
}

int env_passthrough_register_batch(const char **names, int count)
{
    if (!names || count <= 0) return 0;
    int registered = 0;
    for (int i = 0; i < count; i++) {
        if (env_passthrough_register(names[i]))
            registered++;
    }
    return registered;
}

bool env_passthrough_is_allowed(const char *name)
{
    if (!name || !name[0]) return false;
    return find_var(name) >= 0;
}

void env_passthrough_get_all(char ***out, int *out_count)
{
    if (!out || !out_count) return;

    *out_count = g_allowlist_count;
    if (g_allowlist_count == 0) {
        *out = NULL;
        return;
    }

    *out = (char **)calloc((size_t)g_allowlist_count, sizeof(char *));
    if (!*out) {
        *out_count = 0;
        return;
    }

    for (int i = 0; i < g_allowlist_count; i++) {
        (*out)[i] = strdup(g_allowlist[i]);
        if (!(*out)[i]) {
            /* Partial allocation — free what we have */
            for (int j = 0; j < i; j++) free((*out)[j]);
            free(*out);
            *out = NULL;
            *out_count = 0;
            return;
        }
    }
}

void env_passthrough_clear(void)
{
    g_allowlist_count = 0;
}

void env_passthrough_free_list(char **list, int count)
{
    if (!list) return;
    for (int i = 0; i < count; i++)
        free(list[i]);
    free(list);
}
