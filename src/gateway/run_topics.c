/*
 * run_topics.c — C port of gateway/run_topics.py (gate w6_runtopics_d5aa88).
 * PoP: w6_runtopics_d5aa88_collapse_title @ gateway/run_topics.py:_collapse_title
 * PoP: w6_runtopics_d5aa88_sanitize_telegram_topic_title @ gateway/run_topics.py:_sanitize_telegram_topic_title
 * PoP: w6_runtopics_d5aa88_sanitize_discord_thread_title @ gateway/run_topics.py:_sanitize_discord_thread_title
 * PoP: w6_runtopics_d5aa88_telegram_topic_profile_name @ gateway/run_topics.py:_telegram_topic_profile_name
 * PoP: w6_runtopics_d5aa88_telegram_topic_key @ gateway/run_topics.py:_telegram_topic_key
 * PoP: w6_runtopics_d5aa88_telegram_topic_cooldown_key @ gateway/run_topics.py:_telegram_topic_cooldown_key
 * PoP: w6_runtopics_d5aa88_is_telegram_dm @ gateway/run_topics.py:_is_telegram_dm
 * PoP: w6_runtopics_d5aa88_is_discord_auto_thread_lane @ gateway/run_topics.py:_is_discord_auto_thread_lane
 * PoP: w6_runtopics_d5aa88_is_relay_discord_channel_lane @ gateway/run_topics.py:_is_relay_discord_channel_lane
 * PoP: w6_runtopics_d5aa88_topic_restore_steps @ gateway/run_topics.py:_topic_restore_steps
 */
#include "hermes_json.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_runtopics_d5aa88.h"

#define DEFAULT_TITLE "DEFAULT_TITLE"

static char *dupstr(const char *s) { return s ? strdup(s) : NULL; }
static int nonempty(const char *s) { return s && s[0]; }

/* One UTF-8 code point; returns its byte length (1..4), 0 at end. */
static size_t next_cp(const unsigned char *p, unsigned int *cp) {
    unsigned char c = p[0];
    if (!c) return 0;
    if (c < 0x80) { *cp = c; return 1; }
    if (c < 0xE0) { *cp = ((c & 0x1F) << 6) | (p[1] & 0x3F); return 2; }
    if (c < 0xF0) { *cp = ((c & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F); return 3; }
    *cp = ((c & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
    return 4;
}
static size_t cp_count(const char *s) {
    size_t n = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (*p) { unsigned int cp; size_t k = next_cp(p, &cp); p += k ? k : 1; n++; }
    return n;
}
static size_t utf16_units(const char *s) {
    size_t n = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (*p) { unsigned int cp; size_t k = next_cp(p, &cp); p += k ? k : 1; n += cp > 0xFFFF ? 2 : 1; }
    return n;
}
/* Longest prefix of s using at most lim UTF-16 code units (no split pairs). */
static char *utf16_prefix(const char *s, size_t lim) {
    size_t used = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        unsigned int cp;
        size_t k = next_cp(p, &cp);
        size_t u = cp > 0xFFFF ? 2 : 1;
        if (used + u > lim) break;
        used += u;
        p += k;
    }
    size_t n = (size_t)(p - (const unsigned char *)s);
    char *r = (char *)malloc(n + 1);
    if (!r) return NULL;
    memcpy(r, s, n); r[n] = 0;
    return r;
}
/* First ncp code points of s (Python s[:n] counts code points). */
static char *cp_prefix(const char *s, size_t ncp) {
    size_t i = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (*p && i < ncp) { unsigned int cp; size_t k = next_cp(p, &cp); p += k ? k : 1; i++; }
    size_t n = (size_t)(p - (const unsigned char *)s);
    char *r = (char *)malloc(n + 1);
    if (!r) return NULL;
    memcpy(r, s, n); r[n] = 0;
    return r;
}
static void rstrip_ascii(char *s) {
    size_t n = strlen(s);
    while (n && isspace((unsigned char)s[n - 1])) s[--n] = 0;
}

/* re.sub(r"\s+", " ", str(title or "")).strip() or DEFAULT_TITLE */
char *w6_runtopics_d5aa88_collapse_title(const char *title) {
    if (!title) title = "";
    char *buf = (char *)malloc(strlen(title) + 2);
    if (!buf) return NULL;
    size_t n = 0;
    int in_ws = 0;
    for (const unsigned char *p = (const unsigned char *)title; *p; p++) {
        if (isspace(*p)) {
            if (!in_ws) { buf[n++] = ' '; in_ws = 1; }
        } else { buf[n++] = (char)*p; in_ws = 0; }
    }
    buf[n] = 0;
    rstrip_ascii(buf);
    /* lstrip (leading runs collapse to one space already) */
    if (buf[0] == ' ') {
        memmove(buf, buf + 1, strlen(buf));
    }
    if (!buf[0]) { free(buf); return dupstr(DEFAULT_TITLE); }
    return buf;
}

static char *sanitize(const char *title, int discord) {
    char *cleaned = w6_runtopics_d5aa88_collapse_title(title);
    if (!cleaned) return NULL;
    size_t units = discord ? utf16_units(cleaned) : cp_count(cleaned);
    if (units <= (discord ? 80u : 120u)) return cleaned;
    char *prefix = discord ? utf16_prefix(cleaned, 77) : cp_prefix(cleaned, 117);
    free(cleaned);
    if (!prefix) return NULL;
    rstrip_ascii(prefix);
    size_t n = strlen(prefix);
    char *r = (char *)malloc(n + 4);
    if (!r) { free(prefix); return NULL; }
    memcpy(r, prefix, n);
    memcpy(r + n, "...", 4);
    free(prefix);
    return r;
}
char *w6_runtopics_d5aa88_sanitize_telegram_topic_title(const char *title) {
    return sanitize(title, 0);
}
char *w6_runtopics_d5aa88_sanitize_discord_thread_title(const char *title) {
    return sanitize(title, 1);
}

char *w6_runtopics_d5aa88_telegram_topic_profile_name(const char *profile) {
    if (!profile || !profile[0]) return dupstr("default");
    /* str(profile or "").strip() or "default" */
    size_t n = strlen(profile);
    size_t a = 0, b = n;
    while (a < b && isspace((unsigned char)profile[a])) a++;
    while (b > a && isspace((unsigned char)profile[b - 1])) b--;
    if (a == b) return dupstr("default");
    char *r = (char *)malloc(b - a + 1);
    if (!r) return NULL;
    memcpy(r, profile + a, b - a); r[b - a] = 0;
    return r;
}

struct json_t *w6_runtopics_d5aa88_telegram_topic_key(const char *chat_id, const char *thread_id) {
    struct json_t *a = json_array();
    json_append(a, json_string(chat_id ? chat_id : ""));
    json_append(a, json_string(thread_id ? thread_id : ""));
    return a;
}
char *w6_runtopics_d5aa88_telegram_topic_cooldown_key(const char *profile, const char *chat_id) {
    if (!chat_id || !chat_id[0]) return NULL;
    char *p = w6_runtopics_d5aa88_telegram_topic_profile_name(profile);
    if (!p) return NULL;
    size_t n = strlen(p) + 1 + strlen(chat_id) + 1;
    char *r = (char *)malloc(n);
    if (r) snprintf(r, n, "%s:%s", p, chat_id);
    free(p);
    return r;
}
int w6_runtopics_d5aa88_is_telegram_dm(const char *platform, const char *chat_type) {
    return platform && !strcmp(platform, "telegram") && chat_type && !strcmp(chat_type, "dm");
}
int w6_runtopics_d5aa88_is_discord_auto_thread_lane(const char *platform, const char *chat_type,
        int auto_thread_created, const char *thread_id, const char *initial_name) {
    return platform && !strcmp(platform, "discord") && chat_type && !strcmp(chat_type, "thread")
           && auto_thread_created && nonempty(thread_id) && nonempty(initial_name);
}
int w6_runtopics_d5aa88_is_relay_discord_channel_lane(const char *platform, const char *chat_id,
        const char *thread_id, const char *chat_type, int relay) {
    if (!platform || strcmp(platform, "discord")) return 0;
    if (!nonempty(chat_id) || nonempty(thread_id) || !relay) return 0;
    return chat_type && (!strcmp(chat_type, "group") || !strcmp(chat_type, "channel"));
}
struct json_t *w6_runtopics_d5aa88_topic_restore_steps(void) {
    struct json_t *a = json_array();
    json_append(a, json_string("gateway.topic.restore_step_1"));
    json_append(a, json_string("gateway.topic.restore_step_2"));
    return a;
}
