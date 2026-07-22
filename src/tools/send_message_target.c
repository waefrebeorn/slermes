/*
 * send_message_target.c — focused extraction from tools/send_message_tool.py
 *
 * Pure, oracle-verifiable target/display helpers (no gateway/HTTP):
 *   - send_message_target_parse_target_ref  (platform target -> chat/thread)
 *   - send_message_target_display_chat_id   (log-safe id; signal group redact)
 *   - send_message_target_telegram_retry_delay (error text -> backoff seconds)
 *
 * Faithful to the LIVE Python for the platforms each implements. NOTE:
 * Python's _parse_target_ref also handles matrix/weixin/yuanbao/ntfy/email/
 * whatsapp/E.164-phone/signal-group/xmpp targets (via regexes + platform
 * modules); those branches are intentionally NOT in this C subset yet — the
 * oracle asserts only the convergent platform cases. Extending to the full
 * set needs the per-platform regex table pulled in here (future window).
 */

#ifndef SRC_TOOLS_SEND_MESSAGE_TARGET_C
#define SRC_TOOLS_SEND_MESSAGE_TARGET_C

#include "send_message_target.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <regex.h>

/* PoP: _parse_target_ref @ tools/send_message_tool.py:_parse_target_ref
 * Faithful subset for the regex-based platforms Python implements. Uses
 * POSIX ERE (capturing groups only — no (?:...) which fails to compile
 * under this glibc). g1 = chat_id, g2 = thread_id (where present). */
int send_message_target_parse_target_ref(const char *platform_name, const char *target_ref,
                                          char *chat_id_out, size_t chat_id_size,
                                          char *thread_id_out, size_t thread_id_size)
{
    if (!platform_name || !target_ref) return 0;

    /* Regex table: platform -> ERE pattern, and which groups are chat/thread.
     * Patterns mirror the LIVE Python *_TARGET_RE exactly (POSIX ERE form). */
    const char *pattern = NULL;
    int chat_g = 1, thread_g = 2;
    if (strcmp(platform_name, "telegram") == 0) {
/* PoP: target @ agent/thread_scoped_output.py:_target */
        /* Telegram topic target (digits:digits) OR @username. */
        if (target_ref[0] == '@') {
            if (chat_id_size > 0) {
                strncpy(chat_id_out, target_ref, chat_id_size - 1);
                chat_id_out[chat_id_size - 1] = '\0';
            }
            if (thread_id_out && thread_id_size > 0) thread_id_out[0] = '\0';
            return 1;
        }
        pattern = "^[ \t]*(-?[0-9]+):?([0-9]+)?[ \t]*$";
        chat_g = 1; thread_g = 2;
    } else if (strcmp(platform_name, "discord") == 0) {
        pattern = "^[ \t]*(-?[0-9]+):?([0-9]+)?[ \t]*$";
        chat_g = 1; thread_g = 2;
    } else if (strcmp(platform_name, "feishu") == 0) {
        pattern = "^[ \t]*([A-Za-z]+_[-A-Za-z0-9]+)(:([-A-Za-z0-9_]+))?[ \t]*$";
        chat_g = 1; thread_g = 3;
    } else if (strcmp(platform_name, "slack") == 0) {
        /* Slack thread target (channel:thread_ts) OR bare channel. */
        pattern = "^[ \t]*([CGD][A-Z0-9]{8,}):([^ \t:]+)[ \t]*$";
        chat_g = 1; thread_g = 2;
    } else {
        return 0;  /* platform not in this C subset (matrix/weixin/etc.) */
    }

    regex_t re;
    if (regcomp(&re, pattern, REG_EXTENDED | REG_NOSUB) != 0) return 0;
    int rc = regexec(&re, target_ref, 0, NULL, 0);
    regfree(&re);
    if (rc != 0) {
        /* For slack, fall back to bare-channel pattern (no thread). */
        if (strcmp(platform_name, "slack") == 0) {
            regex_t re2;
            if (regcomp(&re2, "^[ \t]*([CGDU][A-Z0-9]{8,})[ \t]*$", REG_EXTENDED) != 0) return 0;
            int rc2 = regexec(&re2, target_ref, 0, NULL, 0);
            regfree(&re2);
            if (rc2 == 0) {
                if (chat_id_size > 0) {
                    strncpy(chat_id_out, target_ref, chat_id_size - 1);
                    chat_id_out[chat_id_size - 1] = '\0';
                }
                if (thread_id_out && thread_id_size > 0) thread_id_out[0] = '\0';
                return 1;
            }
        }
        return 0;
    }

    /* Matched. For platforms where the optional thread group may be absent
     * (telegram/discord/feishu), re-run with subexpression capture to read
     * the actual groups. */
    regex_t re_c;
    if (regcomp(&re_c, pattern, REG_EXTENDED) != 0) return 0;
    regmatch_t pm[4] = {0};
    if (regexec(&re_c, target_ref, 4, pm, 0) == 0) {
        if (pm[chat_g].rm_so >= 0 && chat_id_size > 0) {
            size_t len = pm[chat_g].rm_eo - pm[chat_g].rm_so;
            if (len >= chat_id_size) len = chat_id_size - 1;
            memcpy(chat_id_out, target_ref + pm[chat_g].rm_so, len);
            chat_id_out[len] = '\0';
        }
        if (thread_id_out && thread_id_size > 0) {
            if (pm[thread_g].rm_so >= 0) {
                size_t len = pm[thread_g].rm_eo - pm[thread_g].rm_so;
                if (len >= thread_id_size) len = thread_id_size - 1;
                memcpy(thread_id_out, target_ref + pm[thread_g].rm_so, len);
                thread_id_out[len] = '\0';
            } else {
                thread_id_out[0] = '\0';
            }
        }
        regfree(&re_c);
        return 1;
    }
    regfree(&re_c);
    return 0;
}

/* PoP: _display_chat_id @ tools/send_message_tool.py:_display_chat_id
 * Log-safe id: redact signal group ids to group:***. */
char *send_message_target_display_chat_id(const char *platform_name, const char *chat_id)
{
    if (!platform_name) return strdup("unknown:unknown");
    if (strcmp(platform_name, "signal") == 0 && chat_id && strncmp(chat_id, "group:", 6) == 0) {
        return strdup("group:***");
    }
    return strdup(chat_id ? chat_id : "default");
}

/* PoP: _telegram_retry_delay @ tools/send_message_tool.py:_telegram_retry_delay
 * Backoff seconds for retryable Telegram API errors; -1 / NULL = no retry.
 * NOTE: Python reads `retry_after` from the exception ATTRIBUTE, not the
 * error text. A plain error string has no such attribute, so we do NOT
 * parse `retry_after=` out of the text (that would be a fabrication); we
 * only apply the retryable-error-text heuristics. */
double send_message_target_telegram_retry_delay(const char *error_text, int attempt)
{
    if (!error_text) return -1;

    char *text = strdup(error_text);
    if (!text) return -1;
    for (char *p = text; *p; p++) *p = (char)tolower((unsigned char)*p);

    if (strstr(text, "timed out") || strstr(text, "timeout")) {
        free(text);
        return -1;
    }

    double delay = -1;
    if (strstr(text, "bad gateway") || strstr(text, "502") ||
        strstr(text, "too many requests") || strstr(text, "429") ||
        strstr(text, "service unavailable") || strstr(text, "503") ||
        strstr(text, "gateway timeout") || strstr(text, "504")) {
        delay = pow(2.0, attempt);
    }

    free(text);
    return delay;
}

#endif /* SRC_TOOLS_SEND_MESSAGE_TARGET_C */
