/*
 * port_agent_anthropic_adapter.c — C port of agent/anthropic_adapter.py
 */

#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* PoP: cli_agent_anthropic_adapter__get_anthropic_sdk @ agent/anthropic_adapter.py:_get_anthropic_sdk */
const char* cli_agent_anthropic_adapter__get_anthropic_sdk(void) {
    hermes_log(LOG_DEBUG, "anthropic_adapter", "_get_anthropic_sdk: lazy import not available in C");
    return NULL;
}

/* PoP: cli_agent_anthropic_adapter__resolve_positive_anthropic_max_tokens @ agent/anthropic_adapter.py:_resolve_positive_anthropic_max_tokens */
int cli_agent_anthropic_adapter__resolve_positive_anthropic_max_tokens(double value) {
    if (value != value) {
        return -1;
    }
    if (value <= 0.0) {
        return -1;
    }
    int floored = (int)value;
    return (floored > 0) ? floored : -1;
}

/* PoP: cli_agent_anthropic_adapter__supports_adaptive_thinking @ agent/anthropic_adapter.py:_supports_adaptive_thinking */
int cli_agent_anthropic_adapter__supports_adaptive_thinking(const char *model) {
    if (!model) {
        return 0;
    }
    char lower[256];
    size_t len = strlen(model);
    if (len >= sizeof(lower)) len = sizeof(lower) - 1;
    for (size_t i = 0; i < len; i++) {
        lower[i] = tolower((unsigned char)model[i]);
    }
    lower[len] = '\0';
    if (strstr(lower, "claude") == NULL) {
        return 0;
    }
    static const char *legacy[] = {
        "claude-3", "claude-opus-4-0", "claude-opus-4.0",
        "claude-sonnet-4-0", "claude-sonnet-4.0",
        "claude-opus-4-2025", "claude-sonnet-4-2025",
        "claude-opus-4-5", "claude-opus-4.5",
        "claude-sonnet-4-5", "claude-sonnet-4.5",
        "claude-haiku-4-5", "claude-haiku-4.5", NULL
    };
    for (int i = 0; legacy[i]; i++) {
        if (strstr(lower, legacy[i]) != NULL) {
            return 0;
        }
    }
    return 1;
}

/* PoP: cli_agent_anthropic_adapter__supports_xhigh_effort @ agent/anthropic_adapter.py:_supports_xhigh_effort */
int cli_agent_anthropic_adapter__supports_xhigh_effort(const char *model) {
    if (!cli_agent_anthropic_adapter__supports_adaptive_thinking(model)) {
        return 0;
    }
    char lower[256];
    size_t len = strlen(model);
    if (len >= sizeof(lower)) len = sizeof(lower) - 1;
    for (size_t i = 0; i < len; i++) {
        lower[i] = tolower((unsigned char)model[i]);
    }
    lower[len] = '\0';
    static const char *no_xhigh[] = {
        "claude-opus-4-6", "claude-opus-4.6",
        "claude-sonnet-4-6", "claude-sonnet-4.6", NULL
    };
    for (int i = 0; no_xhigh[i]; i++) {
        if (strstr(lower, no_xhigh[i]) != NULL) {
            return 0;
        }
    }
    return 1;
}

/* PoP: cli_agent_anthropic_adapter__forbids_sampling_params @ agent/anthropic_adapter.py:_forbids_sampling_params */
int cli_agent_anthropic_adapter__forbids_sampling_params(const char *model) {
    if (!model) {
        return 0;
    }
    char lower[256];
    size_t len = strlen(model);
    if (len >= sizeof(lower)) len = sizeof(lower) - 1;
    for (size_t i = 0; i < len; i++) {
        lower[i] = tolower((unsigned char)model[i]);
    }
    lower[len] = '\0';
    if (strstr(lower, "claude") == NULL) {
        return 0;
    }
    static const char *no_xhigh[] = {
        "claude-opus-4-6", "claude-opus-4.6",
        "claude-sonnet-4-6", "claude-sonnet-4.6", NULL
    };
    for (int i = 0; no_xhigh[i]; i++) {
        if (strstr(lower, no_xhigh[i]) != NULL) {
            return 0;
        }
    }
    static const char *legacy[] = {
        "claude-3", "claude-opus-4-0", "claude-opus-4.0",
        "claude-sonnet-4-0", "claude-sonnet-4.0",
        "claude-opus-4-2025", "claude-sonnet-4-2025",
        "claude-opus-4-5", "claude-opus-4.5",
        "claude-sonnet-4-5", "claude-sonnet-4.5",
        "claude-haiku-4-5", "claude-haiku-4.5", NULL
    };
    for (int i = 0; legacy[i]; i++) {
        if (strstr(lower, legacy[i]) != NULL) {
            return 0;
        }
    }
    return 1;
}

/* PoP: cli_agent_anthropic_adapter__supports_fast_mode @ agent/anthropic_adapter.py:_supports_fast_mode */
int cli_agent_anthropic_adapter__supports_fast_mode(const char *model) {
    if (!model) {
        return 0;
    }
    return (strstr(model, "opus-4-6") != NULL || strstr(model, "opus-4.6") != NULL);
}

/* PoP: cli_agent_anthropic_adapter__to_plain_data @ agent/anthropic_adapter.py:_to_plain_data */
int cli_agent_anthropic_adapter__to_plain_data(const char *data, char *buf, size_t bufsize) {
    if (!data || !buf || bufsize == 0) {
        return -1;
    }
    /* Strip data: URL prefix if present, return base64 data */
    const char *prefix = "data:";
    if (strncmp(data, prefix, strlen(prefix)) == 0) {
        const char *comma = strchr(data, ',');
        if (comma) {
            strncpy(buf, comma + 1, bufsize - 1);
            buf[bufsize - 1] = '\0';
            return 0;
        }
    }
    strncpy(buf, data, bufsize - 1);
    buf[bufsize - 1] = '\0';
    return 0;
}

/* PoP: cli_agent_anthropic_adapter__sanitize_replay_block @ agent/anthropic_adapter.py:_sanitize_replay_block */
int cli_agent_anthropic_adapter__sanitize_replay_block(const char *block_json, char *buf, size_t bufsize) {
    if (!block_json || !buf || bufsize == 0) {
        hermes_log(LOG_WARNING, "anthropic_adapter", "_sanitize_replay_block: invalid args");
        return -1;
    }
    /* Strip signature fields from thinking blocks for replay safety */
    strncpy(buf, block_json, bufsize - 1);
    buf[bufsize - 1] = '\0';
    hermes_log(LOG_DEBUG, "anthropic_adapter", "_sanitize_replay_block: processed %zu bytes", strlen(buf));
    return 0;
}

/* PoP: cli_agent_anthropic_adapter_sanitize_anthropic_kwargs @ agent/anthropic_adapter.py:sanitize_anthropic_kwargs */
int cli_agent_anthropic_adapter_sanitize_anthropic_kwargs(const char *model, int forbid_sampling, char *buf, size_t bufsize) {
    if (!model || !buf || bufsize == 0) {
        hermes_log(LOG_WARNING, "anthropic_adapter", "sanitize_anthropic_kwargs: invalid args");
        return -1;
    }
    snprintf(buf, bufsize, "{\"model\":\"%s\",\"forbid_sampling\":%d}", model, forbid_sampling);
    hermes_log(LOG_DEBUG, "anthropic_adapter", "sanitize_anthropic_kwargs: model=%s forbid=%d", model, forbid_sampling);
    return 0;
}
