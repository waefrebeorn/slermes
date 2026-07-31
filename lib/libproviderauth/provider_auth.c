/*
 * provider_auth.c — Provider authentication registry (faithful port of
 * Python hermes_cli.auth.PROVIDER_REGISTRY).
 *
 * Self-contained: no dependency on provider_metadata.c (which is
 * provider-family + capability oriented and carries no auth_type) or the
 * libdb/hermes.h chain. The table below was regenerated from the LIVE Python
 * PROVIDER_REGISTRY via tests/sta_oracle_provider_pool_setup.py and is
 * diff-checked against it, so any drift is caught by the oracle.
 *
 * See provider_auth.h for the API contract.
 */

#include "provider_auth.h"

#include <stddef.h>
#include <string.h>

struct provider_auth_entry_t {
    const char *name;
    provider_auth_type_t auth_type;
};

/* Registry table — 45 keys, regenerated from LIVE Python PROVIDER_REGISTRY
 * (2026-07-31). Sorted for binary search. auth_type mirrors the Python
 * auth_type string (api_key=1, oauth_device_code=2, oauth_external=3,
 * aws_sdk=4, external_process=5, oauth_minimax=6, vertex/unknown=0).
 * Diff-checked against LIVE Python by tests/sta_oracle_provider_auth.py. */
static const provider_auth_entry_t PROVIDER_AUTH_TABLE[] = {
    {"alibaba",            PROVIDER_AUTH_API_KEY},
    {"alibaba-coding-plan",PROVIDER_AUTH_API_KEY},
    {"anthropic",          PROVIDER_AUTH_API_KEY},
    {"arcee",              PROVIDER_AUTH_API_KEY},
    {"azure-foundry",      PROVIDER_AUTH_API_KEY},
    {"bedrock",            PROVIDER_AUTH_AWS_SDK},
    {"copilot",            PROVIDER_AUTH_API_KEY},
    {"copilot-acp",        PROVIDER_AUTH_EXTERNAL_PROCESS},
    {"deep-infra",         PROVIDER_AUTH_API_KEY},
    {"deepinfra",          PROVIDER_AUTH_API_KEY},
    {"deepinfra-ai",       PROVIDER_AUTH_API_KEY},
    {"deepseek",           PROVIDER_AUTH_API_KEY},
    {"fireworks",          PROVIDER_AUTH_API_KEY},
    {"fireworks-ai",       PROVIDER_AUTH_API_KEY},
    {"fw",                 PROVIDER_AUTH_API_KEY},
    {"gemini",             PROVIDER_AUTH_API_KEY},
    {"gmi",                PROVIDER_AUTH_API_KEY},
    {"huggingface",        PROVIDER_AUTH_API_KEY},
    {"kilocode",           PROVIDER_AUTH_API_KEY},
    {"kimi-coding",        PROVIDER_AUTH_API_KEY},
    {"kimi-coding-cn",     PROVIDER_AUTH_API_KEY},
    {"lmstudio",           PROVIDER_AUTH_API_KEY},
    {"minimax",            PROVIDER_AUTH_API_KEY},
    {"minimax-cn",         PROVIDER_AUTH_API_KEY},
    {"minimax-oauth",      PROVIDER_AUTH_OAUTH_MINIMAX},
    {"nous",               PROVIDER_AUTH_OAUTH_DEVICE_CODE},
    {"novita",             PROVIDER_AUTH_API_KEY},
    {"novita-ai",          PROVIDER_AUTH_API_KEY},
    {"novitaai",           PROVIDER_AUTH_API_KEY},
    {"nvidia",             PROVIDER_AUTH_API_KEY},
    {"ollama-cloud",       PROVIDER_AUTH_API_KEY},
    {"openai-api",         PROVIDER_AUTH_API_KEY},
    {"openai-codex",       PROVIDER_AUTH_OAUTH_EXTERNAL},
    {"opencode-go",        PROVIDER_AUTH_API_KEY},
    {"opencode-zen",       PROVIDER_AUTH_API_KEY},
    {"qwen-oauth",         PROVIDER_AUTH_OAUTH_EXTERNAL},
    {"solar",              PROVIDER_AUTH_API_KEY},
    {"stepfun",            PROVIDER_AUTH_API_KEY},
    {"tencent-tokenhub",   PROVIDER_AUTH_API_KEY},
    {"upstage",            PROVIDER_AUTH_API_KEY},
    {"vertex",             PROVIDER_AUTH_UNKNOWN},
    {"xai",                PROVIDER_AUTH_API_KEY},
    {"xai-oauth",          PROVIDER_AUTH_OAUTH_EXTERNAL},
    {"xiaomi",             PROVIDER_AUTH_API_KEY},
    {"zai",                PROVIDER_AUTH_API_KEY},
};

static const size_t PROVIDER_AUTH_COUNT =
    sizeof(PROVIDER_AUTH_TABLE) / sizeof(PROVIDER_AUTH_TABLE[0]);

/* Lower-case compare (registry keys are lowercase; accept any case input). */
static int lc_eq(const char *a, const char *b) {
    if (!a || !b) return 0;
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return 0;
        a++; b++;
    }
    return *a == *b;
}

provider_auth_type_t provider_auth_lookup(const char *provider) {
    if (!provider || !*provider) return PROVIDER_AUTH_UNKNOWN;
    for (size_t i = 0; i < PROVIDER_AUTH_COUNT; i++) {
        if (lc_eq(PROVIDER_AUTH_TABLE[i].name, provider))
            return PROVIDER_AUTH_TABLE[i].auth_type;
    }
    return PROVIDER_AUTH_UNKNOWN;
}

bool provider_auth_supports_pool(const char *provider) {
    if (!provider || !*provider) return false;
    if (lc_eq(provider, "custom")) return false;
    if (lc_eq(provider, "openrouter")) return true;

    provider_auth_type_t t = provider_auth_lookup(provider);
    return t == PROVIDER_AUTH_API_KEY || t == PROVIDER_AUTH_OAUTH_DEVICE_CODE;
}

bool provider_auth_iterate(size_t *idx, const char **out_name,
                           provider_auth_type_t *out_type) {
    if (!idx) return false;
    if (*idx >= PROVIDER_AUTH_COUNT) return false;
    if (out_name) *out_name = PROVIDER_AUTH_TABLE[*idx].name;
    if (out_type) *out_type = PROVIDER_AUTH_TABLE[*idx].auth_type;
    (*idx)++;
    return true;
}
