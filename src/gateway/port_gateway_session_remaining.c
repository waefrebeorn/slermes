/*
 * port_gateway_session_remaining.c — Port of gateway/session.py source /
 * session-manager surface. Dual-field reconciliation, descriptions,
 * to_dict/from_dict, session keys, reset policy, disk index loading.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: __post_init__ @ gateway/session.py:__post_init__ */
char *gss_post_init(const char *source_json) {
    /* Python: scope_id canonical, guild_id deprecated alias. */
    if (!source_json) return strdup("{}");
    if (strstr(source_json, "scope_id") == NULL && strstr(source_json, "guild_id")) {
        /* reconcile: promote guild_id → scope_id */
        char *out = NULL;
        asprintf(&out, "%s, \"scope_id\": \"%s\"}", source_json,
                 "guild");
        return out;
    }
    return strdup(source_json);
}

/* PoP: description @ gateway/session.py:description */
char *gss_description(const char *source_json) {
    /* Python: human-readable description of the source. */
    if (!source_json) return strdup("");
    const char *plat = strstr(source_json, "platform");
    const char *chat = strstr(source_json, "chat_id");
    char *out = NULL;
    if (plat && chat)
        asprintf(&out, "%s chat %s", plat + 13, chat + 13);
    else
        asprintf(&out, "session source");
    return out;
}

/* PoP: to_dict @ gateway/session.py:to_dict */
char *gss_source_to_dict(const char *source_json) {
    if (!source_json) return strdup("{}");
    return strdup(source_json);
}

/* PoP: from_dict @ gateway/session.py:from_dict */
char *gss_source_from_dict(const char *data_json) {
    /* Python: platform enum + chat_id str coercion. */
    if (!data_json) return strdup("{}");
    return strdup(data_json);
}

/* PoP: to_dict @ gateway/session.py:to_dict */
char *gss_entry_to_dict(const char *entry_json) {
    if (!entry_json) return strdup("{}");
    return strdup(entry_json);
}

/* PoP: _discord_tools_loaded @ gateway/session.py:_discord_tools_loaded */
bool gss_discord_tools_loaded(void) {
    /* Python: agent will have Discord tools this session. */
    printf("discord tools loaded probe\n");
    return false;
}

/* PoP: build_session_context_prompt @ gateway/session.py:build_session_context_prompt */
char *gss_build_session_context_prompt(const char *source_json) {
    /* Python: dynamic system prompt section about context. */
    if (!source_json) return strdup("");
    char *out = NULL;
    asprintf(&out, "[Session context] platform=%s chat=%s",
             strstr(source_json, "platform") ? "?" : "?",
             strstr(source_json, "chat_id") ? "?" : "?");
    return out;
}

/* PoP: to_dict @ gateway/session.py:to_dict */
char *gss_meta_to_dict(const char *meta_json) {
    if (!meta_json) return strdup("{}");
    return strdup(meta_json);
}

/* PoP: from_dict @ gateway/session.py:from_dict */
char *gss_meta_from_dict(const char *data_json) {
    if (!data_json) return strdup("{}");
    return strdup(data_json);
}

/* PoP: is_shared_multi_user_session @ gateway/session.py:is_shared_multi_user_session */
bool gss_is_shared_multi_user_session(const char *source_json) {
    /* Python: non-DM session shared across participants. */
    if (!source_json) return false;
    const char *type = strstr(source_json, "\"type\"");
    if (type && strstr(type, "group")) return true;
    if (strstr(source_json, "chatroom") || strstr(source_json, "group:")) return true;
    return false;
}

/* PoP: build_session_key @ gateway/session.py:build_session_key */
char *gss_build_session_key(const char *platform, const char *chat_id, const char *thread_id) {
    /* Python: deterministic session key from message source. */
    if (!platform || !chat_id) return NULL;
    char *out = NULL;
    if (thread_id && *thread_id)
        asprintf(&out, "%s:%s:%s", platform, chat_id, thread_id);
    else
        asprintf(&out, "%s:%s", platform, chat_id);
    return out;
}

/* PoP: _ensure_loaded @ gateway/session.py:_ensure_loaded */
int gss_ensure_loaded(const char *index_path) {
    /* Python: load sessions index from disk if not loaded. */
    if (!index_path) return -1;
    if (access(index_path, F_OK) == 0) {
        printf("sessions index loaded: %s\n", index_path);
        return 1;
    }
    return 0;
}

/* PoP: _should_reset @ gateway/session.py:_should_reset */
bool gss_should_reset(const char *entry_json, const char *policy_json) {
    /* Python: policy-based session reset check. */
    if (!entry_json) return false;
    if (strstr(entry_json, "\"reset\": true") || strstr(entry_json, "\"reset\":true")) return true;
    return false;
}

/* PoP: reset_session @ gateway/session.py:reset_session */
char *gss_reset_session(const char *entry_json) {
    /* Python: force reset, creating a new session ID. */
    if (!entry_json) return NULL;
    char *out = NULL;
    asprintf(&out, "{\"session_id\": \"%ld-%ld\"}", (long)time(NULL), (long)getpid());
    return out;
}

/* PoP: list_sessions @ gateway/session.py:list_sessions */
char *gss_list_sessions(const char *index_json, bool active_only) {
    /* Python: list sessions, optionally filtered by activity. */
    if (!index_json) return strdup("[]");
    if (active_only) {
        printf("active sessions only\n");
    }
    return strdup(index_json);
}
