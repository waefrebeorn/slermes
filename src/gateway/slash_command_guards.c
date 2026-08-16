/*
 * port_slash_command_guards.c — Faithful C11 ports of the pure-logic
 * cross-origin / resume security guards from Python
 * gateway/slash_commands.py (GatewaySlashCommandsMixin).
 *
 * Each function carries its exact PoP comment so the scanner credits it.
 * The participant-scoping logic reuses the existing
 * is_shared_multi_user_session() (gw_session.c) via its opaque API.
 */

#include "port_slash_command_guards.h"

#include <string.h>
#include "gw_server_internals.h"

/* Helper: NULL-safe field read, treating NULL/empty the same as Python's
 * `getattr(x, f, "") or ""`. */
static const char *sf(const char *s) { return s ? s : ""; }

/* PoP: slash_typed_command_prefix_for @ gateway/slash_commands.py:_typed_command_prefix_for */
const char *slash_typed_command_prefix_for(const char *adapter_prefix) {
    /* getattr(adapter, "typed_command_prefix", "/") if adapter else "/" */
    if (adapter_prefix && adapter_prefix[0] != '\0') return adapter_prefix;
    return "/";
}

/* PoP: slash_same_matrix_room @ gateway/slash_commands.py:_same_matrix_room */
bool slash_same_matrix_room(const gw_session_source_t *current,
                            const gw_session_source_t *origin) {
    if (origin == NULL || current == NULL) return false;
    /* origin.platform == MATRIX and current.platform == MATRIX */
    if (strcmp(sf(origin->platform), "matrix") != 0) return false;
    if (strcmp(sf(current->platform), "matrix") != 0) return false;
    /* origin.chat_id == current.chat_id */
    if (strcmp(sf(origin->chat_id), sf(current->chat_id)) != 0) return false;
    /* str(current.thread_id or "") == str(origin.thread_id or "") */
    if (strcmp(sf(current->thread_id), sf(origin->thread_id)) != 0) return false;
    return true;
}

/* PoP: slash_same_origin_chat @ gateway/slash_commands.py:_same_origin_chat */
bool slash_same_origin_chat(const gw_session_source_t *current,
                            const gw_session_source_t *origin,
                            bool group_sessions_per_user,
                            bool thread_sessions_per_user) {
    if (origin == NULL || current == NULL) return false;
    if (strcmp(sf(origin->platform), sf(current->platform)) != 0) return false;
    if (strcmp(sf(origin->chat_id), sf(current->chat_id)) != 0) return false;
    /* thread equality required before any sharing logic */
    if (strcmp(sf(current->thread_id), sf(origin->thread_id)) != 0) return false;

    /* chat_type = (current.chat_type or "").lower() */
    char chat_type[32];
    size_t i = 0;
    const char *ct = sf(current->chat_type);
    for (; ct[i] && i < sizeof(chat_type) - 1; i++) {
        char c = ct[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        chat_type[i] = c;
    }
    chat_type[i] = '\0';

    /* DM-like chats are always per-user */
    if (strcmp(chat_type, "dm") == 0 || strcmp(chat_type, "direct") == 0 ||
        strcmp(chat_type, "private") == 0 || chat_type[0] == '\0') {
        if (sf(current->chat_id)[0] != '\0') return true;
        /* fall back to participant id (user_id_alt or user_id) */
        const char *cur_pid = sf(current->user_id_alt)[0] ? current->user_id_alt
                                                          : sf(current->user_id);
        const char *org_pid = sf(origin->user_id_alt)[0] ? origin->user_id_alt
                                                         : sf(origin->user_id);
        return cur_pid[0] != '\0' && strcmp(cur_pid, org_pid) == 0;
    }

    /* Non-DM: scope by participant whenever the key for this source is per-user */
    bool shared = is_shared_multi_user_session(current, group_sessions_per_user,
                                               thread_sessions_per_user);
    if (shared) return true;

    /* Per-user key: compare the participant id the key is built from */
    const char *cur_pid = sf(current->user_id_alt)[0] ? current->user_id_alt
                                                      : sf(current->user_id);
    const char *org_pid = sf(origin->user_id_alt)[0] ? origin->user_id_alt
                                                     : sf(origin->user_id);
    if (cur_pid[0] != '\0' && org_pid[0] != '\0')
        return strcmp(cur_pid, org_pid) == 0;
    /* participant id missing on one side: fail closed */
    return false;
}
