/*
 * gw_approval.c -- extracted from gateway/server.c monolith.
 * Real implementation of one gateway-lifecycle concern. Public
 * gw_* protos stay in include/hermes_gateway.h; promoted cross-
 * module statics are in include/gw_server_internals.h.
 */

#include "hermes.h"
#include "hermes_agent.h"
#include "hermes_gateway.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include "gateway_helpers.h"
#include "hermes_skill_commands.h"
#include "hermes_logger.h"
#include "hermes_telegram_filter.h"
#include "gw_server_internals.h"
#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <strings.h>
#include <time.h>
#include <ctype.h>
#include <sys/stat.h>
#include <dirent.h>

void gw_clarify_set_poll(char *(*fn)(const char *chat_id), int interval_sec) {
    g_gw_clarify.poll_fn = fn;
    g_gw_clarify.poll_interval = interval_sec > 0 ? interval_sec : 1;
}

void gw_clarify_begin(const char *platform, const char *chat_id,
                      const char *session_key, const char *clarify_id,
                      const char (*choices)[256], int n_choices) {
    pthread_mutex_lock(&g_gw_clarify.mutex);
    g_gw_clarify.pending = true;
    g_gw_clarify.response[0] = '\0';
    snprintf(g_gw_clarify.platform, sizeof(g_gw_clarify.platform), "%s", platform ? platform : "");
    snprintf(g_gw_clarify.chat_id, sizeof(g_gw_clarify.chat_id), "%s", chat_id ? chat_id : "");
    snprintf(g_gw_clarify.session_key, sizeof(g_gw_clarify.session_key), "%s", session_key ? session_key : "");
    snprintf(g_gw_clarify.clarify_id, sizeof(g_gw_clarify.clarify_id), "%s", clarify_id ? clarify_id : "");
    g_gw_clarify.n_choices = 0;
    g_gw_clarify.has_choices = (choices != NULL && n_choices > 0);
    if (choices && n_choices > 0) {
        for (int i = 0; i < n_choices && i < 4; i++) {
            snprintf(g_gw_clarify.choices[i], sizeof(g_gw_clarify.choices[i]), "%s", choices[i]);
            g_gw_clarify.n_choices = i + 1;
        }
    }
    pthread_mutex_unlock(&g_gw_clarify.mutex);
}

bool gw_clarify_match(const char *platform, const char *chat_id, const char *text) {
    if (!platform || !chat_id || !text) return false;
    if (!g_gw_clarify.pending) return false;
    if (strcmp(g_gw_clarify.platform, platform) != 0 ||
        strcmp(g_gw_clarify.chat_id, chat_id) != 0) return false;

    const char *trimmed = text;
    while (*trimmed == ' ' || *trimmed == '\t') trimmed++;

    /* If we have choices, accept a number (1-4) as a choice selection */
    if (g_gw_clarify.has_choices && trimmed[0] >= '1' && trimmed[0] <= '4' &&
        (trimmed[1] == '\0' || trimmed[1] == ' ' || trimmed[1] == '\t')) {
        int idx = trimmed[0] - '1';
        if (idx >= 0 && idx < g_gw_clarify.n_choices) {
            snprintf(g_gw_clarify.response, sizeof(g_gw_clarify.response), "%s", g_gw_clarify.choices[idx]);
            g_gw_clarify.pending = false;
            pthread_cond_signal(&g_gw_clarify.cond);
            printf("[gateway] Clarify response from %s/%s: %s (choice %d)\n", platform, chat_id, g_gw_clarify.response, idx + 1);
            return true;
        }
    }

    /* Any non-empty text is a valid clarify response (open-ended or free-form) */
    snprintf(g_gw_clarify.response, sizeof(g_gw_clarify.response), "%s", trimmed);
    g_gw_clarify.pending = false;
    pthread_cond_signal(&g_gw_clarify.cond);
    printf("[gateway] Clarify response from %s/%s: %.80s\n", platform, chat_id, trimmed);
    return true;
}

bool gw_clarify_check_response(const char *platform, const char *chat_id,
                                        const char *text) {
    pthread_mutex_lock(&g_gw_clarify.mutex);
    if (!g_gw_clarify.pending) {
        pthread_mutex_unlock(&g_gw_clarify.mutex);
        return false;
    }
    if (strcmp(g_gw_clarify.platform, platform) != 0 ||
        strcmp(g_gw_clarify.chat_id, chat_id) != 0) {
        pthread_mutex_unlock(&g_gw_clarify.mutex);
        return false;
    }
    bool consumed = gw_clarify_match(platform, chat_id, text);
    pthread_mutex_unlock(&g_gw_clarify.mutex);
    return consumed;
}

void gw_approval_set_poll(char *(*fn)(const char *chat_id), int interval_sec) {
    g_gw_approval.poll_fn = fn;
    g_gw_approval.poll_interval = interval_sec > 0 ? interval_sec : 1;
}

void gw_approval_set_context(const char *platform, const char *chat_id) {
    pthread_mutex_lock(&g_gw_approval.mutex);
    snprintf(g_gw_approval.platform, sizeof(g_gw_approval.platform), "%s", platform ? platform : "");
    snprintf(g_gw_approval.chat_id, sizeof(g_gw_approval.chat_id), "%s", chat_id ? chat_id : "");
    pthread_mutex_unlock(&g_gw_approval.mutex);
}

void gw_approval_begin(const char *platform, const char *chat_id) {
    pthread_mutex_lock(&g_gw_approval.mutex);
    g_gw_approval.pending = true;
    g_gw_approval.response[0] = '\0';
    snprintf(g_gw_approval.platform, sizeof(g_gw_approval.platform), "%s", platform ? platform : "");
    snprintf(g_gw_approval.chat_id, sizeof(g_gw_approval.chat_id), "%s", chat_id ? chat_id : "");
    pthread_mutex_unlock(&g_gw_approval.mutex);
}

bool gw_approval_match(const char *platform, const char *chat_id, const char *text) {
    if (!platform || !chat_id || !text) return false;

    const char *trimmed = text;
    while (*trimmed == ' ' || *trimmed == '\t') trimmed++;

    /* Short responses only: y, n, a, yes, no, always */
    if (strlen(trimmed) > 16) return false;

    char lower[64];
    snprintf(lower, sizeof(lower), "%s", trimmed);
    for (char *p = lower; *p; p++) *p = (char)tolower((unsigned char)*p);

    /* Only consume if it looks like an approval response */
    if (lower[0] != 'y' && lower[0] != 'n' && lower[0] != 'a') return false;

    snprintf(g_gw_approval.response, sizeof(g_gw_approval.response), "%s", lower);
    g_gw_approval.pending = false;
    pthread_cond_signal(&g_gw_approval.cond);
    printf("[gateway] Approval response from %s/%s: %s\n", platform, chat_id, trimmed);
    return true;
}

bool gw_approval_check_response(const char *platform, const char *chat_id,
                                         const char *text) {
    pthread_mutex_lock(&g_gw_approval.mutex);
    if (!g_gw_approval.pending) {
        pthread_mutex_unlock(&g_gw_approval.mutex);
        return false;
    }
    if (strcmp(g_gw_approval.platform, platform) != 0 ||
        strcmp(g_gw_approval.chat_id, chat_id) != 0) {
        pthread_mutex_unlock(&g_gw_approval.mutex);
        return false;
    }
    bool consumed = gw_approval_match(platform, chat_id, text);
    pthread_mutex_unlock(&g_gw_approval.mutex);
    return consumed;
}
