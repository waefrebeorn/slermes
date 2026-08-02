/*
 * port_whatsapp_common_helpers.c — C port of gateway/platforms/whatsapp_common.py
 *
 * Pure-logic helpers for WhatsApp platform: text sanitization, chunk limits,
 * allow-list processing, mention detection, bot ID extraction, message routing.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <strings.h>
#include <sys/stat.h>

#include "hermes_json.h"
#include "hermes_logger.h"

/* PoP: _sanitize_outbound_text @ gateway/platforms/whatsapp_common.py:_sanitize_outbound_text */
char *whatsapp_common_sanitize_outbound_text(const char *text) {
    if (!text) return strdup("");
    size_t n = strlen(text);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)text[i];
        if (c < 0x20 && c != '\n' && c != '\t') continue;
        if (c == 0x7F) continue;
        out[j++] = text[i];
    }
    out[j] = '\0';
    return out;
}

/* PoP: _effective_reply_prefix @ gateway/platforms/whatsapp_common.py:_effective_reply_prefix */
const char *whatsapp_common_effective_reply_prefix(const char *config_prefix) {
    return config_prefix && config_prefix[0] ? config_prefix : "";
}

/* PoP: _outgoing_chunk_limit @ gateway/platforms/whatsapp_common.py:_outgoing_chunk_limit */
/* PoP: whatsapp_common_outgoing_chunk_limit @ gateway/platforms/whatsapp_common.py:_outgoing_chunk_limit */
int whatsapp_common_outgoing_chunk_limit(int config_limit) {
    if (config_limit <= 0) return 4096;
    if (config_limit > 65536) return 65536;
    return config_limit;
}

/* PoP: _whatsapp_require_mention @ gateway/platforms/whatsapp_common.py:_whatsapp_require_mention */
bool whatsapp_common_require_mention(const char *chat_type) {
    if (!chat_type) return false;
    return strcasecmp(chat_type, "group") == 0 || strcasecmp(chat_type, "supergroup") == 0;
}

/* PoP: _whatsapp_free_response_chats @ gateway/platforms/whatsapp_common.py:_whatsapp_free_response_chats */
bool whatsapp_common_free_response_chats(const char *chat_id) {
    /* Python: set of chat ids from config.extra.free_response_chats or
     * WHATSAPP_FREE_RESPONSE_CHATS env (comma-split). C: membership check. */
    static char cached[2048];
    static int loaded = 0;
    if (!loaded) {
        const char *env = getenv("WHATSAPP_FREE_RESPONSE_CHATS");
        if (!env) { cached[0] = '\0'; }
        else {
            size_t n = strlen(env);
            if (n >= sizeof(cached)) n = sizeof(cached) - 1;
            memcpy(cached, env, n); cached[n] = '\0';
        }
        loaded = 1;
    }
    if (!chat_id || !*chat_id) return false;
    const char *p = cached;
    while (*p) {
        const char *comma = strchr(p, ',');
        size_t len = comma ? (size_t)(comma - p) : strlen(p);
        const char *t = p;
        while (len > 0 && (*t == ' ')) { t++; len--; }
        while (len > 0 && t[len-1] == ' ') len--;
        if (len == strlen(chat_id) && strncmp(t, chat_id, len) == 0) return true;
        p = comma ? comma + 1 : p + len;
    }
    return false;
}

/* PoP: _coerce_allow_list @ gateway/platforms/whatsapp_common.py:_coerce_allow_list */
json_t *whatsapp_common_coerce_allow_list(json_t *raw) {
    json_t *out = json_array();
    if (!raw) return out;
    if (json_array_size(raw) > 0 || (raw->type == JSON_ARRAY)) {
        size_t n = json_array_size(raw);
        for (size_t i = 0; i < n; i++) {
            json_t *item = json_array_get(raw, i);
            const char *s = json_string_value(item);
            if (s && s[0]) json_array_append(out, json_string(s));
        }
        return out;
    }
    /* Single string → wrap in array */
    const char *s = json_string_value(raw);
    if (s && s[0]) json_array_append(out, json_string(s));
    return out;
}

/* PoP: _normalize_whatsapp_id @ gateway/platforms/whatsapp_common.py:_normalize_whatsapp_id */
char *whatsapp_common_normalize_whatsapp_id(const char *id) {
    if (!id) return NULL;
    /* Strip non-digit chars, keep + prefix */
    size_t n = strlen(id);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    size_t j = 0;
    if (id[0] == '+') out[j++] = '+';
    for (size_t i = 0; i < n; i++) {
        if (id[i] >= '0' && id[i] <= '9') out[j++] = id[i];
    }
    out[j] = '\0';
    return out;
}

/* PoP: _is_broadcast_chat @ gateway/platforms/whatsapp_common.py:_is_broadcast_chat */
bool whatsapp_common_is_broadcast_chat(const char *chat_type) {
    if (!chat_type) return false;
    return strcasecmp(chat_type, "broadcast") == 0 || strcasecmp(chat_type, "status") == 0;
}

/* PoP: _matches_whatsapp_allowlist @ gateway/platforms/whatsapp_common.py:_matches_whatsapp_allowlist */
bool whatsapp_common_matches_allowlist(const char *id, json_t *allowlist) {
    if (!id || !allowlist) return false;
    size_t n = json_array_size(allowlist);
    for (size_t i = 0; i < n; i++) {
        const char *entry = json_string_value(json_array_get(allowlist, i));
        if (entry && strcmp(entry, id) == 0) return true;
    }
    return false;
}

/* PoP: _is_dm_allowed @ gateway/platforms/whatsapp_common.py:_is_dm_allowed */
bool whatsapp_common_is_dm_allowed(const char *sender_id, json_t *allowlist, bool allowlist_enabled) {
    if (!allowlist_enabled) return true;
    return whatsapp_common_matches_allowlist(sender_id, allowlist);
}

/* PoP: _is_dm_intake_allowed @ gateway/platforms/whatsapp_common.py:_is_dm_intake_allowed */
/* PoP: whatsapp_common_is_dm_intake_allowed @ gateway/platforms/qqbot/adapter.py:_is_dm_intake_allowed */
bool whatsapp_common_is_dm_intake_allowed(const char *sender_id, json_t *allowlist, bool intake_enabled) {
    if (!intake_enabled) return false;
    return whatsapp_common_matches_allowlist(sender_id, allowlist);
}

/* PoP: _is_group_allowed @ gateway/platforms/whatsapp_common.py:_is_group_allowed */
/* PoP: whatsapp_common_is_group_allowed @ gateway/platforms/qqbot/adapter.py:_is_group_allowed */
bool whatsapp_common_is_group_allowed(const char *chat_id, json_t *allowlist, bool allowlist_enabled) {
    if (!allowlist_enabled) return true;
    return whatsapp_common_matches_allowlist(chat_id, allowlist);
}

/* PoP: _compile_mention_patterns @ gateway/platforms/whatsapp_common.py:_compile_mention_patterns */
/* PoP: whatsapp_common_compile_mention_patterns @ gateway/platforms/bluebubbles.py:_compile_mention_patterns */
json_t *whatsapp_common_compile_mention_patterns(json_t *bot_names) {
    json_t *patterns = json_array();
    if (!bot_names) return patterns;
    size_t n = json_array_size(bot_names);
    for (size_t i = 0; i < n; i++) {
        const char *name = json_string_value(json_array_get(bot_names, i));
        if (name && name[0]) {
            char pattern[256];
            snprintf(pattern, sizeof(pattern), "@%s", name);
            json_array_append(patterns, json_string(pattern));
        }
    }
    return patterns;
}

/* PoP: _bot_ids_from_message @ gateway/platforms/whatsapp_common.py:_bot_ids_from_message */
json_t *whatsapp_common_bot_ids_from_message(json_t *msg) {
    json_t *ids = json_array();
    if (!msg) return ids;
    json_t *mentions = json_object_get(msg, "mentioned_ids");
    if (mentions) {
        size_t n = json_array_size(mentions);
        for (size_t i = 0; i < n; i++) json_array_append(ids, json_array_get(mentions, i));
    }
    return ids;
}

/* PoP: _message_is_reply_to_bot @ gateway/platforms/whatsapp_common.py:_message_is_reply_to_bot */
bool whatsapp_common_message_is_reply_to_bot(json_t *msg, const char *bot_id) {
    if (!msg || !bot_id) return false;
    json_t *reply = json_object_get(msg, "reply_to");
    if (!reply) return false;
    const char *sender = json_object_get_string(reply, "sender_id", NULL);
    return sender && strcmp(sender, bot_id) == 0;
}

/* PoP: _message_mentions_bot @ gateway/platforms/whatsapp_common.py:_message_mentions_bot */
bool whatsapp_common_message_mentions_bot(json_t *msg, const char *bot_id) {
    if (!msg || !bot_id) return false;
    json_t *ids = whatsapp_common_bot_ids_from_message(msg);
    bool found = whatsapp_common_matches_allowlist(bot_id, ids);
    json_free(ids);
    return found;
}

/* PoP: _message_matches_mention_patterns @ gateway/platforms/whatsapp_common.py:_message_matches_mention_patterns */
/* PoP: whatsapp_common_message_matches_mention_patterns @ gateway/platforms/bluebubbles.py:_message_matches_mention_patterns */
bool whatsapp_common_message_matches_mention_patterns(const char *text, json_t *patterns) {
    if (!text || !patterns) return false;
    size_t n = json_array_size(patterns);
    for (size_t i = 0; i < n; i++) {
        const char *p = json_string_value(json_array_get(patterns, i));
        if (p && strcasestr(text, p)) return true;
    }
    return false;
}

/* PoP: _clean_bot_mention_text @ gateway/platforms/whatsapp_common.py:_clean_bot_mention_text */
char *whatsapp_common_clean_bot_mention_text(const char *text, const char *bot_name) {
    if (!text) return strdup("");
    if (!bot_name) return strdup(text);
    char mention[256];
    snprintf(mention, sizeof(mention), "@%s", bot_name);
    char *pos = strcasestr(text, mention);
    if (!pos) return strdup(text);
    /* Remove the mention and surrounding whitespace */
    size_t prefix = pos - text;
    size_t suffix = strlen(text) - prefix - strlen(mention);
    char *out = malloc(prefix + suffix + 1);
    if (!out) return strdup(text);
    memcpy(out, text, prefix);
    const char *rest = pos + strlen(mention);
    while (*rest == ' ') rest++;
    strcpy(out + prefix, rest);
    return out;
}

/* PoP: _should_process_message @ gateway/platforms/whatsapp_common.py:_should_process_message */
bool whatsapp_common_should_process_message(json_t *msg, bool is_dm, bool require_mention,
                                             bool mentions_bot, bool is_reply_to_bot) {
    (void)msg;
    if (is_dm) return true;
    if (!require_mention) return true;
    return mentions_bot || is_reply_to_bot;
}

/* PoP: resolve_whatsapp_bridge_dir @ gateway/platforms/whatsapp_common.py:resolve_whatsapp_bridge_dir */
char *whatsapp_common_resolve_bridge_dir(void) {
    const char *home = getenv("HOME");
    if (!home) home = "/tmp";
    char path[1024];
    snprintf(path, sizeof(path), "%s/.hermes/whatsapp-bridge", home);
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) return strdup(path);
    return strdup(path);
}
