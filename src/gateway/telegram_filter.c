/* telegram_filter.c — Telegram message filtering implementation
 * Port of Python TelegramAdapter message gating logic.
 * Checks allowed_chats, topics, threads, mention patterns, guest mode, etc.
 */

#include "hermes_telegram_filter.h"
#include "hermes_core_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>  /* POSIX regex for mention_patterns */

/* ── Cached compiled regex patterns ──────────────────────────── */

#define TG_MAX_PATTERNS 32
static regex_t tg_compiled_patterns[TG_MAX_PATTERNS];
static int tg_pattern_count = 0;
static bool tg_patterns_initialized = false;

/* ════════════════════════════════════════════════════════════════
 *  Helper: tokenize comma-separated string
 * ════════════════════════════════════════════════════════════════ */

/* Returns true if the given string value is in a comma-separated list. */
static bool in_comma_list(const char *value, const char *list) {
    if (!value || !*value || !list || !*list) return false;

    size_t vlen = strlen(value);
    const char *p = list;
    while (*p) {
        /* Skip leading whitespace */
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;

        /* Find end of this token */
        const char *start = p;
        while (*p && *p != ',') p++;
        const char *end = p;

        /* Trim trailing whitespace */
        while (end > start && (*(end-1) == ' ' || *(end-1) == '\t')) end--;

        size_t tlen = (size_t)(end - start);
        if (tlen == vlen && strncmp(start, value, tlen) == 0) return true;

        if (*p == ',') p++;
    }
    return false;
}

/* ════════════════════════════════════════════════════════════════
 *  Config accessor helpers
 * ════════════════════════════════════════════════════════════════ */

const hermes_platform_cfg_t *tg_get_config(void) {
    return hermes_config_get_platform("telegram");
}

/* ════════════════════════════════════════════════════════════════
 *  Chat/topic/thread filtering
 * ════════════════════════════════════════════════════════════════ */

bool tg_chat_in_list(const char *chat_id, const char *comma_list) {
    if (!comma_list || !*comma_list) return false;
    return in_comma_list(chat_id, comma_list);
}

bool tg_thread_in_list(int thread_id, const char *comma_list) {
    if (!comma_list || !*comma_list) return false;
    /* Convert thread_id to string for comparison */
    char tid_str[16];
    snprintf(tid_str, sizeof(tid_str), "%d", thread_id);
    return in_comma_list(tid_str, comma_list);
}

bool tg_chat_is_allowed(const char *chat_id) {
    const hermes_platform_cfg_t *cfg = tg_get_config();
    if (!cfg) return true; /* No config = no restriction */
    if (!cfg->allowed_chats[0]) return true; /* Empty = no restriction */
    return in_comma_list(chat_id, cfg->allowed_chats);
}

bool tg_chat_is_free_response(const char *chat_id) {
    const hermes_platform_cfg_t *cfg = tg_get_config();
    if (!cfg || !cfg->free_response_chats[0]) return false;
    return in_comma_list(chat_id, cfg->free_response_chats);
}

bool tg_topic_is_allowed(const char *topic_id) {
    const hermes_platform_cfg_t *cfg = tg_get_config();
    if (!cfg || !cfg->allowed_topics[0]) return true; /* Empty = no restriction */

    /* General topic (NULL or "1" in Telegram) maps to "1" */
    const char *tid = topic_id;
    char general_buf[8] = "1";
    if (!tid || !*tid) tid = general_buf;

    return in_comma_list(tid, cfg->allowed_topics);
}

bool tg_thread_is_ignored(int thread_id) {
    const hermes_platform_cfg_t *cfg = tg_get_config();
    if (!cfg || !cfg->ignored_threads[0]) return false;
    return tg_thread_in_list(thread_id, cfg->ignored_threads);
}

/* ════════════════════════════════════════════════════════════════
 *  Boolean config accessors
 * ════════════════════════════════════════════════════════════════ */

bool tg_require_mention(void) {
    const hermes_platform_cfg_t *cfg = tg_get_config();
    return cfg ? cfg->require_mention : false;
}

bool tg_exclusive_bot_mentions(void) {
    const hermes_platform_cfg_t *cfg = tg_get_config();
    return cfg ? cfg->exclusive_bot_mentions : true;
}

bool tg_guest_mode(void) {
    const hermes_platform_cfg_t *cfg = tg_get_config();
    return cfg ? cfg->guest_mode : false;
}

bool tg_observe_unmentioned(void) {
    const hermes_platform_cfg_t *cfg = tg_get_config();
    return cfg ? cfg->observe_unmentioned : false;
}

bool tg_chat_is_group_allowed(const char *chat_id) {
    const hermes_platform_cfg_t *cfg = tg_get_config();
    if (!cfg) return true;
    if (cfg->group_allowed_chats[0]) {
        return in_comma_list(chat_id, cfg->group_allowed_chats);
    }
    /* Fall back to allowed_chats */
    if (cfg->allowed_chats[0]) {
        return in_comma_list(chat_id, cfg->allowed_chats);
    }
    return false;
}

bool tg_chat_is_observe_allowed(const char *chat_id) {
    const hermes_platform_cfg_t *cfg = tg_get_config();
    if (!cfg || !cfg->observe_allowed_chats[0]) return false;
    return in_comma_list(chat_id, cfg->observe_allowed_chats);
}

/* ════════════════════════════════════════════════════════════════
 *  Mention pattern matching (regex)
 * ════════════════════════════════════════════════════════════════ */

/* Parse and compile mention_patterns config string into regex_t array.
 * Supports JSON array format and line-separated/commas. */
static void tg_init_patterns(void) {
    if (tg_patterns_initialized) return;
    tg_patterns_initialized = true;
    tg_pattern_count = 0;

    const hermes_platform_cfg_t *cfg = tg_get_config();
    if (!cfg || !cfg->mention_patterns[0]) return;

    const char *src = cfg->mention_patterns;
    char buf[2048];
    snprintf(buf, sizeof(buf), "%s", src);

    /* Line-separated or comma-separated patterns */
    char *dup = strdup(buf);
    if (!dup) return;

    const char *delim = "\n";
    if (!strchr(dup, '\n')) delim = ",";

    char *saveptr = NULL;
    for (char *tok = strtok_r(dup, delim, &saveptr); tok; tok = strtok_r(NULL, delim, &saveptr)) {
        /* Strip whitespace */
        while (*tok == ' ' || *tok == '\t') tok++;
        char *end = tok + strlen(tok);
        while (end > tok && (*(end-1) == ' ' || *(end-1) == '\t')) end--;
        *end = '\0';
        if (!*tok) continue;

        if (tg_pattern_count >= TG_MAX_PATTERNS) break;
        if (regcomp(&tg_compiled_patterns[tg_pattern_count], tok, REG_EXTENDED | REG_ICASE | REG_NOSUB) == 0) {
            tg_pattern_count++;
        }
    }
    free(dup);
}

bool tg_mention_patterns_loaded(void) {
    if (!tg_patterns_initialized) tg_init_patterns();
    return tg_pattern_count > 0;
}

bool tg_text_matches_mention_patterns(const char *text) {
    if (!text || !*text) return false;
    if (!tg_mention_patterns_loaded()) return false;

    for (int i = 0; i < tg_pattern_count; i++) {
        if (regexec(&tg_compiled_patterns[i], text, 0, NULL, 0) == 0) {
            return true;
        }
    }
    return false;
}

/* ════════════════════════════════════════════════════════════════
 *  Multi-bot exclusive mention routing
 * ════════════════════════════════════════════════════════════════ */

int tg_extract_bot_handles(const char *text, const char ***out_handles) {
    if (!text || !out_handles) return 0;
    *out_handles = NULL;

    /* Pattern: @<name> where name is 2-29 alphanum/underscore ending in "bot" */
    regex_t re;
    int rc = regcomp(&re, "@([a-zA-Z0-9_]{2,29}bot)\\b", REG_EXTENDED | REG_ICASE);
    if (rc != 0) return 0;

    regmatch_t matches[2];
    const char *scan = text;
    int count = 0;
    int capacity = 8;
    char **handles = (char **)calloc((size_t)capacity, sizeof(char *));
    if (!handles) { regfree(&re); return 0; }

    while (regexec(&re, scan, 2, matches, 0) == 0 && count < 64) {
        if (matches[1].rm_so >= 0) {
            size_t len = (size_t)(matches[1].rm_eo - matches[1].rm_so);
            char *h = (char *)malloc(len + 1);
            if (h) {
                memcpy(h, scan + matches[1].rm_so, len);
                h[len] = '\0';
                /* Lowercase for comparison */
                for (char *p = h; *p; p++) *p = (char)tolower((unsigned char)*p);

                if (count >= capacity) {
                    capacity *= 2;
                    char **newh = (char **)realloc(handles, (size_t)capacity * sizeof(char *));
                    if (!newh) { free(h); break; }
                    handles = newh;
                }
                handles[count++] = h;
            }
        }
        scan += (size_t)(matches[0].rm_eo > 0 ? matches[0].rm_eo : 1);
    }
    regfree(&re);

    if (count == 0) { free(handles); return 0; }
    *out_handles = handles;
    return count;
}

bool tg_explicit_other_bot_mention(const char *text, const char *bot_username) {
    if (!text || !bot_username) return false;

    const char **handles = NULL;
    int n = tg_extract_bot_handles(text, &handles);
    if (n <= 0) return false;

    bool found_self = false;
    for (int i = 0; i < n; i++) {
        if (strcmp(handles[i], bot_username) == 0) {
            found_self = true;
        }
        free((char *)handles[i]);
    }
    free(handles);

    /* If we found bot handles but none match this bot, it's an exclusive mention of other bots */
    return !found_self;
}

/* ════════════════════════════════════════════════════════════════
 *  Main filtering decision — should_process_message
 * ════════════════════════════════════════════════════════════════ */

bool tg_should_process_message(const char *chat_type, const char *chat_id,
                                const char *text, const char *thread_id,
                                bool is_group, bool is_mentioned,
                                bool is_reply_to_bot,
                                const char *bot_username)
{
    (void)text;

    /* DMs are always processed */
    if (!is_group) return true;

    if (!chat_id) return false;

    /* Thread filtering: check ignored_threads */
    if (thread_id) {
        int tid = atoi(thread_id);
        if (tg_thread_is_ignored(tid)) return false;
    } else {
        /* General topic = thread_id 1 */
        if (tg_thread_is_ignored(1)) return false;
    }

    /* Topic filtering: check allowed_topics */
    if (!tg_topic_is_allowed(thread_id)) return false;

    /* Exclusive bot mentions: if other bots are @mentioned but not this one, skip */
    if (tg_exclusive_bot_mentions() && bot_username && bot_username[0]) {
        if (tg_explicit_other_bot_mention(text ? text : "", bot_username)) {
            return false;
        }
    }

    /* Obverserve-unmentioned check: if we shouldn't even observe, check chat allowlists */
    if (!tg_observe_unmentioned()) {
        /* When not observing, check allowed_chats gate */
        if (!tg_chat_is_allowed(chat_id)) {
            /* Guest mode bypass: allow if explicitly @mentioned */
            if (tg_guest_mode() && is_mentioned) {
                return true;
            }
            return false;
        }
    }

    /* If message was observed by _should_observe, it shouldn't be processed here */
    if (tg_observe_unmentioned()) {
        /* Only pass through messages that would NOT be observed.
         * Observed messages are: groups with require_mention enabled,
         * no @mention, no reply, no mention_patterns match, no free_response. */
        if (tg_chat_is_free_response(chat_id)) {
            /* Free response chats always come through as normal messages */
            return true;
        }
        if (!tg_require_mention()) {
            /* No mention required = all messages are processed */
            return true;
        }
        /* Now checking: if any of these are true, it's a triggered message */
        if (is_reply_to_bot) return true;
        if (is_mentioned) return true;
        if (tg_text_matches_mention_patterns(text ? text : "")) return true;

        /* Otherwise, it would be observed — don't process */
        return false;
    }

    /* Standard gating when observe is off:
     * Process if: free_response, require_mention_off, reply, @mention, or pattern match */
    if (tg_chat_is_free_response(chat_id)) return true;
    if (!tg_require_mention()) return true;
    if (is_reply_to_bot) return true;
    if (is_mentioned) return true;
    if (tg_text_matches_mention_patterns(text ? text : "")) return true;

    return false;
}

/* ════════════════════════════════════════════════════════════════
 *  should_observe_message
 * ════════════════════════════════════════════════════════════════ */

bool tg_should_observe_message(const char *chat_type, const char *chat_id,
                                const char *text, const char *thread_id,
                                bool is_group, bool is_mentioned,
                                bool is_reply_to_bot,
                                const char *bot_username)
{
    if (!tg_observe_unmentioned()) return false;
    if (!is_group) return false;

    if (!chat_id) return false;

    /* Topic and thread filtering */
    if (thread_id) {
        int tid = atoi(thread_id);
        if (tg_thread_is_ignored(tid)) return false;
    } else {
        if (tg_thread_is_ignored(1)) return false;
    }
    if (!tg_topic_is_allowed(thread_id)) return false;

    /* Exclusive bot mentions: if other bots mentioned but not this one, skip observe */
    if (tg_exclusive_bot_mentions() && bot_username && bot_username[0]) {
        if (tg_explicit_other_bot_mention(text ? text : "", bot_username)) {
            return false;
        }
    }

    /* Check observe_allowed_chats */
    if (!tg_chat_is_observe_allowed(chat_id)) return false;

    /* Free response chats are normal messages, not observed */
    if (tg_chat_is_free_response(chat_id)) return false;

    /* Only observe messages skipped by require_mention gate */
    if (!tg_require_mention()) return false;
    if (is_reply_to_bot) return false;
    if (is_mentioned) return false;
    if (tg_text_matches_mention_patterns(text ? text : "")) return false;

    return true;
}
