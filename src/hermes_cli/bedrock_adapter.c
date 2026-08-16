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
