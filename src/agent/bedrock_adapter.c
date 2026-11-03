/*
 * bedrock_adapter.c — Port of Python agent/bedrock_adapter.py
 *
 * Python API → C implementation mapping:
 *   bedrock_adapter_process_message()  → provider_bedrock.c
 *   bedrock_adapter_stream()           → provider_bedrock.c
 *   bedrock_adapter_build_request()    → provider_bedrock.c
 *   bedrock_adapter_parse_response()   → provider_bedrock.c
 *   bedrock_adapter_count_tokens()     → provider_bedrock.c
 *
 * AWS Bedrock provider adapter implemented in provider_bedrock.c.
 */

#include "provider.h"   /* provider_bedrock interface via provider dispatch */

/* ─────────────────────────────────────────────────────────────────────
 * Bedrock utility helpers exercised by tests/test_bedrock_depth.c.
 *
 * Ported from agent/bedrock_adapter.py. Pure string/env predicates; the
 * model tables below mirror the Python constants.
 * ───────────────────────────────────────────────────────────────────── */

/* PoP: bedrock_classify_error @ agent/bedrock_adapter.py:_classify_bedrock_error */
/* Bucket a Bedrock error string into a retry-relevant class.
 * Returns a static string: "context_overflow" | "rate_limit" | "overloaded"
 * | "unknown". Case-insensitive. */
const char *bedrock_classify_error(const char *error)
{
    if (!error) return "unknown";

    if (strcasestr(error, "input is too long") ||
        strcasestr(error, "context length") ||
        strcasestr(error, "too many tokens")) {
        return "context_overflow";
    }
    if (strcasestr(error, "ThrottlingException") ||
        strcasestr(error, "Too many concurrent") ||
        strcasestr(error, "ServiceQuotaExceeded")) {
        return "rate_limit";
    }
    if (strcasestr(error, "ModelNotReady") ||
        strcasestr(error, "ModelTimeout") ||
        strcasestr(error, "InternalServer")) {
        return "overloaded";
    }
    return "unknown";
}

/* PoP: bedrock_get_context_length @ agent/bedrock_adapter.py:_model_context_length */
/* Context window for a Bedrock model id; 128000 when unrecognised. */
int bedrock_get_context_length(const char *model_id)
{
    static const struct { const char *needle; int tokens; } TABLE[] = {
        {"nova-pro",              300000},
        {"nova-lite",             300000},
        {"claude-sonnet-4",       200000},
        {"claude-opus-4",         200000},
        {"claude-3-7-sonnet",     200000},
        {"claude-3-5-sonnet",     200000},
    };
    if (!model_id) return 128000;
    for (size_t i = 0; i < sizeof(TABLE) / sizeof(TABLE[0]); i++) {
        if (strcasestr(model_id, TABLE[i].needle)) return TABLE[i].tokens;
    }
    return 128000;
}

/* PoP: bedrock_is_anthropic_model @ agent/bedrock_adapter.py:_is_anthropic_model */
/* True for any Anthropic model id, including region-prefixed forms such as
 * "us.anthropic.claude-…" and "global.anthropic.…". */
bool bedrock_is_anthropic_model(const char *model_id)
{
    if (!model_id || !model_id[0]) return false;
    return strcasestr(model_id, "anthropic.") != NULL ||
           strncasecmp(model_id, "anthropic", 9) == 0;
}

/* PoP: bedrock_model_supports_tool_use @ agent/bedrock_adapter.py:_supports_tool_use */
/* False for reasoning-only and embedding/image models. */
bool bedrock_model_supports_tool_use(const char *model_id)
{
    if (!model_id || !model_id[0]) return false;

    static const char *const NO_TOOLS[] = {
        "deepseek.r1", "deepseek-r1",
        "stability.",
        "cohere.embed",
        "amazon.titan-embed",
    };
    for (size_t i = 0; i < sizeof(NO_TOOLS) / sizeof(NO_TOOLS[0]); i++) {
        if (strcasestr(model_id, NO_TOOLS[i])) return false;
    }
    return true;
}

/* PoP: bedrock_resolve_auth_env_var @ agent/bedrock_adapter.py:_resolve_auth_env_var */
/* Which env var(s) supply Bedrock credentials, in precedence order.
 * AWS_ACCESS_KEY_ID requires AWS_SECRET_ACCESS_KEY to be present too. */
const char *bedrock_resolve_auth_env_var(void)
{
    const char *v;
    if ((v = getenv("AWS_BEARER_TOKEN_BEDROCK")) && v[0]) return "AWS_BEARER_TOKEN_BEDROCK";
    if ((v = getenv("AWS_ACCESS_KEY_ID")) && v[0]) {
        const char *s = getenv("AWS_SECRET_ACCESS_KEY");
        if (s && s[0]) return "AWS_ACCESS_KEY_ID";
    }
    if ((v = getenv("AWS_PROFILE")) && v[0]) return "AWS_PROFILE";
    return NULL;
}

/* PoP: bedrock_has_credentials @ agent/bedrock_adapter.py:_has_credentials */
bool bedrock_has_credentials(void)
{
    return bedrock_resolve_auth_env_var() != NULL;
}

/* PoP: bedrock_resolve_region @ agent/bedrock_adapter.py:_resolve_region */
/* AWS_REGION wins over AWS_DEFAULT_REGION; otherwise us-east-1. */
const char *bedrock_resolve_region(void)
{
    const char *v;
    if ((v = getenv("AWS_REGION")) && v[0]) return v;
    if ((v = getenv("AWS_DEFAULT_REGION")) && v[0]) return v;
    return "us-east-1";
}
