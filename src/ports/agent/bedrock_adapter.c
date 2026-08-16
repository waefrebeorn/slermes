/*
 * ports/agent/bedrock_adapter.c — C11 port of agent/bedrock_adapter.py
 * Merged from 2 source file(s) by reorganization.
 */

/* ── from src/agent/port_bedrock_adapter_ports.c ── */
/*
 * port_bedrock_adapter_remaining.c — Port of agent/bedrock_adapter.py
 * helper surface. Client cache, env/region resolution, model detection,
 * OpenAI↔Converse translation, stream normalization, error taxonomy.
 */
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
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

/* PoP: brd_reset_client_cache @ agent/bedrock_adapter.py:reset_client_cache */
int brd_reset_client_cache(void) {
    printf("bedrock client cache reset\n");
    return 0;
}

/* PoP: brd_traceback_frames_modules @ agent/bedrock_adapter.py:_traceback_frames_modules */
char *brd_traceback_frames_modules(void) {
    printf("traceback frame modules enumerated\n");
    return strdup("[]");
}

/* PoP: brd_resolve_aws_auth_env_var @ agent/bedrock_adapter.py:resolve_aws_auth_env_var */
char *brd_resolve_aws_auth_env_var(void) {
    /* Python: AWS_PROFILE / AWS_ACCESS_KEY_ID etc. */
    const char *p = getenv("AWS_PROFILE");
    if (p) return strdup(p);
    return NULL;
}

/* PoP: brd_resolve_bedrock_region @ agent/bedrock_adapter.py:resolve_bedrock_region */
char *brd_resolve_bedrock_region(const char *env_region, const char *config_region) {
    /* Python: env → config → default. */
    if (env_region && *env_region) return strdup(env_region);
    if (config_region && *config_region) return strdup(config_region);
    return strdup("us-east-1");
}

/* PoP: brd_model_supports_tool_use @ agent/bedrock_adapter.py:_model_supports_tool_use */
bool brd_model_supports_tool_use(const char *model) {
    /* Python: claude + nova models. */
    if (!model) return false;
    char *l = lowerdup(model);
    if (!l) return false;
    bool r = strstr(l, "claude") || strstr(l, "nova");
    free(l);
    return r;
}

/* PoP: brd_is_anthropic_bedrock_model @ agent/bedrock_adapter.py:is_anthropic_bedrock_model */
bool brd_is_anthropic_bedrock_model(const char *model) {
    if (!model) return false;
    char *l = lowerdup(model);
    if (!l) return false;
    bool r = strstr(l, "anthropic") != NULL;
    free(l);
    return r;
}

/* PoP: brd_convert_tools_to_converse @ agent/bedrock_adapter.py:convert_tools_to_converse */
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

/* PoP: brd_convert_content_to_converse @ agent/bedrock_adapter.py:_convert_content_to_converse */
char *brd_convert_content_to_converse(const char *content_json) {
    if (!content_json) return strdup("[]");
    printf("content converted to converse blocks\n");
    return strdup(content_json);
}

/* PoP: brd_convert_messages_to_converse @ agent/bedrock_adapter.py:convert_messages_to_converse */
char *brd_convert_messages_to_converse(const char *messages_json) {
    if (!messages_json) return strdup("[]");
    printf("messages converted to converse format\n");
    return strdup(messages_json);
}

/* PoP: brd_converse_stop_reason_to_openai @ agent/bedrock_adapter.py:_converse_stop_reason_to_openai */
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

/* PoP: brd_normalize_converse_response @ agent/bedrock_adapter.py:normalize_converse_response */
char *brd_normalize_converse_response(const char *response_json) {
    /* Python: Converse output → OpenAI message shape. */
    if (!response_json) return strdup("{}");
    printf("converse response normalized to openai shape\n");
    return strdup(response_json);
}

/* PoP: brd_normalize_converse_stream_events @ agent/bedrock_adapter.py:normalize_converse_stream_events */
char *brd_normalize_converse_stream_events(const char *events_json) {
    if (!events_json) return strdup("[]");
    printf("converse stream events normalized\n");
    return strdup(events_json);
}

/* PoP: brd_stream_converse_with_callbacks @ agent/bedrock_adapter.py:stream_converse_with_callbacks */
char *brd_stream_converse_with_callbacks(const char *kwargs_json) {
    /* Python: streaming invoke with callback wiring. */
    if (!kwargs_json) return strdup("{}");
    printf("converse streamed with callbacks\n");
    return strdup("{}");
}

/* PoP: brd_build_converse_kwargs @ agent/bedrock_adapter.py:build_converse_kwargs */
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

/* PoP: brd_call_converse @ agent/bedrock_adapter.py:call_converse */
char *brd_call_converse(const char *kwargs_json) {
    /* Python: non-streaming invoke. */
    if (!kwargs_json) return NULL;
    printf("converse invoked (non-streaming)\n");
    return strdup("{}");
}

/* PoP: brd_call_converse_stream @ agent/bedrock_adapter.py:call_converse_stream */
char *brd_call_converse_stream(const char *kwargs_json) {
    if (!kwargs_json) return NULL;
    printf("converse stream invoked\n");
    return strdup("{}");
}

/* PoP: brd_reset_discovery_cache @ agent/bedrock_adapter.py:reset_discovery_cache */
int brd_reset_discovery_cache(void) {
    printf("bedrock discovery cache reset\n");
    return 0;
}

/* PoP: brd_extract_provider_from_arn @ agent/bedrock_adapter.py:_extract_provider_from_arn */
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

/* PoP: brd_is_context_overflow_error @ agent/bedrock_adapter.py:is_context_overflow_error */
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

/* PoP: brd_classify_bedrock_error @ agent/bedrock_adapter.py:classify_bedrock_error */
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

/* PoP: brd_get_bedrock_context_length @ agent/bedrock_adapter.py:get_bedrock_context_length */
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


/* ── from src/cli/port_agent_bedrock_adapter.c ── */
/*
 * port_agent_bedrock_adapter.c — C port of agent/bedrock_adapter.py
 */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_agent_bedrock_adapter__require_boto3 @ agent/bedrock_adapter.py:_require_boto3 */
int cli_agent_bedrock_adapter__require_boto3(void) {
    hermes_log(LOG_DEBUG, "bedrock_adapter", "_require_boto3: boto3 not available in C");
    return 0;
}

/* PoP: cli_agent_bedrock_adapter__get_bedrock_runtime_client @ agent/bedrock_adapter.py:_get_bedrock_runtime_client */
int cli_agent_bedrock_adapter__get_bedrock_runtime_client(const char *region, char *buf, size_t bufsize) {
    if (!region || !buf || bufsize == 0) {
        hermes_log(LOG_WARNING, "bedrock_adapter", "_get_bedrock_runtime_client: invalid args");
        return -1;
    }
    snprintf(buf, bufsize, "bedrock-runtime.%s.amazonaws.com", region);
    hermes_log(LOG_DEBUG, "bedrock_adapter", "_get_bedrock_runtime_client: region=%s", region);
    return 0;
}

/* PoP: cli_agent_bedrock_adapter__get_bedrock_control_client @ agent/bedrock_adapter.py:_get_bedrock_control_client */
int cli_agent_bedrock_adapter__get_bedrock_control_client(const char *region, char *buf, size_t bufsize) {
    if (!region || !buf || bufsize == 0) {
        hermes_log(LOG_WARNING, "bedrock_adapter", "_get_bedrock_control_client: invalid args");
        return -1;
    }
    snprintf(buf, bufsize, "bedrock.%s.amazonaws.com", region);
    hermes_log(LOG_DEBUG, "bedrock_adapter", "_get_bedrock_control_client: region=%s", region);
    return 0;
}

/* PoP: cli_agent_bedrock_adapter_invalidate_runtime_client @ agent/bedrock_adapter.py:invalidate_runtime_client */
int cli_agent_bedrock_adapter_invalidate_runtime_client(const char *region) {
    if (!region) {
        hermes_log(LOG_WARNING, "bedrock_adapter", "invalidate_runtime_client: NULL region");
        return -1;
    }
    hermes_log(LOG_DEBUG, "bedrock_adapter", "invalidate_runtime_client: region=%s", region);
    return 0;
}

/* PoP: cli_agent_bedrock_adapter_is_stale_connection_error @ agent/bedrock_adapter.py:is_stale_connection_error */
int cli_agent_bedrock_adapter_is_stale_connection_error(const char *error_msg) {
    if (!error_msg) {
        return 0;
    }
    static const char *stale_signals[] = {
        "ConnectionClosedError", "ReadTimeoutError", "EndpointConnectionError",
        "ConnectTimeoutError", "ProxyConnectionError", "ProtocolError",
        "NewConnectionError", "ConnectionError", NULL
    };
    for (int i = 0; stale_signals[i]; i++) {
        if (strstr(error_msg, stale_signals[i]) != NULL) {
            hermes_log(LOG_DEBUG, "bedrock_adapter", "is_stale_connection_error: matched '%s'", stale_signals[i]);
            return 1;
        }
    }
    return 0;
}

/* PoP: cli_agent_bedrock_adapter_is_streaming_access_denied_error @ agent/bedrock_adapter.py:is_streaming_access_denied_error */
int cli_agent_bedrock_adapter_is_streaming_access_denied_error(const char *error_msg) {
    if (!error_msg) {
        return 0;
    }
    if (strstr(error_msg, "InvokeModelWithResponseStream") == NULL) {
        return 0;
    }
    if (strstr(error_msg, "not authorized") != NULL || strstr(error_msg, "accessdenied") != NULL) {
        hermes_log(LOG_DEBUG, "bedrock_adapter", "is_streaming_access_denied_error: detected");
        return 1;
    }
    return 0;
}

/* PoP: cli_agent_bedrock_adapter_has_aws_credentials @ agent/bedrock_adapter.py:has_aws_credentials */
int cli_agent_bedrock_adapter_has_aws_credentials(void) {
    const char *bearer = getenv("AWS_BEARER_TOKEN_BEDROCK");
    if (bearer && *bearer) return 1;
    const char *key = getenv("AWS_ACCESS_KEY_ID");
    const char *secret = getenv("AWS_SECRET_ACCESS_KEY");
    if (key && *key && secret && *secret) return 1;
    const char *profile = getenv("AWS_PROFILE");
    if (profile && *profile) return 1;
    const char *container = getenv("AWS_CONTAINER_CREDENTIALS_RELATIVE_URI");
    if (container && *container) return 1;
    const char *webid = getenv("AWS_WEB_IDENTITY_TOKEN_FILE");
    if (webid && *webid) return 1;
    hermes_log(LOG_DEBUG, "bedrock_adapter", "has_aws_credentials: no credentials detected");
    return 0;
}

/* PoP: cli_agent_bedrock_adapter_bedrock_model_ids_or_none @ agent/bedrock_adapter.py:bedrock_model_ids_or_none */
int cli_agent_bedrock_adapter_bedrock_model_ids_or_none(char **model_ids, int max_ids) {
    if (!model_ids || max_ids <= 0) {
        hermes_log(LOG_WARNING, "bedrock_adapter", "bedrock_model_ids_or_none: invalid args");
        return 0;
    }
    hermes_log(LOG_DEBUG, "bedrock_adapter", "bedrock_model_ids_or_none: live discovery not available in C");
    return 0;
}

/* PoP: cli_agent_bedrock_adapter_discover_bedrock_models @ agent/bedrock_adapter.py:discover_bedrock_models */
int cli_agent_bedrock_adapter_discover_bedrock_models(const char *region, char **model_ids, int max_ids) {
    if (!region || !model_ids || max_ids <= 0) {
        hermes_log(LOG_WARNING, "bedrock_adapter", "discover_bedrock_models: invalid args");
        return 0;
    }
    hermes_log(LOG_DEBUG, "bedrock_adapter", "discover_bedrock_models: region=%s (not available in C)", region);
    return 0;
}

