/*
 * transport_common.c — shared LLM-transport helper primitives.
 * Faithful port of agent/transports/{base,anthropic,bedrock,codex,types}.py
 * map_finish_reason / role handling.
 */
#include "hermes_transport_common.h"
#include <string.h>
#include <strings.h>

static int ieq(const char *a, const char *b) {
    return a && b && strcasecmp(a, b) == 0;
}

/* PoP: transport_map_finish_reason @ agent/transports/anthropic.py:map_finish_reason */
/* PoP: transport_map_finish_reason @ agent/transports/bedrock.py:map_finish_reason */
/* PoP: transport_map_finish_reason @ agent/transports/codex.py:map_finish_reason */
/* PoP: transport_map_finish_reason @ agent/transports/base.py:map_finish_reason */
/* PoP: transport_map_finish_reason @ agent/transports/types.py:map_finish_reason */
const char *transport_map_finish_reason(const char *provider, const char *raw_reason)
{
    /* None / empty -> stop (types.py default) */
    if (!raw_reason || !*raw_reason) return "stop";

    if (provider && *provider) {
        if (ieq(provider, "anthropic")) {
            /* _STOP_REASON_MAP */
            if (ieq(raw_reason, "end_turn")) return "stop";
            if (ieq(raw_reason, "tool_use")) return "tool_calls";
            if (ieq(raw_reason, "max_tokens")) return "length";
            if (ieq(raw_reason, "stop_sequence")) return "stop";
            if (ieq(raw_reason, "refusal")) return "content_filter";
            if (ieq(raw_reason, "model_context_window_exceeded")) return "length";
            return "stop";
        }
        if (ieq(provider, "bedrock")) {
            /* normalize_converse_response map */
            if (ieq(raw_reason, "end_turn")) return "stop";
            if (ieq(raw_reason, "tool_use")) return "tool_calls";
            if (ieq(raw_reason, "max_tokens")) return "length";
            if (ieq(raw_reason, "stop_sequence")) return "stop";
            if (ieq(raw_reason, "guardrail_intervened")) return "content_filter";
            if (ieq(raw_reason, "content_filtered")) return "content_filter";
            return "stop";
        }
        if (ieq(provider, "google")) {
            /* provider_google.c google_map_finish_reason */
            if (ieq(raw_reason, "STOP")) return "stop";
            if (ieq(raw_reason, "MAX_TOKENS")) return "length";
            if (ieq(raw_reason, "SAFETY")) return "content_filter";
            if (ieq(raw_reason, "RECITATION")) return "content_filter";
            if (ieq(raw_reason, "BLOCKLIST")) return "content_filter";
            if (ieq(raw_reason, "PROHIBITED_CONTENT")) return "content_filter";
            if (ieq(raw_reason, "SPAM")) return "content_filter";
            if (ieq(raw_reason, "IMAGE_SAFETY")) return "content_filter";
            return "stop";
        }
        if (ieq(provider, "codex")) {
            /* codex.py map */
            if (ieq(raw_reason, "completed")) return "stop";
            if (ieq(raw_reason, "incomplete")) return "length";
            if (ieq(raw_reason, "failed")) return "stop";
            if (ieq(raw_reason, "cancelled")) return "stop";
            return "stop";
        }
        /* openai / openai-compatible: reason already in normalized vocabulary */
        if (ieq(provider, "openai")) {
            if (ieq(raw_reason, "stop")) return "stop";
            if (ieq(raw_reason, "length")) return "length";
            if (ieq(raw_reason, "tool_calls")) return "tool_calls";
            if (ieq(raw_reason, "content_filter")) return "content_filter";
            return "stop";
        }
    }
    /* No provider (base.py default) or unknown provider: identity pass-through. */
    return raw_reason;
}

const char *transport_normalize_role(const char *role)
{
    if (!role || !*role) return "user";
    if (ieq(role, "system")) return "system";
    if (ieq(role, "user")) return "user";
    if (ieq(role, "assistant")) return "assistant";
    if (ieq(role, "tool")) return "tool";
    if (ieq(role, "function")) return "tool";   /* OpenAI legacy function role */
    /* lenient default for anything unexpected */
    return "user";
}
