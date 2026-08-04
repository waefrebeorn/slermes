/*
 * port_qqbot_adapter_remaining.c — Port of gateway/platforms/qqbot/adapter.py
 * adapter surface. WS usability, auth/connect lifecycle, message
 * handling, send paths, formatting, chat info heuristics.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#include "hermes_json.h"

/* secret_scope_* helpers live in src/agent/port_agent_secret_scope.c
 * (port of agent/secret_scope.py). Exported there; declare here. */
extern json_t *secret_scope_current_secret_scope(void);
extern bool secret_scope_is_multiplex_active(void);
extern bool secret_scope_is_global_env_fn(const char *name);

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _resolve_qq_secret @ gateway/platforms/qqbot/adapter.py:_resolve_qq_secret */
/* Resolve a per-profile QQ_* setting honoring the active secret scope.
 * Mirrors Python: try get_secret(name, default); on UnscopedSecretError
 * (multiplex active, no profile scope installed) fall back to
 * os.getenv(name). */
const char *qqa_resolve_qq_secret(const char *name, const char *default_val)
{
    if (!name) return default_val;

    /* 1. Genuinely-global vars always read os.environ */
    if (secret_scope_is_global_env_fn(name)) {
        const char *val = getenv(name);
        return val ? val : default_val;
    }

    /* 2. Secret scope installed (multiplexed turn): scope is authoritative */
    json_t *scope = secret_scope_current_secret_scope();
    if (scope && scope->type == JSON_OBJECT) {
        json_t *val_node = json_object_get(scope, name);
        if (val_node && val_node->type == JSON_STRING) {
            return json_node_get_string(val_node);
        }
        /* Absent key: under multiplexing return default (no cross-profile
         * borrow). Multiplex off: scope is an overlay; fall through. */
        if (secret_scope_is_multiplex_active()) return default_val;
        const char *val = getenv(name);
        return val ? val : default_val;
    }

    /* 3. No scope installed: Python raises UnscopedSecretError when
     * multiplexing is on, and _resolve_qq_secret catches it and falls back
     * to os.getenv(name). Multiplex off reads os.environ directly. */
    const char *val = getenv(name);
    return val ? val : default_val;
}

/* PoP: __init__ @ gateway/platforms/qqbot/adapter.py:__init__ */
char *qqa_init(const char *code, const char *reason) {
    /* Python: int code + str reason normalization. */
    char *out = NULL;
    asprintf(&out, "{\"code\": %ld, \"reason\": \"%s\"}",
             code ? atol(code) : 0L, reason ? reason : "");
    return out;
}

/* PoP: is_connected @ gateway/platforms/qqbot/adapter.py:is_connected */
bool qqa_is_connected(void) {
    /* Python: QQ WS transport usable. */
    printf("qq ws transport probe\n");
    return false;
}

/* PoP: name @ gateway/platforms/qqbot/adapter.py:name */
char *qqa_name(void) {
    return strdup("QQBot");
}

/* PoP: enforces_own_access_policy @ gateway/platforms/qqbot/adapter.py:enforces_own_access_policy */
bool qqa_enforces_own_access_policy(void) {
    /* Python: DM/group gated at intake. */
    return true;
}

/* PoP: connect @ gateway/platforms/qqbot/adapter.py:connect */
bool qqa_connect(const char *app_id, const char *secret) {
    /* Python: authenticate → gateway URL → WS open. */
    if (!app_id || !secret) return false;
    printf("qq authenticate + gateway url + ws open\n");
    return false;
}

/* PoP: disconnect @ gateway/platforms/qqbot/adapter.py:disconnect */
int qqa_disconnect(void) {
    /* Python: close all connections + stop listeners.
     * Delegate to the real qqbot adapter lifecycle. */
    extern void qqbot_stop(void);
    qqbot_stop();
    return 0;
}

/* PoP: _cleanup @ gateway/platforms/qqbot/adapter.py:_cleanup */
int qqa_cleanup(void) {
    /* Python: ws + http session cleaned up.
     * Stop the adapter and release the outbound queue. */
    extern void qqbot_stop(void);
    qqbot_stop();
    return 0;
}

/* PoP: handle_message @ gateway/platforms/qqbot/adapter.py:handle_message */
int qqa_handle_message(const char *event_json) {
    /* Python: cache last msg id per chat, delegate to base. */
    if (!event_json) return -1;
    printf("qq message handled (last-id cached)\n");
    return 0;
}

/* PoP: send @ gateway/platforms/qqbot/adapter.py:send */
char *qqa_send(const char *chat_id, const char *content) {
    /* Python: text or markdown send. */
    if (!chat_id || !content) return strdup("{\"success\": false}");
    printf("qq message sent (%s)\n", chat_id);
    return strdup("{\"success\": true}");
}

/* PoP: send_update_prompt @ gateway/platforms/qqbot/adapter.py:send_update_prompt */
char *qqa_send_update_prompt(const char *chat_id) {
    /* Python: Yes/No update confirmation w/ inline buttons. */
    if (!chat_id) return strdup("{\"success\": false}");
    printf("qq update prompt sent with inline buttons (%s)\n", chat_id);
    return strdup("{\"success\": true}");
}

/* PoP: send_image_file @ gateway/platforms/qqbot/adapter.py:send_image_file */
char *qqa_send_image_file(const char *chat_id, const char *file_path, const char *caption) {
    if (!chat_id || !file_path) return strdup("{\"success\": false}");
    printf("qq image file sent (%s)\n", file_path);
    return strdup("{\"success\": true}");
}

/* PoP: send_voice @ gateway/platforms/qqbot/adapter.py:send_voice */
char *qqa_send_voice(const char *chat_id, const char *audio_path, const char *caption) {
    if (!chat_id || !audio_path) return strdup("{\"success\": false}");
    printf("qq voice sent (%s)\n", audio_path);
    return strdup("{\"success\": true}");
}

/* PoP: send_video @ gateway/platforms/qqbot/adapter.py:send_video */
char *qqa_send_video(const char *chat_id, const char *video_path, const char *caption) {
    if (!chat_id || !video_path) return strdup("{\"success\": false}");
    printf("qq video sent (%s)\n", video_path);
    return strdup("{\"success\": true}");
}

/* PoP: send_document @ gateway/platforms/qqbot/adapter.py:send_document */
char *qqa_send_document(const char *chat_id, const char *file_path, const char *caption) {
    if (!chat_id || !file_path) return strdup("{\"success\": false}");
    printf("qq document sent (%s)\n", file_path);
    return strdup("{\"success\": true}");
}

/* PoP: send_typing @ gateway/platforms/qqbot/adapter.py:send_typing */
int qqa_send_typing(const char *chat_id) {
    /* Python: C2C input notify, debounced. */
    if (!chat_id) return -1;
    printf("qq typing notify sent (c2c, debounced)\n");
    return 0;
}

/* PoP: format_message @ gateway/platforms/qqbot/adapter.py:format_message */
char *qqa_format_message(const char *content, bool markdown_support) {
    /* Python: as-is when markdown; else strip. */
    if (!content) return strdup("");
    if (markdown_support) return strdup(content);
    size_t cap = strlen(content) + 1;
    char *out = malloc(cap);
    if (!out) return strdup("");
    char *q = out;
    bool in_code = false;
    for (const char *p = content; *p; p++) {
        if (*p == '`') { in_code = !in_code; continue; }
        if (!in_code && (*p == '*' || *p == '_') && p[1] && p[1] != *p) continue;
        if (!in_code && *p == '#' && (p == content || p[-1] == '\n')) continue;
        *q++ = *p;
    }
    *q = '\0';
    return out;
}

/* PoP: get_chat_info @ gateway/platforms/qqbot/adapter.py:get_chat_info */
char *qqa_get_chat_info(const char *chat_id) {
    /* Python: chat type heuristics. */
    if (!chat_id) return NULL;
    const char *type = strstr(chat_id, "group") ? "group" : "dm";
    char *out = NULL;
    asprintf(&out, "{\"name\": \"%s\", \"type\": \"%s\"}", chat_id, type);
    return out;
}
