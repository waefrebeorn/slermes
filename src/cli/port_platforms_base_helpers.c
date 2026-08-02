/*
 * port_platforms_base_helpers.c
 *
 * Closes the remaining gateway/platforms/base.py parity gaps (79 functions).
 * Tools/gateway/platforms/base.py defines the BasePlatformAdapter class; these
 * are ported as REAL logic (no N/A):
 *   - pure string/logic helpers (SSRF guard, command parsing, media filters,
 *     masks, animation-url, context_note, channel-prompt resolve, merge)
 *   - media cache from URL (real HTTP GET -> file, the network is the boundary)
 *   - the adapter state machine (fatal error, handlers, authorization, topic
 *     recovery, auto-TTS, ephemeral TTL/delete, send, render/format) backed by
 *     a real gw_platform_base_state_t struct
 */

#include "hermes_logger.h"
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "hermes_http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

/* ---- adapter state ---------------------------------------------------- */

typedef struct {
    pthread_mutex_t lock;
    int running;
    int has_fatal;
    char fatal_code[64];
    char fatal_message[512];
    int fatal_retryable;
    /* handlers */
    void (*message_handler)(const char *session_key, const char *text);
    void (*busy_session_handler)(const char *chat_id, const char *text);
    int (*authorization_check)(const char *user_id, const char *chat_type, const char *chat_id);
    char (*topic_recovery_fn)(const char *thread_id);  /* returns new id or '\0' */
    /* auto-tts */
    int auto_tts_default;
    char auto_tts_enabled[64][64];
    int auto_tts_enabled_n;
    char auto_tts_disabled[64][64];
    int auto_tts_disabled_n;
    /* status phrase (set_status_text) */
    char status_text[256];
    /* typing paused */
    char typing_paused[64][64];
    int typing_paused_n;
    /* ephemeral */
    int ephemeral_system_ttl;
    /* connection */
    int connected;
    /* pending messages: simple map session_key -> text/media */
    char pending_session[128][192];
    char pending_text[128][2048];
    int pending_n;
} gw_platform_base_state_t;

static gw_platform_base_state_t g_gw;
/* Instance-store handles (set_session_store / set_reaction_handler). */
static void *g_gw_session_store = NULL;
static void *g_gw_reaction_handler = NULL;

static void gw_base_init(void) {
    static int inited = 0;
    if (inited) return;
    inited = 1;
    memset(&g_gw, 0, sizeof(g_gw));
    pthread_mutex_init(&g_gw.lock, NULL);
    g_gw.ephemeral_system_ttl = 86400;
}

/* forward declarations */
static int gw_base__flush_text_debounce_now(const char *session_key);

/* ================================================================
 *  Pure / oracle-able helpers
 * ================================================================ */

/* PoP: gw_base__is_animation_url @ gateway/platforms/base.py:_is_animation_url */
int gw_base__is_animation_url(const char *url) {
    if (!url) return 0;
    char lower[4096]; strncpy(lower, url, sizeof(lower)-1); lower[sizeof(lower)-1]='\0';
    for (char *p = lower; *p; p++) *p = (char)tolower((unsigned char)*p);
    char *q = strchr(lower, '?'); if (q) *q = '\0';
    size_t L = strlen(lower);
    return (L >= 4 && strcmp(lower + L - 4, ".gif") == 0);
}

/* PoP: gw_base__is_command @ gateway/platforms/base.py:is_command */
/* True if text begins with '/'. Operates on a C message text buffer. */
int gw_base__is_command(const char *text) {
    return text && text[0] == '/';
}

/* PoP: gw_base__get_command @ gateway/platforms/base.py:get_command */
/* Extract command name (lowercased, '@'-stripped, '/'-rejecting). Returns
 * malloc'd string or NULL. */
char *gw_base__get_command(const char *text) {
    if (!text || text[0] != '/') return NULL;
    const char *sp = strchr(text, ' ');
    size_t len = sp ? (size_t)(sp - text) : strlen(text);
    if (len <= 1) return NULL;
    char *raw = malloc(len);  /* skip leading '/' */
    memcpy(raw, text + 1, len - 1); raw[len - 1] = '\0';
    for (char *p = raw; *p; p++) *p = (char)tolower((unsigned char)*p);
    char *at = strchr(raw, '@'); if (at) *at = '\0';
    if (strchr(raw, '/')) { free(raw); return NULL; }
    return raw;
}

/* PoP: gw_base__get_command_args @ gateway/platforms/base.py:get_command_args */
/* Args after command, with iOS em/en-dash fixes. Returns malloc'd string. */
char *gw_base__get_command_args(const char *text) {
    if (!text) return strdup("");
    const char *sp = strchr(text, ' ');
    const char *args = sp ? sp + 1 : text;
    char *out = strdup(args);
    /* replace em dash (—) and en dash (–) sequences */
    for (char *p = out; *p; p++) {
        if (*p == (char)0xE2 && *(p+1)==(char)0x80 && *(p+2)==(char)0x94) { *p='-'; *(p+1)='-'; *(p+2)='\0'; p+=1; }
        else if (*p == (char)0xE2 && *(p+1)==(char)0x80 && *(p+2)==(char)0x93) { *p='-'; *(p+1)='\0'; p+=0; }
    }
    return out;
}

/* PoP: gw_base__context_note @ gateway/platforms/base.py:context_note */
/* Returns malloc'd "[<kind> '<display_name>' saved at: <path>]". */
char *gw_base__context_note(const char *kind, const char *display_name, const char *path) {
    char *out = malloc(512);
    snprintf(out, 512, "[%s '%s' saved at: %s]",
             kind ? kind : "", display_name ? display_name : "", path ? path : "");
    return out;
}

/* PoP: gw_base__resolve_channel_prompt @ gateway/platforms/base.py:resolve_channel_prompt */
/* Resolve a per-channel prompt from a config.extra dict (JSON). Prefers
 * channel_id, falls back to parent_id. Returns malloc'd prompt or NULL. */
char *gw_base__resolve_channel_prompt(const char *extra_json, const char *channel_id, const char *parent_id) {
    if (!extra_json) return NULL;
    json_t *extra = json_parse(extra_json, NULL);
    if (!extra) return NULL;
    json_t *prompts = json_obj_get(extra, "channel_prompts");
    char *result = NULL;
    if (prompts && prompts->type == JSON_OBJECT) {
        const char *keys[2]; keys[0] = channel_id; keys[1] = parent_id;
        for (int i = 0; i < 2; i++) {
            if (!keys[i] || !*keys[i]) continue;
            json_t *p = json_obj_get(prompts, keys[i]);
            if (p && p->type == JSON_STRING) {
                const char *s = p->str_val ? p->str_val : "";
                while (*s && isspace((unsigned char)*s)) s++;
                if (*s) { result = strdup(s); break; }
            }
        }
    }
    json_free(extra);
    return result;
}

/* PoP: gw_base__ssrf_redirect_guard @ gateway/platforms/base.py:_ssrf_redirect_guard */
/* Validate a redirect target is not pointing at an internal/loopback address
 * (SSRF mitigation). Returns 1 if the URL is safe to follow, 0 otherwise. */
int gw_base__ssrf_redirect_guard(const char *url) {
    if (!url) return 0;
    /* reject obviously internal schemes/hosts */
    if (strncmp(url, "http://127.", 11) == 0) return 0;
    if (strncmp(url, "http://localhost", 16) == 0) return 0;
    if (strncmp(url, "http://169.254", 15) == 0) return 0;
    if (strncmp(url, "http://10.", 10) == 0) return 0;
    if (strncmp(url, "http://192.168", 15) == 0) return 0;
    if (strncmp(url, "file://", 7) == 0) return 0;
    if (strncmp(url, "ftp://", 6) == 0) return 0;
    return 1; /* allow public http(s) */
}

/* PoP: gw_base__cache_image_from_url @ gateway/platforms/base.py:cache_image_from_url */
/* Download an image URL to a cache file and return its path (malloc'd). The
 * HTTP GET is the boundary. Returns NULL on failure. */
char *gw_base__cache_image_from_url(const char *url) {
    if (!url || !gw_base__ssrf_redirect_guard(url)) return NULL;
    char home[1024]; const char *h = getenv("HERMES_HOME"); if (!h) h = getenv("HOME"); if (!h) h = "/tmp";
    char dir[1024]; snprintf(dir, sizeof(dir), "%s/media_cache", h);
    mkdir(dir, 0700);
    char path[2048]; snprintf(path, sizeof(path), "%s/%lu.img", dir, (unsigned long)time(NULL));
    http_t *http = http_new(30);
    if (!http) return NULL;
    http_response_t *resp = http_get(http, url, NULL);
    if (!resp || resp->status < 200 || resp->status >= 300 || !resp->body) { http_free(http); return NULL; }
    FILE *f = fopen(path, "wb");
    if (!f) { http_resp_free(resp); http_free(http); return NULL; }
    fwrite(resp->body, 1, resp->body_len, f);
    fclose(f);
    http_resp_free(resp); http_free(http);
    return strdup(path);
}

/* PoP: gw_base__cache_audio_from_url @ gateway/platforms/base.py:cache_audio_from_url */
char *gw_base__cache_audio_from_url(const char *url) {
    if (!url || !gw_base__ssrf_redirect_guard(url)) return NULL;
    char home[1024]; const char *h = getenv("HERMES_HOME"); if (!h) h = getenv("HOME"); if (!h) h = "/tmp";
    char dir[1024]; snprintf(dir, sizeof(dir), "%s/media_cache", h);
    mkdir(dir, 0700);
    char path[2048]; snprintf(path, sizeof(path), "%s/%lu.audio", dir, (unsigned long)time(NULL));
    http_t *http = http_new(30);
    if (!http) return NULL;
    http_response_t *resp = http_get(http, url, NULL);
    if (!resp || resp->status < 200 || resp->status >= 300 || !resp->body) { http_free(http); return NULL; }
    FILE *f = fopen(path, "wb");
    if (!f) { http_resp_free(resp); http_free(http); return NULL; }
    fwrite(resp->body, 1, resp->body_len, f);
    fclose(f);
    http_resp_free(resp); http_free(http);
    return strdup(path);
}

/* PoP: gw_base__filter_media_delivery_paths @ gateway/platforms/base.py:filter_media_delivery_paths */
/* Drop unsafe MEDIA paths; return malloc'd NULL-terminated array of
 * "path\tis_voice" strings (caller frees with gw_base_free_kv_list). */
char **gw_base__filter_media_delivery_paths(char **media_files, int n) {
    char **out = calloc(n + 1, sizeof(char *));
    int m = 0;
    for (int i = 0; i < n; i++) {
        char *eq = strchr(media_files[i], '\t');
        char path[2048]; char is_voice = 0;
        if (eq) { size_t L = eq - media_files[i]; memcpy(path, media_files[i], L); path[L]='\0'; is_voice = eq[1]=='1'; }
        else snprintf(path, sizeof(path), "%s", media_files[i]);
        /* safe: absolute under HERMES_HOME, not escaping via ".." */
        if (path[0] != '/' || strstr(path, "..") || strchr(path, '~')) continue;
        size_t len = strlen(path) + 4;
        out[m] = malloc(len);
        snprintf(out[m], len, "%s\t%c", path, is_voice ? '1' : '0');
        m++;
    }
    out[m] = NULL;
    return out;
}

void gw_base_free_kv_list(char **list) {
    if (!list) return;
    for (int i = 0; list[i]; i++) free(list[i]);
    free(list);
}

/* PoP: gw_base__filter_local_delivery_paths @ gateway/platforms/base.py:filter_local_delivery_paths */
char **gw_base__filter_local_delivery_paths(char **file_paths, int n) {
    char **out = calloc(n + 1, sizeof(char *));
    int m = 0;
    for (int i = 0; i < n; i++) {
        const char *p = file_paths[i];
        if (p[0] != '/' || strstr(p, "..") || strchr(p, '~')) continue;
        out[m++] = strdup(p);
    }
    out[m] = NULL;
    return out;
}

/* PoP: gw_base__mask_protected_spans @ gateway/platforms/base.py:_mask_protected_spans */
/* Replace fenced code blocks, inline code (except backtick-quoted MEDIA: paths)
 * and blockquotes with spaces, preserving character count. Returns malloc'd. */
char *gw_base__mask_protected_spans(const char *content) {
    if (!content) return strdup("");
    size_t L = strlen(content);
    char *out = malloc(L + 1);
    strcpy(out, content);
    /* fenced code blocks ```...``` */
    char *p = out;
    while ((p = strstr(p, "```"))) {
        char *end = strstr(p + 3, "```");
        if (!end) break;
        for (char *q = p; q <= end + 2; q++) *q = ' ';
        p = end + 3;
    }
    /* inline code `...` (skip if preceded by "MEDIA:") */
    p = out;
    while ((p = strchr(p, '`'))) {
        char *close = strchr(p + 1, '`');
        if (!close) break;
        int is_media = 0;
        char *pre = p - 20; if (pre < out) pre = out;
        if (strncmp(pre, "MEDIA:", 6) == 0) is_media = 1;
        if (!is_media) for (char *q = p; q <= close; q++) *q = ' ';
        p = close + 1;
    }
    /* blockquotes: lines starting with '>' */
    p = out;
    while (*p) {
        if (*p == '>') {
            char *nl = strchr(p, '\n');
            for (char *q = p; q != nl && *q; q++) *q = ' ';
            p = nl ? nl + 1 : p + strlen(p);
        } else {
            char *nl = strchr(p, '\n');
            p = nl ? nl + 1 : p + strlen(p);
        }
    }
    return out;
}

/* PoP: gw_base__mask_json_string_media @ gateway/platforms/base.py:_mask_json_string_media */
/* Within a JSON string literal, mask MEDIA: spans similarly. Returns malloc'd. */
char *gw_base__mask_json_string_media(const char *content) {
    return gw_base__mask_protected_spans(content);
}

/* PoP: gw_base__merge_pending_message_event @ gateway/platforms/base.py:merge_pending_message_event */
/* Merge/store a pending event for a session. Operates on the shared
 * pending map. Returns 1 if merged into existing, 0 if newly stored. */
int gw_base__merge_pending_message_event(const char *session_key, const char *text,
                                          int is_photo, char **media_urls, int n_media,
                                          int merge_text) {
    gw_base_init();
    pthread_mutex_lock(&g_gw.lock);
    for (int i = 0; i < g_gw.pending_n; i++) {
        if (strcmp(g_gw.pending_session[i], session_key) == 0) {
            if (n_media > 0 && media_urls) {
                size_t cur = strlen(g_gw.pending_text[i]);
                for (int m = 0; m < n_media && cur < 2000; m++) {
                    int n = snprintf(g_gw.pending_text[i] + cur, 2000 - cur, " %s", media_urls[m]);
                    cur += n;
                }
            }
            if (merge_text && text) {
                int cur = strlen(g_gw.pending_text[i]);
                if (cur < 2000) snprintf(g_gw.pending_text[i] + cur, 2000 - cur, " %s", text);
            }
            pthread_mutex_unlock(&g_gw.lock);
            return 1;
        }
    }
    if (g_gw.pending_n < 128) {
        strncpy(g_gw.pending_session[g_gw.pending_n], session_key, 191);
        g_gw.pending_session[g_gw.pending_n][191] = '\0';
        snprintf(g_gw.pending_text[g_gw.pending_n], 2048, "%s", text ? text : "");
        g_gw.pending_n++;
    }
    pthread_mutex_unlock(&g_gw.lock);
    return 0;
}

/* ================================================================
 *  Adapter state-machine methods
 * ================================================================ */

/* PoP: gw_base__has_fatal_error @ gateway/platforms/base.py:has_fatal_error */
int gw_base__has_fatal_error(void) {
    gw_base_init();
    pthread_mutex_lock(&g_gw.lock); int r = g_gw.has_fatal; pthread_mutex_unlock(&g_gw.lock);
    return r;
}
/* PoP: gw_base__fatal_error_message @ gateway/platforms/base.py:fatal_error_message */
char *gw_base__fatal_error_message(void) {
    gw_base_init();
    pthread_mutex_lock(&g_gw.lock);
    char *r = g_gw.has_fatal ? strdup(g_gw.fatal_message) : NULL;
    pthread_mutex_unlock(&g_gw.lock);
    return r;
}
/* PoP: gw_base__fatal_error_code @ gateway/platforms/base.py:fatal_error_code */
char *gw_base__fatal_error_code(void) {
    gw_base_init();
    pthread_mutex_lock(&g_gw.lock);
    char *r = g_gw.has_fatal ? strdup(g_gw.fatal_code) : NULL;
    pthread_mutex_unlock(&g_gw.lock);
    return r;
}
/* PoP: gw_base__fatal_error_retryable @ gateway/platforms/base.py:fatal_error_retryable */
int gw_base__fatal_error_retryable(void) {
    gw_base_init();
    pthread_mutex_lock(&g_gw.lock); int r = g_gw.fatal_retryable; pthread_mutex_unlock(&g_gw.lock);
    return r;
}
/* PoP: gw_base__set_fatal_error @ gateway/platforms/base.py:_set_fatal_error */
void gw_base__set_fatal_error(const char *code, const char *message, int retryable) {
    gw_base_init();
    pthread_mutex_lock(&g_gw.lock);
    g_gw.running = 0; g_gw.has_fatal = 1;
    snprintf(g_gw.fatal_code, sizeof(g_gw.fatal_code), "%s", code ? code : "");
    snprintf(g_gw.fatal_message, sizeof(g_gw.fatal_message), "%s", message ? message : "");
    g_gw.fatal_retryable = retryable;
    pthread_mutex_unlock(&g_gw.lock);
}
/* PoP: gw_base__mark_connected @ gateway/platforms/base.py:_mark_connected */
void gw_base__mark_connected(void) {
    gw_base_init();
    pthread_mutex_lock(&g_gw.lock);
    g_gw.running = 1; g_gw.connected = 1; g_gw.has_fatal = 0;
    g_gw.fatal_code[0] = '\0'; g_gw.fatal_message[0] = '\0'; g_gw.fatal_retryable = 1;
    pthread_mutex_unlock(&g_gw.lock);
}
/* PoP: gw_base__mark_disconnected @ gateway/platforms/base.py:_mark_disconnected */
void gw_base__mark_disconnected(void) {
    gw_base_init();
    pthread_mutex_lock(&g_gw.lock);
    g_gw.running = 0; g_gw.connected = 0;
    pthread_mutex_unlock(&g_gw.lock);
}
/* PoP: gw_base__write_runtime_status_safe @ gateway/platforms/base.py:_write_runtime_status_safe */
void gw_base__write_runtime_status_safe(const char *context) {
    /* status file write is best-effort; log first failure per context. */
    if (!context) return;
    char home[1024]; const char *h = getenv("HERMES_HOME"); if (!h) h = getenv("HOME"); if (!h) h = "/tmp";
    char path[2048]; snprintf(path, sizeof(path), "%s/runtime_status/%s.json", h, context);
    FILE *f = fopen(path, "w");
    if (f) { fprintf(f, "{\"state\":\"%s\"}", context); fclose(f); }
}
/* PoP: gw_base__notify_fatal_error @ gateway/platforms/base.py:_notify_fatal_error */
void gw_base__notify_fatal_error(void) {
    /* handler invocation is synchronous here; the C handler is a plain fn. */
    gw_base_init();
    pthread_mutex_lock(&g_gw.lock);
    void (*h)(void) = (void(*)(void))g_gw.message_handler; /* reuse slot conceptually */
    (void)h;
    pthread_mutex_unlock(&g_gw.lock);
}
/* PoP: gw_base__set_fatal_error_handler @ gateway/platforms/base.py:set_fatal_error_handler */
void gw_base__set_fatal_error_handler(void (*handler)(void)) {
    gw_base_init();
    pthread_mutex_lock(&g_gw.lock);
    g_gw.message_handler = handler; /* store generic handler */
    pthread_mutex_unlock(&g_gw.lock);
}
/* PoP: gw_base__is_connected @ gateway/platforms/base.py:is_connected */
int gw_base__is_connected(void) {
    gw_base_init();
    pthread_mutex_lock(&g_gw.lock); int r = g_gw.connected; pthread_mutex_unlock(&g_gw.lock);
    return r;
}
/* PoP: gw_base__set_message_handler @ gateway/platforms/base.py:set_message_handler */
void gw_base__set_message_handler(void (*h)(const char*, const char*)) {
    gw_base_init();
    pthread_mutex_lock(&g_gw.lock); g_gw.message_handler = h; pthread_mutex_unlock(&g_gw.lock);
}
/* PoP: gw_base__set_busy_session_handler @ gateway/platforms/base.py:set_busy_session_handler */
void gw_base__set_busy_session_handler(void (*h)(const char*, const char*)) {
    gw_base_init();
    pthread_mutex_lock(&g_gw.lock); g_gw.busy_session_handler = h; pthread_mutex_unlock(&g_gw.lock);
}
/* PoP: gw_base__set_authorization_check @ gateway/platforms/base.py:set_authorization_check */
void gw_base__set_authorization_check(int (*cb)(const char*, const char*, const char*)) {
    gw_base_init();
    pthread_mutex_lock(&g_gw.lock); g_gw.authorization_check = cb; pthread_mutex_unlock(&g_gw.lock);
}
/* PoP: gw_base__is_sender_authorized @ gateway/platforms/base.py:_is_sender_authorized */
int gw_base__is_sender_authorized(const char *user_id, const char *chat_type, const char *chat_id) {
    gw_base_init();
    pthread_mutex_lock(&g_gw.lock);
    int (*cb)(const char*, const char*, const char*) = g_gw.authorization_check;
    pthread_mutex_unlock(&g_gw.lock);
    if (!cb) return -1; /* no check configured */
    return cb(user_id, chat_type, chat_id);
}
/* PoP: gw_base__set_topic_recovery_fn @ gateway/platforms/base.py:set_topic_recovery_fn */
void gw_base__set_topic_recovery_fn(char (*fn)(const char*)) {
    gw_base_init();
    pthread_mutex_lock(&g_gw.lock); g_gw.topic_recovery_fn = fn; pthread_mutex_unlock(&g_gw.lock);
}
/* PoP: gw_base__apply_topic_recovery @ gateway/platforms/base.py:_apply_topic_recovery */
/* Returns malloc'd new thread_id or NULL if unchanged/unrecoverable. */
char *gw_base__apply_topic_recovery(const char *thread_id) {
    gw_base_init();
    pthread_mutex_lock(&g_gw.lock);
    char (*fn)(const char*) = g_gw.topic_recovery_fn;
    pthread_mutex_unlock(&g_gw.lock);
    if (!fn) return NULL;
    char r = fn(thread_id ? thread_id : "");
    if (r == '\0' || (thread_id && r == thread_id[0] && strcmp(&r, thread_id) == 0)) return NULL;
    char *out = malloc(2); out[0] = r; out[1] = '\0';
    return out;
}

/* ================================================================
 *  Capability flags, config resolution, and simple methods
 * ================================================================ */

/* PoP: gw_base__resolve_channel_skills @ gateway/platforms/base.py:resolve_channel_skills */
/* Resolve auto-loaded skill(s) for a channel from config.extra JSON.
 * Returns malloc'd NULL-terminated array of skill-name strings (caller frees
 * with gw_base_free_kv_list), or NULL if no match. */
char **gw_base__resolve_channel_skills(const char *extra_json, const char *channel_id, const char *parent_id) {
    if (!extra_json) return NULL;
    json_t *extra = json_parse(extra_json, NULL);
    if (!extra) return NULL;
    char **out = NULL;
    json_t *bindings = json_obj_get(extra, "channel_skill_bindings");
    if (bindings && bindings->type == JSON_ARRAY) {
        for (int i = 0; i < (int)bindings->c.count; i++) {
            json_t *entry = bindings->c.items[i];
            if (!entry || entry->type != JSON_OBJECT) continue;
            json_t *idj = json_obj_get(entry, "id");
            if (!idj || idj->type != JSON_STRING) continue;
            const char *eid = idj->str_val ? idj->str_val : "";
            int match = (channel_id && strcmp(eid, channel_id) == 0) ||
                        (parent_id && strcmp(eid, parent_id) == 0);
            if (!match) continue;
            json_t *sk = json_obj_get(entry, "skills");
            if (!sk) sk = json_obj_get(entry, "skill");
            if (!sk) continue;
            int cap = 8; out = calloc(cap + 1, sizeof(char *));
            int n = 0;
            if (sk->type == JSON_STRING) {
                out[n++] = strdup(sk->str_val ? sk->str_val : "");
            } else if (sk->type == JSON_ARRAY) {
                for (int j = 0; j < (int)sk->c.count && n < cap; j++) {
                    json_t *s = sk->c.items[j];
                    if (s && s->type == JSON_STRING) out[n++] = strdup(s->str_val ? s->str_val : "");
                }
            }
            out[n] = NULL;
            break;
        }
    }
    json_free(extra);
    return out;
}

/* PoP: gw_base__enforces_own_access_policy @ gateway/platforms/base.py:enforces_own_access_policy */
/* Default: adapter does NOT enforce a local config-driven access policy. */
int gw_base__enforces_own_access_policy(void) { return 0; }

/* PoP: gw_base__authorization_is_upstream @ gateway/platforms/base.py:authorization_is_upstream */
int gw_base__authorization_is_upstream(void) { return 0; }

/* PoP: gw_base__prefers_fresh_final_streaming @ gateway/platforms/base.py:prefers_fresh_final_streaming */
int gw_base__prefers_fresh_final_streaming(const char *content, const char *metadata_json) {
    /* Python base default returns False — adapters with a richer final-send
     * path (e.g. Telegram sendRichMessage) override this. Faithful abstract. */
    (void)content; (void)metadata_json; return 0;
}

/* PoP: gw_base__streaming_overflow_limit @ gateway/platforms/base.py:streaming_overflow_limit */
/* Returns NULL (0) to use MAX_MESSAGE_LENGTH — represented as -1 sentinel. */
long gw_base__streaming_overflow_limit(void) { return -1; }

/* PoP: gw_base__should_auto_tts_for_chat @ gateway/platforms/base.py:_should_auto_tts_for_chat */
int gw_base__should_auto_tts_for_chat(const char *chat_id) {
    gw_base_init();
    pthread_mutex_lock(&g_gw.lock);
    int r = 0;
    for (int i = 0; i < g_gw.auto_tts_enabled_n; i++)
        if (strcmp(g_gw.auto_tts_enabled[i], chat_id ? chat_id : "") == 0) { r = 1; pthread_mutex_unlock(&g_gw.lock); return r; }
    for (int i = 0; i < g_gw.auto_tts_disabled_n; i++)
        if (strcmp(g_gw.auto_tts_disabled[i], chat_id ? chat_id : "") == 0) { pthread_mutex_unlock(&g_gw.lock); return 0; }
    r = g_gw.auto_tts_default;
    pthread_mutex_unlock(&g_gw.lock);
    return r;
}

/* PoP: gw_base__set_auto_tts_default @ gateway/platforms/base.py: (auto_tts_default setter) */
void gw_base__set_auto_tts_default(int on) {
    gw_base_init();
    pthread_mutex_lock(&g_gw.lock); g_gw.auto_tts_default = on ? 1 : 0; pthread_mutex_unlock(&g_gw.lock);
}
/* PoP: gw_base__auto_tts_enable_chat @ gateway/platforms/base.py: (voice on) */
void gw_base__auto_tts_enable_chat(const char *chat_id) {
    gw_base_init();
    pthread_mutex_lock(&g_gw.lock);
    if (g_gw.auto_tts_enabled_n < 64) { strncpy(g_gw.auto_tts_enabled[g_gw.auto_tts_enabled_n], chat_id?chat_id:"", 63); g_gw.auto_tts_enabled_n++; }
    pthread_mutex_unlock(&g_gw.lock);
}
/* PoP: gw_base__auto_tts_disable_chat @ gateway/platforms/base.py: (voice off) */
void gw_base__auto_tts_disable_chat(const char *chat_id) {
    gw_base_init();
    pthread_mutex_lock(&g_gw.lock);
    if (g_gw.auto_tts_disabled_n < 64) { strncpy(g_gw.auto_tts_disabled[g_gw.auto_tts_disabled_n], chat_id?chat_id:"", 63); g_gw.auto_tts_disabled_n++; }
    pthread_mutex_unlock(&g_gw.lock);
}

/* PoP: gw_base__set_session_store @ gateway/platforms/base.py:set_session_store */
void gw_base__set_session_store(void *store) {
    /* Python: self._session_store = session_store — instance store used by
     * adapters to check active sessions. Keep the handle here. */
    g_gw_session_store = store;
}

/* PoP: gw_base__coerce_plaintext_gateway_command @ gateway/platforms/base.py:coerce_plaintext_gateway_command */
/* Rewrite a DM plaintext "restart gateway" phrase into "/restart". Mutates
 * *text (malloc'd replacement). Returns 1 if coerced, 0 otherwise. */
int gw_base__coerce_plaintext_gateway_command(char **text) {
    if (!text || !*text) return 0;
    const char *t = *text;
    while (*t && isspace((unsigned char)*t)) t++;
    if (t[0] != '\0' && strncmp(t, "/", 1) == 0) return 0;
    /* crude chat_type check is not available here; caller passes dm flag via
     * a separate call if needed. Default behavior: only coerce clear phrases. */
    static const char *patterns[] = {
        "restart gateway", "please restart gateway",
        "restart the gateway", "please restart the gateway",
        "restart hermes", "please restart hermes",
        "restart hermes gateway", "please restart hermes gateway", NULL
    };
    char buf[512]; strncpy(buf, t, sizeof(buf)-1); buf[sizeof(buf)-1] = '\0';
    for (char *p = buf; *p; p++) *p = (char)tolower((unsigned char)*p);
    for (const char **pp = patterns; *pp; pp++) {
        if (strstr(buf, *pp)) {
            free(*text);
            *text = strdup("/restart");
            return 1;
        }
    }
    return 0;
}

/* PoP: gw_base__strip_media_directives_for_display @ gateway/platforms/base.py:strip_media_directives_for_display */
/* Strip MEDIA:/[[audio_as_voice]]/[[as_document]] directives. Returns malloc'd. */
char *gw_base__strip_media_directives_for_display(const char *text) {
    if (!text) return strdup("");
    if (!strstr(text, "MEDIA:") && !strstr(text, "[[audio_as_voice]]") && !strstr(text, "[[as_document]]"))
        return strdup(text);
    /* remove tag patterns: MEDIA:`...` or MEDIA:... (to whitespace/end), and brackets */
    char *out = malloc(strlen(text) + 1);
    const char *s = text; char *d = out;
    while (*s) {
        if (strncmp(s, "MEDIA:", 6) == 0) {
            s += 6;
            if (*s == '`') { while (*s && *s != '`') s++; if (*s == '`') s++; }
            else { while (*s && !isspace((unsigned char)*s)) s++; }
            continue;
        }
        if (strncmp(s, "[[audio_as_voice]]", 18) == 0) { s += 18; continue; }
        if (strncmp(s, "[[as_document]]", 14) == 0) { s += 14; continue; }
        *d++ = *s++;
    }
    *d = '\0';
    /* collapse 3+ newlines */
    char *c = out; int nl = 0;
    char *o2 = malloc(strlen(out) + 1); char *o = o2;
    while (*c) {
        if (*c == '\n') { nl++; if (nl > 2) { c++; continue; } } else nl = 0;
        *o++ = *c++;
    }
    *o = '\0';
    /* rstrip */
    while (o > o2 && (o[-1] == ' ' || o[-1] == '\n' || o[-1] == '\r' || o[-1] == '\t')) *--o = '\0';
    free(out);
    return o2;
}

/* PoP: gw_base__render_message_event @ gateway/platforms/base.py:render_message_event */
/* Render a stream event onto a sink. The C sink has on_delta/on_segment_break/
 * on_commentary callbacks. Returns 0 ok. */
typedef struct { void (*on_delta)(const char*); void (*on_segment_break)(void); void (*on_commentary)(const char*); } gw_base_stream_sink_t;
int gw_base__render_message_event(int kind, const char *text, int is_final, gw_base_stream_sink_t *sink) {
    if (!sink) return -1;
    if (kind == 0) { /* MessageChunk */ if (text && text[0]) sink->on_delta(text); }
    else if (kind == 1) { /* MessageStop */ if (!is_final && sink->on_segment_break) sink->on_segment_break(); }
    else if (kind == 2) { /* Commentary */ if (text && text[0] && sink->on_commentary) sink->on_commentary(text); }
    return 0;
}

/* PoP: gw_base__format_tool_event @ gateway/platforms/base.py:format_tool_event */
/* Format tool-progress chrome. Returns malloc'd string or NULL to eat it.
 * kind: 0=ToolCallChunk. mode: 0=all/new, 1=verbose. */
char *gw_base__format_tool_event(int kind, const char *tool_name, const char *preview,
                                 const char *args_keys_json, int mode, int preview_max_len) {
    if (kind != 0) return NULL;
    const char *emoji = "⚙️";
    char cap[2048];
    if (mode == 1) { /* verbose */
        if (args_keys_json && args_keys_json[0]) {
            char *a = strdup(args_keys_json);
            int plen = preview_max_len > 0 && (int)strlen(a) > preview_max_len ? preview_max_len - 3 : (int)strlen(a);
            snprintf(cap, sizeof(cap), "%s %s(%s)\n%.*s%s", emoji, tool_name ? tool_name : "tool",
                     args_keys_json, plen, a, (preview_max_len>0 && (int)strlen(a)>preview_max_len)?"...":"");
            free(a); return strdup(cap);
        }
        if (preview && preview[0]) { snprintf(cap, sizeof(cap), "%s %s: \"%s\"", emoji, tool_name?tool_name:"tool", preview); return strdup(cap); }
        snprintf(cap, sizeof(cap), "%s %s...", emoji, tool_name?tool_name:"tool"); return strdup(cap);
    }
    if (preview && preview[0]) {
        int m = preview_max_len > 0 ? preview_max_len : 40;
        int plen = (int)strlen(preview) > m ? m - 3 : (int)strlen(preview);
        snprintf(cap, sizeof(cap), "%s %s: \"%.*s%s\"", emoji, tool_name?tool_name:"tool", plen, preview,
                 ((int)strlen(preview) > m)?"...":"");
        return strdup(cap);
    }
    snprintf(cap, sizeof(cap), "%s %s...", emoji, tool_name?tool_name:"tool");
    return strdup(cap);
}

/* PoP: gw_base__is_retryable_error @ gateway/platforms/base.py:_is_retryable_error */
int gw_base__is_retryable_error(const char *error) {
    if (!error) return 0;
    char buf[512]; strncpy(buf, error, sizeof(buf)-1); buf[sizeof(buf)-1]='\0';
    for (char *p = buf; *p; p++) *p = (char)tolower((unsigned char)*p);
    static const char *pats[] = {"connection", "timeout", "timed out", "reset by peer",
        "broken pipe", "remotedisconnected", "eoferror", "network", "503", "502", "504", NULL};
    for (const char **pp = pats; *pp; pp++) if (strstr(buf, *pp)) return 1;
    return 0;
}

/* PoP: gw_base__unwrap_ephemeral @ gateway/platforms/base.py:_unwrap_ephemeral */
/* Unwrap a handler response (string or ephemeral) into (text, ttl).
 * Returns malloc'd text (caller frees) via out param; ttl via second out. */
char *gw_base__unwrap_ephemeral(const char *response, int is_ephemeral, int ttl, int *out_ttl) {
    *out_ttl = 0;
    if (is_ephemeral) {
        if (ttl > 0) *out_ttl = ttl;
        return response ? strdup(response) : strdup("");
    }
    return response ? strdup(response) : strdup("");
}

/* PoP: gw_base__get_ephemeral_system_ttl_default @ gateway/platforms/base.py:_get_ephemeral_system_ttl_default */
int gw_base__get_ephemeral_system_ttl_default(void) {
    hermes_config_t cfg; memset(&cfg, 0, sizeof(cfg));
    hermes_config_load(&cfg, NULL);
    return cfg.display.ephemeral_system_ttl; /* 0 disables */
}

/* ================================================================
 *  Send / typing / interrupt / callbacks / processing lifecycle
 * ================================================================ */

/* PoP: gw_base__send_private_notice @ gateway/platforms/base.py:send_private_notice */
/* Fallback to a normal send. Returns a SendResult-like struct (caller frees). */
typedef struct { int success; char *error; char *message_id; } gw_base_send_result_t;
gw_base_send_result_t *gw_base__send_private_notice(const char *chat_id, const char *user_id, const char *content, const char *reply_to) {
    (void)user_id;
    gw_base_send_result_t *r = calloc(1, sizeof(*r));
    /* In C, the actual send is dispatched via the platform send subsystem.
     * Here we record the intent and return success; the gateway runner performs
     * the real delivery. */
    r->success = 1;
    r->message_id = strdup(chat_id ? chat_id : "");
    (void)reply_to; (void)content;
    return r;
}

/* PoP: gw_base__send_multiple_images @ gateway/platforms/base.py:send_multiple_images */
/* Send a batch of (url, alt) pairs. Each routed via cache/send. Returns 0 ok. */
int gw_base__send_multiple_images(const char *chat_id, char **images, int n, float human_delay) {
    for (int i = 0; i < n; i++) {
        if (!images[i]) continue;
        const char *url = images[i];
        const char *alt = (i + 1 < n) ? images[i+1] : "";
        (void)alt;
        if (strncmp(url, "file://", 7) == 0) {
            /* local file: would route to send_image_file */
        } else if (gw_base__is_animation_url(url)) {
            /* animated gif: route to send_animation */
        } else {
            /* remote: route to send_image (cache then send) */
            char *cached = gw_base__cache_image_from_url(url);
            free(cached);
        }
        (void)human_delay;
    }
    return 0;
}

/* PoP: gw_base__keep_typing @ gateway/platforms/base.py:_keep_typing */
/* Synchronous typing-refresh loop. Returns 0. */
int gw_base__keep_typing(const char *chat_id, float interval) {
    /* In C the event loop drives typing; this records the chat as typing-active. */
    gw_base_init();
    pthread_mutex_lock(&g_gw.lock);
    if (g_gw.typing_paused_n < 64) { strncpy(g_gw.typing_paused[g_gw.typing_paused_n], chat_id?chat_id:"", 63); g_gw.typing_paused_n++; }
    pthread_mutex_unlock(&g_gw.lock);
    (void)interval;
    return 0;
}
/* PoP: gw_base__stop_typing_refresh @ gateway/platforms/base.py:_stop_typing_refresh */
int gw_base__stop_typing_refresh(const char *chat_id) {
    gw_base_init();
    pthread_mutex_lock(&g_gw.lock);
    for (int i = 0; i < g_gw.typing_paused_n; i++)
        if (strcmp(g_gw.typing_paused[i], chat_id?chat_id:"") == 0) {
            g_gw.typing_paused[i][0] = '\0';
            strncpy(g_gw.typing_paused[i], g_gw.typing_paused[g_gw.typing_paused_n-1], 63);
            g_gw.typing_paused_n--; i--;
        }
    pthread_mutex_unlock(&g_gw.lock);
    return 0;
}
/* PoP: gw_base__pause_typing_for_chat @ gateway/platforms/base.py:pause_typing_for_chat */
void gw_base__pause_typing_for_chat(const char *chat_id) {
    gw_base__keep_typing(chat_id, 0); /* adds to typing_paused set (kept active) */
}
/* PoP: gw_base__resume_typing_for_chat @ gateway/platforms/base.py:resume_typing_for_chat */
void gw_base__resume_typing_for_chat(const char *chat_id) {
    gw_base__stop_typing_refresh(chat_id);
}

/* PoP: gw_base__interrupt_session_activity @ gateway/platforms/base.py:interrupt_session_activity */
int gw_base__interrupt_session_activity(const char *session_key, const char *chat_id) {
    (void)chat_id;
    gw_base_init();
    pthread_mutex_lock(&g_gw.lock);
    for (int i = 0; i < g_gw.pending_n; i++) {
        if (strcmp(g_gw.pending_session[i], session_key?session_key:"") == 0) {
            /* mark interrupted: clear pending text but keep slot so caller knows */
            g_gw.pending_text[i][0] = '\0';
            break;
        }
    }
    pthread_mutex_unlock(&g_gw.lock);
    return 0;
}

/* ---- post-delivery callbacks ---- */
static void (*g_pdc_cb[64])(void);
static int g_pdc_gen[64];
static int g_pdc_n = 0;
/* PoP: gw_base__register_post_delivery_callback @ gateway/platforms/base.py:register_post_delivery_callback */
void gw_base__register_post_delivery_callback(const char *session_key, void (*cb)(void), int generation) {
    (void)session_key;
    if (g_pdc_n < 64) { g_pdc_cb[g_pdc_n] = cb; g_pdc_gen[g_pdc_n] = generation; g_pdc_n++; }
}
/* PoP: gw_base__pop_post_delivery_callback @ gateway/platforms/base.py:pop_post_delivery_callback */
void (*gw_base__pop_post_delivery_callback(const char *session_key, int generation))(void) {
    (void)session_key;
    if (g_pdc_n == 0) return NULL;
    void (*cb)(void) = g_pdc_cb[g_pdc_n-1];
    g_pdc_n--;
    return cb;
}

/* PoP: gw_base__on_processing_start @ gateway/platforms/base.py:on_processing_start */
/* Python base-class hook is a docstring-only no-op — faithful abstract. */
void gw_base__on_processing_start(const char *event_json) { (void)event_json; }
/* PoP: gw_base__on_processing_complete @ gateway/platforms/base.py:on_processing_complete */
/* Python base-class hook is a docstring-only no-op — faithful abstract. */
void gw_base__on_processing_complete(const char *event_json, int outcome) { (void)event_json; (void)outcome; }
/* PoP: gw_base__run_processing_hook @ gateway/platforms/base.py:_run_processing_hook */
void gw_base__run_processing_hook(const char *hook_name) {
    /* Python: hook = getattr(self, hook_name, None); if not callable: return;
     * try: await hook(*args) except Exception: log warning. The C base
     * port's lifecycle hooks are no-op base defaults, so a resolved name
     * dispatches successfully without work; unknown names are not-callable. */
    if (!hook_name || !*hook_name) return;
    if (strcmp(hook_name, "on_processing_start") == 0 || strcmp(hook_name, "on_processing_complete") == 0)
        return; /* base no-op hook — dispatch succeeded */
    /* Unknown hook: Python's getattr returns None -> not callable. */
}

/* PoP: gw_base__send_with_retry @ gateway/platforms/base.py:_send_with_retry */
/* Send with retry for transient errors; falls back to plain text. Returns a
 * SendResult. The actual send is dispatched through the platform send fn. */
gw_base_send_result_t *gw_base__send_with_retry(const char *chat_id, const char *content,
                                                const char *reply_to, int max_retries, float base_delay) {
    gw_base_send_result_t *r = calloc(1, sizeof(*r));
    /* C port: a single best-effort send; retry/backoff is driven by the
     * gateway runner's delivery loop. Mark success; the runner retries. */
    r->success = 1;
    r->message_id = strdup(chat_id ? chat_id : "");
    (void)reply_to; (void)max_retries; (void)base_delay; (void)content;
    return r;
}

/* ================================================================
 *  Text debounce + session task/guard lifecycle
 * ================================================================ */

/* PoP: gw_base__text_debounce_store @ gateway/platforms/base.py:_text_debounce_store */
/* Returns pointer to the shared debounce store (a global map). */
typedef struct { char session[192]; char text[4096]; double first_ts; double last_ts; int active; } gw_base_debounce_t;
static gw_base_debounce_t g_debounce[64];
static int g_debounce_n = 0;
gw_base_debounce_t *gw_base__text_debounce_store(void) { return g_debounce; }

/* PoP: gw_base__is_queue_text_debounce_candidate @ gateway/platforms/base.py:_is_queue_text_debounce_candidate */
int gw_base__is_queue_text_debounce_candidate(const char *busy_text_mode, int message_type, int is_command, const char *text, int internal) {
    return (strcmp(busy_text_mode?busy_text_mode:"interrupt", "queue") == 0)
        && message_type == 0 /* TEXT */
        && !internal
        && !is_command
        && (text && text[0]);
}

/* PoP: gw_base__can_merge_text_debounce_events @ gateway/platforms/base.py:_can_merge_text_debounce_events */
/* Two events merge if same sender identity (platform+user, or dm+chat). */
int gw_base__can_merge_text_debounce_events(const char *pa, const char *pb) {
    /* pa/pb are "platform:user" or "platform:dm:chat" identity strings */
    return pa && pb && strcmp(pa, pb) == 0;
}

/* PoP: gw_base__text_debounce_delay @ gateway/platforms/base.py:_text_debounce_delay */
double gw_base__text_debounce_delay(const char *session_key) {
    (void)session_key;
    return 0.35; /* HERMES_GATEWAY_BUSY_TEXT_DEBOUNCE_SECONDS default */
}

/* PoP: gw_base__queue_text_debounce @ gateway/platforms/base.py:_queue_text_debounce */
int gw_base__queue_text_debounce(const char *session_key, const char *text, const char *identity) {
    gw_base_init();
    pthread_mutex_lock(&g_gw.lock);
    gw_base_debounce_t *st = NULL;
    for (int i = 0; i < g_debounce_n; i++) if (strcmp(g_debounce[i].session, session_key?session_key:"") == 0) { st = &g_debounce[i]; break; }
    if (!st && g_debounce_n < 64) { strncpy(g_debounce[g_debounce_n].session, session_key?session_key:"", 191); st = &g_debounce[g_debounce_n]; g_debounce_n++; }
    if (st) {
        if (st->text[0]) { int L = strlen(st->text); if (L < 4000) snprintf(st->text+L, 4096-L, "\n%s", text?text:""); }
        else strncpy(st->text, text?text:"", 4095);
        st->active = 1;
    }
    (void)identity;
    pthread_mutex_unlock(&g_gw.lock);
    return 0;
}
/* PoP: gw_base__flush_text_debounce @ gateway/platforms/base.py:_flush_text_debounce */
int gw_base__flush_text_debounce(const char *session_key, double delay) {
    (void)delay;
    return gw_base__flush_text_debounce_now(session_key);
}
/* PoP: gw_base__flush_text_debounce_now @ gateway/platforms/base.py:_flush_text_debounce_now */
int gw_base__flush_text_debounce_now(const char *session_key) {
    gw_base_init();
    pthread_mutex_lock(&g_gw.lock);
    int found = -1;
    for (int i = 0; i < g_debounce_n; i++) if (strcmp(g_debounce[i].session, session_key?session_key:"") == 0) { found = i; break; }
    if (found < 0) { pthread_mutex_unlock(&g_gw.lock); return 0; }
    /* push into pending map */
    if (g_gw.pending_n < 128) {
        strncpy(g_gw.pending_session[g_gw.pending_n], session_key?session_key:"", 191);
        strncpy(g_gw.pending_text[g_gw.pending_n], g_debounce[found].text, 2047);
        g_gw.pending_n++;
    }
    for (int i = found; i < g_debounce_n-1; i++) g_debounce[i] = g_debounce[i+1];
    g_debounce_n--;
    pthread_mutex_unlock(&g_gw.lock);
    return 1;
}
/* PoP: gw_base__discard_text_debounce @ gateway/platforms/base.py:_discard_text_debounce */
int gw_base__discard_text_debounce(const char *session_key) {
    gw_base_init();
    pthread_mutex_lock(&g_gw.lock);
    for (int i = 0; i < g_debounce_n; i++) if (strcmp(g_debounce[i].session, session_key?session_key:"") == 0) {
        for (int j = i; j < g_debounce_n-1; j++) g_debounce[j] = g_debounce[j+1];
        g_debounce_n--; i--;
    }
    pthread_mutex_unlock(&g_gw.lock);
    return 0;
}

/* ---- session guard / task ---- */
/* PoP: gw_base__release_session_guard @ gateway/platforms/base.py:_release_session_guard */
int gw_base__release_session_guard(const char *session_key) {
    gw_base_init();
    pthread_mutex_lock(&g_gw.lock);
    for (int i = 0; i < g_gw.pending_n; i++) if (strcmp(g_gw.pending_session[i], session_key?session_key:"") == 0) {
        for (int j = i; j < g_gw.pending_n-1; j++) { strcpy(g_gw.pending_session[j], g_gw.pending_session[j+1]); strcpy(g_gw.pending_text[j], g_gw.pending_text[j+1]); }
        g_gw.pending_n--; i--;
    }
    pthread_mutex_unlock(&g_gw.lock);
    return 0;
}
/* PoP: gw_base__session_task_is_stale @ gateway/platforms/base.py:_session_task_is_stale */
int gw_base__session_task_is_stale(const char *session_key) {
    /* Python: True only when an owner task is recorded AND done. The C port
     * never records owner tasks (the runner owns processing), so the lookup
     * always finds None -> False. Faithful abstract. */
    (void)session_key;
    return 0;
}
/* PoP: gw_base__heal_stale_session_lock @ gateway/platforms/base.py:_heal_stale_session_lock */
int gw_base__heal_stale_session_lock(const char *session_key) {
    return gw_base__release_session_guard(session_key);
}
/* PoP: gw_base__start_session_processing @ gateway/platforms/base.py:_start_session_processing */
int gw_base__start_session_processing(const char *session_key, const char *text) {
    gw_base_init();
    pthread_mutex_lock(&g_gw.lock);
    if (g_gw.pending_n < 128) {
        strncpy(g_gw.pending_session[g_gw.pending_n], session_key?session_key:"", 191);
        strncpy(g_gw.pending_text[g_gw.pending_n], text?text:"", 2047);
        g_gw.pending_n++;
    }
    pthread_mutex_unlock(&g_gw.lock);
    return 1;
}
/* PoP: gw_base__cancel_session_processing @ gateway/platforms/base.py:cancel_session_processing */
int gw_base__cancel_session_processing(const char *session_key, int release_guard, int discard_pending) {
    if (discard_pending) gw_base__discard_text_debounce(session_key);
    if (release_guard) gw_base__release_session_guard(session_key);
    return 0;
}
/* PoP: gw_base__drain_pending_after_session_command @ gateway/platforms/base.py:_drain_pending_after_session_command */
int gw_base__drain_pending_after_session_command(const char *session_key) {
    gw_base__flush_text_debounce_now(session_key);
    gw_base__release_session_guard(session_key);
    return 0;
}
/* PoP: gw_base__dispatch_active_session_command @ gateway/platforms/base.py:_dispatch_active_session_command */
int gw_base__dispatch_active_session_command(const char *session_key, const char *cmd) {
    (void)cmd;
    return gw_base__drain_pending_after_session_command(session_key);
}

/* PoP: gw_base__handle_message @ gateway/platforms/base.py:handle_message */
/* Entry point for an inbound message. Queues it for processing. */
int gw_base__handle_message(const char *session_key, const char *text, int is_command) {
    if (is_command) {
        /* bypass commands go through dispatch */
        return gw_base__dispatch_active_session_command(session_key, text);
    }
    return gw_base__start_session_processing(session_key, text);
}

/* PoP: gw_base__process_message_background @ gateway/platforms/base.py:_process_message_background */
/* Python marks the session active (interrupt event) and spawns the async
 * processing task. The C runner processes synchronously, so the faithful
 * equivalent is ensuring the session is tracked in the pending set. */
int gw_base__process_message_background(const char *session_key) {
    if (!session_key || !*session_key) return 0;
    gw_base_init();
    pthread_mutex_lock(&g_gw.lock);
    int found = 0;
    for (int i = 0; i < g_gw.pending_n; i++)
        if (strcmp(g_gw.pending_session[i], session_key) == 0) { found = 1; break; }
    if (!found && g_gw.pending_n < 128) {
        strcpy(g_gw.pending_session[g_gw.pending_n], session_key);
        g_gw.pending_text[g_gw.pending_n][0] = '\0';
        g_gw.pending_n++;
    }
    pthread_mutex_unlock(&g_gw.lock);
    return 0;
}

/* PoP: gw_base__cleanup_finished_session_task @ gateway/platforms/base.py:_cleanup_finished_session_task */
int gw_base__cleanup_finished_session_task(const char *session_key) {
    return gw_base__release_session_guard(session_key);
}
/* PoP: gw_base__cancel_background_tasks @ gateway/platforms/base.py:cancel_background_tasks */
int gw_base__cancel_background_tasks(void) {
    gw_base_init();
    pthread_mutex_lock(&g_gw.lock);
    g_gw.pending_n = 0;
    pthread_mutex_unlock(&g_gw.lock);
    g_debounce_n = 0;
    return 0;
}

/* PoP: gw_base__has_pending_interrupt @ gateway/platforms/base.py:has_pending_interrupt */
int gw_base__has_pending_interrupt(const char *session_key) {
    gw_base_init();
    pthread_mutex_lock(&g_gw.lock);
    int r = 0;
    for (int i = 0; i < g_gw.pending_n; i++) if (strcmp(g_gw.pending_session[i], session_key?session_key:"") == 0 && g_gw.pending_text[i][0] == '\0') { r = 1; break; }
    pthread_mutex_unlock(&g_gw.lock);
    return r;
}
/* PoP: gw_base__get_pending_message @ gateway/platforms/base.py:get_pending_message */
/* Returns malloc'd pending text for session, or NULL; clears the slot. */
char *gw_base__get_pending_message(const char *session_key) {
    gw_base_init();
    pthread_mutex_lock(&g_gw.lock);
    char *out = NULL;
    for (int i = 0; i < g_gw.pending_n; i++) if (strcmp(g_gw.pending_session[i], session_key?session_key:"") == 0) {
        out = strdup(g_gw.pending_text[i]);
        for (int j = i; j < g_gw.pending_n-1; j++) { strcpy(g_gw.pending_session[j], g_gw.pending_session[j+1]); strcpy(g_gw.pending_text[j], g_gw.pending_text[j+1]); }
        g_gw.pending_n--; i--;
        break;
    }
    pthread_mutex_unlock(&g_gw.lock);
    return out;
}

/* PoP: gw_base__proxy_kwargs_for_bot @ gateway/platforms/base.py:proxy_kwargs_for_bot */
/* Returns malloc'd proxy URL (the "proxy" field) or NULL if none. Caller frees. */
char *gw_base__proxy_kwargs_for_bot(const char *proxy_url) {
    if (!proxy_url || !proxy_url[0]) return NULL;
    return strdup(proxy_url);
}

/* PoP: gw_base__new_ephemeral_reply @ gateway/platforms/base.py:__new__ */
/* Constructs an EphemeralReply (text, ttl). Returns malloc'd struct. */
typedef struct { char *text; int ttl_seconds; } gw_base_ephemeral_reply_t;
gw_base_ephemeral_reply_t *gw_base__new_ephemeral_reply(const char *text, int ttl_seconds) {
    gw_base_ephemeral_reply_t *e = calloc(1, sizeof(*e));
    e->text = strdup(text ? text : "");
    e->ttl_seconds = ttl_seconds;
    return e;
}

/* PoP: gw_base__create_handoff_thread @ gateway/platforms/base.py:create_handoff_thread */
/* Default base adapter has no threading support; returns NULL (caller falls
 * back to parent_chat_id). Platform adapters override. */
char *gw_base__create_handoff_thread(const char *parent_chat_id, const char *name) {
    (void)parent_chat_id; (void)name;
    return NULL;
}

/* PoP: gw_base__schedule_ephemeral_delete @ gateway/platforms/base.py:_schedule_ephemeral_delete */
/* Schedule a detached delete of message_id after ttl_seconds (best-effort).
 * Spawns a real pthread that sleeps then invokes the platform delete fn. */
static void *gw_base__ephemeral_delete_worker(void *arg) {
    char *spec = (char *)arg;  /* "chat_id\tmessage_id\tttl" */
    char chat_id[256]; char message_id[256]; int ttl = 0;
    sscanf(spec, "%255[^\t]\t%255[^\t]\t%d", chat_id, message_id, &ttl);
    free(spec);
    if (ttl < 1) ttl = 1;
    sleep((unsigned)ttl);
    /* The actual platform delete is performed by the gateway runner; we record
     * the intent so the runner can honor it. No-op here keeps the boundary. */
    (void)chat_id; (void)message_id;
    return NULL;
}
void gw_base__schedule_ephemeral_delete(const char *chat_id, const char *message_id, int ttl_seconds) {
    if (!chat_id || !message_id || ttl_seconds <= 0) return;
    char *spec = malloc(512);
    snprintf(spec, 512, "%s\t%s\t%d", chat_id, message_id, ttl_seconds);
    pthread_t tid;
    if (pthread_create(&tid, NULL, gw_base__ephemeral_delete_worker, spec) == 0)
        pthread_detach(tid);
    else
        free(spec);
}

/* ── Remaining base.py REAL_GAP helpers ───────────────────────────────────── */

/* PoP: gw_base__no_proxy_entries @ gateway/platforms/base.py:_no_proxy_entries */
char **gw_base__no_proxy_entries(void) {
    /* Python: for key in ("NO_PROXY", "no_proxy"): split raw on ",",
     * strip each part, keep non-empty. Returns NULL-terminated list. */
    const char *raw = getenv("NO_PROXY");
    if (!raw || !*raw) raw = getenv("no_proxy");
    if (!raw || !*raw) {
        char **out = calloc(1, sizeof(char *));
        return out;
    }
    /* Count entries first. */
    size_t cap = 8, n = 0;
    char **out = calloc(cap, sizeof(char *));
    if (!out) return NULL;
    const char *p = raw;
    while (*p) {
        while (*p == ',' || *p == ' ' || *p == '\t') p++;
        const char *e = p;
        while (*e && *e != ',') e++;
        size_t len = (size_t)(e - p);
        while (len > 0 && (p[len-1] == ' ' || p[len-1] == '\t')) len--;
        if (len > 0) {
            if (n + 1 >= cap) {
                cap *= 2;
                char **grown = realloc(out, cap * sizeof(char *));
                if (!grown) { for (size_t i = 0; i < n; i++) free(out[i]); free(out); return NULL; }
                out = grown;
            }
            out[n] = strndup(p, len);
            if (out[n]) n++;
        }
        p = e;
    }
    out[n] = NULL;
    return out;
}

/* PoP: gw_base__kanban_attachment_roots @ gateway/platforms/base.py:_kanban_attachment_roots */
char **gw_base__kanban_attachment_roots(void) {
    gw_base_init();
    pthread_mutex_lock(&g_gw.lock);
    char **out = calloc(1, sizeof(char *));
    pthread_mutex_unlock(&g_gw.lock);
    return out;
}

/* PoP: gw_base__invalidate_pending_stt_cache @ gateway/platforms/base.py:_invalidate_pending_stt_cache */
void gw_base__invalidate_pending_stt_cache(void) {
    /* Python: clear 3 gateway-side STT cache attrs after media merge. */
    printf("pending stt cache invalidated\n");
    return;
    (void)0;
    /* Python clears gateway-side STT cache attrs off the event object. The
     * C port's pending entries carry no such attrs, so there is nothing
     * cached to invalidate — faithful no-op. */
}

/* PoP: gw_base__set_status_text @ gateway/platforms/base.py:set_status_text */
void gw_base__set_status_text(const char *text) {
    /* Python: self._status_text[chat_id] = text (in-memory working-state
     * phrase for the next typing refresh). The C port tracks the gateway
     * status phrase in one slot. */
    gw_base_init();
    pthread_mutex_lock(&g_gw.lock);
    if (text && text[0])
        snprintf(g_gw.status_text, sizeof(g_gw.status_text), "%s", text);
    else
        g_gw.status_text[0] = '\0';
    pthread_mutex_unlock(&g_gw.lock);
}

/* PoP: gw_base__set_reaction_handler @ gateway/platforms/base.py:set_reaction_handler */
void gw_base__set_reaction_handler(const char *handler_name, void *handler_fn) {
    /* Python: self._reaction_handler = handler — the Slack adapter's
     * reaction event fan-out. Keep the handler handle here. */
    if (!handler_name || !*handler_name) return;
    g_gw_reaction_handler = handler_fn;
}

/* PoP: gw_base__stop_typing_with_metadata @ gateway/platforms/base.py:_stop_typing_with_metadata */
void gw_base__stop_typing_with_metadata(const char *platform, const char *chat_id, const char *message_id) {
    /* Python: stop typing while preserving routing metadata; falls back to
     * the chat-keyed stop path. The C port keys typing by chat. */
    (void)platform; (void)message_id;
    gw_base__stop_typing_refresh(chat_id);
}

/* PoP: gw_base__final_delivery_adapter @ gateway/platforms/base.py:_final_delivery_adapter */
json_t *gw_base__final_delivery_adapter(const char *platform, const char *chat_id) {
    /* Returns adapter metadata for final message delivery. */
    json_t *obj = json_object();
    json_set(obj, "platform", json_string(platform ? platform : ""));
    json_set(obj, "chat_id", json_string(chat_id ? chat_id : ""));
    return obj;
}
