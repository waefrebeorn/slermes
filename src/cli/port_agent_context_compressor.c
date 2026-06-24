/*
 * port_agent_context_compressor.c — C port of agent/context_compressor.py
 */

#include "hermes.h"
#include "hermes_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* PoP: cli_agent_context_compressor__build_static_fallback_summary @ agent/context_compressor.py:_build_static_fallback_summary */
int cli_agent_context_compressor__build_static_fallback_summary(const char **messages, int num_messages, const char *reason, char *buf, size_t bufsize) {
    if (!messages || num_messages <= 0 || !buf || bufsize == 0) {
        hermes_log(LOG_WARNING, "context_compressor", "_build_static_fallback_summary: invalid args");
        return -1;
    }
    int n = snprintf(buf, bufsize, "[FALLBACK SUMMARY] %d messages compacted.", num_messages);
    if (reason && strlen(reason) > 0) {
        int rem = (int)bufsize - n - 1;
        if (rem > 20) {
            n += snprintf(buf + n, rem, " Reason: %s", reason);
        }
    }
    hermes_log(LOG_DEBUG, "context_compressor", "_build_static_fallback_summary: %d messages", num_messages);
    return 0;
}

/* PoP: cli_agent_context_compressor__fallback_to_main_for_compression @ agent/context_compressor.py:_fallback_to_main_for_compression */
int cli_agent_context_compressor__fallback_to_main_for_compression(const char *error_msg, const char *reason) {
    if (!error_msg) {
        hermes_log(LOG_WARNING, "context_compressor", "_fallback_to_main_for_compression: NULL error_msg");
        return -1;
    }
    hermes_log(LOG_DEBUG, "context_compressor", "_fallback_to_main_for_compression: error=%s reason=%s",
               error_msg, reason ? reason : "(null)");
    return 0;
}

/* PoP: cli_agent_context_compressor__strip_summary_prefix @ agent/context_compressor.py:_strip_summary_prefix */
int cli_agent_context_compressor__strip_summary_prefix(const char *text, char *buf, size_t bufsize) {
    if (!text || !buf || bufsize == 0) {
        hermes_log(LOG_WARNING, "context_compressor", "_strip_summary_prefix: invalid args");
        return -1;
    }
    static const char *prefixes[] = {
        "[CONTEXT COMPACTION — REFERENCE ONLY]",
        "[CONTEXT SUMMARY]:", NULL
    };
    const char *src = text;
    for (int i = 0; prefixes[i]; i++) {
        size_t plen = strlen(prefixes[i]);
        if (strncmp(src, prefixes[i], plen) == 0) {
            src += plen;
            while (*src == ' ' || *src == '\n') src++;
            break;
        }
    }
    strncpy(buf, src, bufsize - 1);
    buf[bufsize - 1] = '\0';
    hermes_log(LOG_DEBUG, "context_compressor", "_strip_summary_prefix: stripped prefix");
    return 0;
}

/* PoP: cli_agent_context_compressor__with_summary_prefix @ agent/context_compressor.py:_with_summary_prefix */
int cli_agent_context_compressor__with_summary_prefix(const char *text, char *buf, size_t bufsize) {
    if (!text || !buf || bufsize == 0) {
        hermes_log(LOG_WARNING, "context_compressor", "_with_summary_prefix: invalid args");
        return -1;
    }
    static const char *prefix = "[CONTEXT COMPACTION — REFERENCE ONLY] ";
    size_t plen = strlen(prefix);
    size_t tlen = strlen(text);
    if (plen + tlen + 1 > bufsize) {
        hermes_log(LOG_WARNING, "context_compressor", "_with_summary_prefix: buffer too small");
        return -1;
    }
    memcpy(buf, prefix, plen);
    memcpy(buf + plen, text, tlen);
    buf[plen + tlen] = '\0';
    hermes_log(LOG_DEBUG, "context_compressor", "_with_summary_prefix: added prefix");
    return 0;
}

/* PoP: cli_agent_context_compressor__is_context_summary_content @ agent/context_compressor.py:_is_context_summary_content */
int cli_agent_context_compressor__is_context_summary_content(const char *text) {
    if (!text) {
        return 0;
    }
    if (strstr(text, "[CONTEXT COMPACTION") != NULL) return 1;
    if (strstr(text, "[CONTEXT SUMMARY]") != NULL) return 1;
    if (strstr(text, "CONTEXT COMPACTION — REFERENCE ONLY") != NULL) return 1;
    return 0;
}

/* PoP: cli_agent_context_compressor__has_compressed_summary_metadata @ agent/context_compressor.py:_has_compressed_summary_metadata */
int cli_agent_context_compressor__has_compressed_summary_metadata(const char *metadata) {
    if (!metadata) {
        return 0;
    }
    if (strstr(metadata, "_compressed_summary") != NULL) return 1;
    if (strstr(metadata, "is_compressed_summary") != NULL) return 1;
    return 0;
}

/* PoP: cli_agent_context_compressor__derive_auto_focus_topic @ agent/context_compressor.py:_derive_auto_focus_topic */
int cli_agent_context_compressor__derive_auto_focus_topic(const char **recent_messages, int num_messages, char *buf, size_t bufsize) {
    if (!recent_messages || num_messages <= 0 || !buf || bufsize == 0) {
        hermes_log(LOG_WARNING, "context_compressor", "_derive_auto_focus_topic: invalid args");
        return -1;
    }
    /* Use the last user message as the auto-focus topic */
    for (int i = num_messages - 1; i >= 0; i--) {
        if (recent_messages[i]) {
            strncpy(buf, recent_messages[i], bufsize - 1);
            buf[bufsize - 1] = '\0';
            hermes_log(LOG_DEBUG, "context_compressor", "_derive_auto_focus_topic: derived from message %d", i);
            return 0;
        }
    }
    buf[0] = '\0';
    return -1;
}

/* PoP: cli_agent_context_compressor__find_latest_context_summary @ agent/context_compressor.py:_find_latest_context_summary */
int cli_agent_context_compressor__find_latest_context_summary(const char **messages, int num_messages) {
    if (!messages || num_messages <= 0) {
        return -1;
    }
    for (int i = num_messages - 1; i >= 0; i--) {
        if (messages[i] && cli_agent_context_compressor__is_context_summary_content(messages[i])) {
            hermes_log(LOG_DEBUG, "context_compressor", "_find_latest_context_summary: found at index %d", i);
            return i;
        }
    }
    return -1;
}

/* PoP: cli_agent_context_compressor__find_tail_cut_by_tokens @ agent/context_compressor.py:_find_tail_cut_by_tokens */
int cli_agent_context_compressor__find_tail_cut_by_tokens(const int *token_counts, int num_messages, int budget_tokens, int summary_tokens) {
    if (!token_counts || num_messages <= 0 || budget_tokens <= 0) {
        hermes_log(LOG_WARNING, "context_compressor", "_find_tail_cut_by_tokens: invalid args");
        return 0;
    }
    int available = budget_tokens - summary_tokens;
    if (available <= 0) {
        return 0;
    }
    int total = 0;
    int cut = num_messages;
    for (int i = num_messages - 1; i >= 0; i--) {
        total += token_counts[i];
        if (total > available) {
            cut = i + 1;
            break;
        }
    }
    hermes_log(LOG_DEBUG, "context_compressor", "_find_tail_cut_by_tokens: budget=%d summary=%d cut=%d",
               budget_tokens, summary_tokens, cut);
    return cut;
}
