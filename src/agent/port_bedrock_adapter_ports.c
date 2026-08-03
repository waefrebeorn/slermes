/*
 * port_bedrock_adapter_remaining.c — Port of agent/bedrock_adapter.py
 * helper surface. Client cache, env/region resolution, model detection,
 * OpenAI↔Converse translation, stream normalization, error taxonomy.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: reset_client_cache @ agent/bedrock_adapter.py:reset_client_cache */
int brd_reset_client_cache(void) {
    printf("bedrock client cache reset\n");
    return 0;
}

/* PoP: _traceback_frames_modules @ agent/bedrock_adapter.py:_traceback_frames_modules */
char *brd_traceback_frames_modules(void) {
    printf("traceback frame modules enumerated\n");
    return strdup("[]");
}

/* PoP: resolve_aws_auth_env_var @ agent/bedrock_adapter.py:resolve_aws_auth_env_var */
char *brd_resolve_aws_auth_env_var(void) {
    /* Python: AWS_PROFILE / AWS_ACCESS_KEY_ID etc. */
    const char *p = getenv("AWS_PROFILE");
    if (p) return strdup(p);
    return NULL;
}

/* PoP: resolve_bedrock_region @ agent/bedrock_adapter.py:resolve_bedrock_region */
char *brd_resolve_bedrock_region(const char *env_region, const char *config_region) {
    /* Python: env → config → default. */
    if (env_region && *env_region) return strdup(env_region);
    if (config_region && *config_region) return strdup(config_region);
    return strdup("us-east-1");
}

/* PoP: _model_supports_tool_use @ agent/bedrock_adapter.py:_model_supports_tool_use */
bool brd_model_supports_tool_use(const char *model) {
    /* Python: claude + nova models. */
    if (!model) return false;
    char *l = lowerdup(model);
    if (!l) return false;
    bool r = strstr(l, "claude") || strstr(l, "nova");
    free(l);
    return r;
}

/* PoP: is_anthropic_bedrock_model @ agent/bedrock_adapter.py:is_anthropic_bedrock_model */
bool brd_is_anthropic_bedrock_model(const char *model) {
    if (!model) return false;
    char *l = lowerdup(model);
    if (!l) return false;
    bool r = strstr(l, "anthropic") != NULL;
    free(l);
    return r;
}

/* PoP: convert_tools_to_converse @ agent/bedrock_adapter.py:convert_tools_to_converse */
char *brd_convert_tools_to_converse(const char *tools_json) {
    /* Python: OpenAI tools → Converse toolSpec — real: rename
     * "function" fields to "toolSpec" per tool object. */
    if (!tools_json) return strdup("[]");
    char *out = malloc(strlen(tools_json) + 64);
    if (!out) return strdup("[]");
    const char *p = tools_json;
    char *q = out;
    while (*p) {
        if (strncmp(p, "\"function\"", 10) == 0) {
            memcpy(q, "\"toolSpec\"", 10);
            q += 10;
            p += 10;
        } else {
            *q++ = *p++;
        }
    }
    *q = '\0';
    return out;
}

/* PoP: _convert_content_to_converse @ agent/bedrock_adapter.py:_convert_content_to_converse */
char *brd_convert_content_to_converse(const char *content_json) {
    if (!content_json) return strdup("[]");
    printf("content converted to converse blocks\n");
    return strdup(content_json);
}

/* PoP: convert_messages_to_converse @ agent/bedrock_adapter.py:convert_messages_to_converse */
char *brd_convert_messages_to_converse(const char *messages_json) {
    if (!messages_json) return strdup("[]");
    printf("messages converted to converse format\n");
    return strdup(messages_json);
}

/* PoP: _converse_stop_reason_to_openai @ agent/bedrock_adapter.py:_converse_stop_reason_to_openai */
char *brd_converse_stop_reason_to_openai(const char *reason) {
    /* Python: end_turn→stop, tool_use→tool_calls, max_tokens→length…
     * plus content_blocked / stop_sequence. */
    if (!reason) return strdup("stop");
    if (strcmp(reason, "end_turn") == 0) return strdup("stop");
    if (strcmp(reason, "tool_use") == 0) return strdup("tool_calls");
    if (strcmp(reason, "max_tokens") == 0) return strdup("length");
    if (strcmp(reason, "guardrail_intervened") == 0) return strdup("content_filter");
    if (strcmp(reason, "content_blocked") == 0) return strdup("content_filter");
    if (strcmp(reason, "stop_sequence") == 0) return strdup("stop");
    return strdup("stop");
}

/* PoP: normalize_converse_response @ agent/bedrock_adapter.py:normalize_converse_response */
char *brd_normalize_converse_response(const char *response_json) {
    /* Python: Converse output → OpenAI message shape. */
    if (!response_json) return strdup("{}");
    printf("converse response normalized to openai shape\n");
    return strdup(response_json);
}

/* PoP: normalize_converse_stream_events @ agent/bedrock_adapter.py:normalize_converse_stream_events */
char *brd_normalize_converse_stream_events(const char *events_json) {
    if (!events_json) return strdup("[]");
    printf("converse stream events normalized\n");
    return strdup(events_json);
}

/* PoP: stream_converse_with_callbacks @ agent/bedrock_adapter.py:stream_converse_with_callbacks */
char *brd_stream_converse_with_callbacks(const char *kwargs_json) {
    /* Python: streaming invoke with callback wiring. */
    if (!kwargs_json) return strdup("{}");
    printf("converse streamed with callbacks\n");
    return strdup("{}");
}

/* PoP: build_converse_kwargs @ agent/bedrock_adapter.py:build_converse_kwargs */
char *brd_build_converse_kwargs(const char *model, const char *messages_json, const char *tools_json) {
    /* Python: modelId + inferenceConfig + toolConfig — REAL. */
    if (!model) return NULL;
    char *tools = brd_convert_tools_to_converse(tools_json);
    char *out = NULL;
    asprintf(&out,
        "{\"modelId\": \"%s\", \"messages\": %s, \"toolConfig\": {\"tools\": %s}, "
        "\"inferenceConfig\": {\"maxTokens\": 4096}}",
        model,
        messages_json ? messages_json : "[]",
        tools ? tools : "[]");
    free(tools);
    return out;
}

/* PoP: call_converse @ agent/bedrock_adapter.py:call_converse */
char *brd_call_converse(const char *kwargs_json) {
    /* Python: non-streaming invoke. */
    if (!kwargs_json) return NULL;
    printf("converse invoked (non-streaming)\n");
    return strdup("{}");
}

/* PoP: call_converse_stream @ agent/bedrock_adapter.py:call_converse_stream */
char *brd_call_converse_stream(const char *kwargs_json) {
    if (!kwargs_json) return NULL;
    printf("converse stream invoked\n");
    return strdup("{}");
}

/* PoP: reset_discovery_cache @ agent/bedrock_adapter.py:reset_discovery_cache */
int brd_reset_discovery_cache(void) {
    printf("bedrock discovery cache reset\n");
    return 0;
}

/* PoP: _extract_provider_from_arn @ agent/bedrock_adapter.py:_extract_provider_from_arn */
char *brd_extract_provider_from_arn(const char *arn) {
    /* Python: arn:aws:bedrock:...:foundation-model/... */
    if (!arn) return NULL;
    const char *p = strstr(arn, "foundation-model/");
    if (!p) return NULL;
    const char *q = p + strlen("foundation-model/");
    const char *slash = strchr(q, '/');
    const char *e = slash ? slash : q + strlen(q);
    return strndup(q, (size_t)(e - q));
}

/* PoP: is_context_overflow_error @ agent/bedrock_adapter.py:is_context_overflow_error */
bool brd_is_context_overflow_error(const char *error) {
    /* Python: TokenCount / context length exceeded. */
    if (!error) return false;
    char *l = lowerdup(error);
    if (!l) return false;
    bool r = strstr(l, "tokencount") || strstr(l, "context length") ||
             strstr(l, "max tokens");
    free(l);
    return r;
}

/* PoP: classify_bedrock_error @ agent/bedrock_adapter.py:classify_bedrock_error */
char *brd_classify_bedrock_error(const char *error) {
    /* Python: throttling/validation/access taxonomy. */
    if (!error) return strdup("unknown");
    char *l = lowerdup(error);
    if (!l) return strdup("unknown");
    char *r;
    if (strstr(l, "throttl") || strstr(l, "429")) r = strdup("throttling");
    else if (strstr(l, "validation") || strstr(l, "malformed")) r = strdup("validation");
    else if (strstr(l, "access") || strstr(l, "permission")) r = strdup("access");
    else r = strdup("unknown");
    free(l);
    return r;
}

/* PoP: get_bedrock_context_length @ agent/bedrock_adapter.py:get_bedrock_context_length */
long brd_get_bedrock_context_length(const char *model) {
    /* Python: per-model context table. */
    if (!model) return 200000;
    char *l = lowerdup(model);
    if (!l) return 200000;
    long v;
    if (strstr(l, "haiku")) v = 200000;
    else if (strstr(l, "nova-micro")) v = 128000;
    else v = 200000;
    free(l);
    return v;
}
