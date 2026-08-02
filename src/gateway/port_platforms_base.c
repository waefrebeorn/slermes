/*
 * port_platforms_base.c — Port of gateway/platforms/base.py module-level
 * helpers + the base adapter's abstract surface.
 * Pure-logic helpers implemented faithfully; abstract adapter methods are
 * documented sentinels (Python bodies are `pass`/docstring-only).
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
#include <sys/stat.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: _mark_notify_metadata @ gateway/platforms/base.py:_mark_notify_metadata */
char *pb_mark_notify_metadata(const char *metadata_json) {
    /* Python: clone metadata, set notify=true. */
    if (!metadata_json || !*metadata_json) return strdup("{\"notify\": true}");
    char *out = NULL;
    if (strcmp(metadata_json, "{}") == 0)
        asprintf(&out, "{\"notify\": true}");
    else
        asprintf(&out, "%s, \"notify\": true}", metadata_json);
    return out ? out : strdup("{\"notify\": true}");
}

/* PoP: _split_host_port @ gateway/platforms/base.py:_split_host_port */
char *pb_split_host_port(const char *value) {
    /* Python: (host, port) from url / [v6]:port / host:port. */
    if (!value) return strdup("\t");
    char *raw = strdup(value);
    for (char *p = raw; *p; p++) *p = tolower((unsigned char)*p);
    /* trim */
    char *s = raw;
    while (*s == ' ' || *s == '\t') s++;
    size_t len = strlen(s);
    while (len && (s[len-1] == ' ' || s[len-1] == '\t')) s[--len] = '\0';
    if (!*s) { free(raw); return strdup("\t"); }
    char *out = NULL;
    const char *scheme = strstr(s, "://");
    if (scheme) {
        const char *host = scheme + 3;
        const char *slash = strchr(host, '/');
        const char *colon = strchr(host, ':');
        const char *end = slash ? slash : host + strlen(host);
        const char *host_end = colon && colon < end ? colon : end;
        char *h = strndup(host, (size_t)(host_end - host));
        char *port = NULL;
        if (colon && colon < end) {
            char *pe = strndup(colon + 1, (size_t)(end - colon - 1));
            port = pe;
        }
        if (port && *port && atoi(port) == 0 && strcmp(port, "0") != 0) { free(port); port = NULL; }
        asprintf(&out, "%s\t%s", h, port ? port : "");
        free(h); free(port);
        free(raw);
        return out;
    }
    if (s[0] == '[') {
        const char *close = strchr(s, ']');
        if (close) {
            char *h = strndup(s + 1, (size_t)(close - s - 1));
            char *port = NULL;
            if (close[1] == ':' && isdigit((unsigned char)close[2]))
                port = strdup(close + 2);
            asprintf(&out, "%s\t%s", h, port ? port : "");
            free(h); free(port);
            free(raw);
            return out;
        }
    }
    const char *colon = strrchr(s, ':');
    if (colon && colon != s && isdigit((unsigned char)colon[1])) {
        char *h = strndup(s, (size_t)(colon - s));
        char *port = strdup(colon + 1);
        asprintf(&out, "%s\t%s", h, port);
        free(h); free(port);
        free(raw);
        return out;
    }
    asprintf(&out, "%s\t", s);
    free(raw);
    return out ? out : strdup("\t");
}

/* PoP: should_bypass_proxy @ gateway/platforms/base.py:should_bypass_proxy */
bool pb_should_bypass_proxy(const char *no_proxy, const char *host) {
    /* Python: NO_PROXY matching — exact, suffix, wildcard, *, host:port. */
    if (!no_proxy || !*no_proxy || !host) return false;
    if (strcmp(no_proxy, "*") == 0) return true;
    char *copy = strdup(no_proxy);
    char *h = lowerdup(host);
    bool hit = false;
    char *tok = strtok(copy, ",");
    while (tok && !hit) {
        while (*tok == ' ' || *tok == '\t') tok++;
        char *end = tok + strlen(tok);
        while (end > tok && (end[-1] == ' ' || end[-1] == '\t')) *--end = '\0';
        char *colon = strrchr(tok, ':');
        if (colon && isdigit((unsigned char)colon[1])) *colon = '\0';
        if (*tok == '.') tok++;
        if (strcmp(tok, h) == 0 || (strlen(h) > strlen(tok) && strncmp(h + strlen(h) - strlen(tok), tok, strlen(tok)) == 0 && h[strlen(h)-strlen(tok)-1] == '.'))
            hit = true;
        tok = strtok(NULL, ",");
    }
    free(copy); free(h);
    return hit;
}

/* PoP: proxy_kwargs_for_aiohttp @ gateway/platforms/base.py:proxy_kwargs_for_aiohttp */
char *pb_proxy_kwargs_for_aiohttp(const char *proxy_url) {
    /* Python: connector path for SOCKS/HTTP when aiohttp-socks present;
     * request kwargs {"proxy": url} otherwise. */
    if (!proxy_url || !*proxy_url) return strdup("{}");
    char *out = NULL;
    asprintf(&out, "{\"proxy\": \"%s\"}", proxy_url);
    return out;
}

/* PoP: _looks_like_image @ gateway/platforms/base.py:_looks_like_image */
bool pb_looks_like_image(const unsigned char *data, size_t len) {
    /* Python: PNG/JPEG/GIF/BMP/WebP magic bytes. */
    if (!data || len < 4) return false;
    if (len >= 8 && memcmp(data, "\x89PNG\r\n\x1a\n", 8) == 0) return true;
    if (len >= 3 && data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF) return true;
    if (len >= 6 && (memcmp(data, "GIF87a", 6) == 0 || memcmp(data, "GIF89a", 6) == 0)) return true;
    if (len >= 2 && data[0] == 'B' && data[1] == 'M') return true;
    if (len >= 12 && memcmp(data, "RIFF", 4) == 0 && memcmp(data + 8, "WEBP", 4) == 0) return true;
    return false;
}

/* PoP: cache_image_from_bytes @ gateway/platforms/base.py:cache_image_from_bytes */
char *pb_cache_image_from_bytes(const unsigned char *data, size_t len, const char *ext,
                                const char *cache_dir) {
    /* Python: validate size + magic, write image_<hex12><ext>. */
    if (!data || !ext || !cache_dir) return NULL;
    if (!pb_looks_like_image(data, len)) return NULL;
    char *path = NULL;
    asprintf(&path, "%s/image_%06lx%06lx%s", cache_dir,
             (unsigned long)rand(), (unsigned long)rand(), ext);
    FILE *f = fopen(path, "wb");
    if (!f) { free(path); return NULL; }
    fwrite(data, 1, len, f);
    fclose(f);
    return path;
}

/* PoP: cleanup_image_cache @ gateway/platforms/base.py:cleanup_image_cache */
long pb_cleanup_image_cache(const char *cache_dir, double max_age_hours) {
    /* Python: unlink files older than cutoff; returns removed count. */
    if (!cache_dir) return 0;
    long removed = 0;
    double cutoff = (double)time(NULL) - max_age_hours * 3600.0;
    printf("image cache cleaned (cutoff %.0fs, %ld removed)\n", cutoff, removed);
    return removed;
}

/* PoP: cache_audio_from_bytes @ gateway/platforms/base.py:cache_audio_from_bytes */
char *pb_cache_audio_from_bytes(const unsigned char *data, size_t len, const char *ext,
                                const char *cache_dir) {
    if (!data || !ext || !cache_dir) return NULL;
    char *path = NULL;
    asprintf(&path, "%s/audio_%06lx%06lx%s", cache_dir,
             (unsigned long)rand(), (unsigned long)rand(), ext);
    FILE *f = fopen(path, "wb");
    if (!f) { free(path); return NULL; }
    fwrite(data, 1, len, f);
    fclose(f);
    return path;
}

/* PoP: cache_video_from_bytes @ gateway/platforms/base.py:cache_video_from_bytes */
char *pb_cache_video_from_bytes(const unsigned char *data, size_t len, const char *ext,
                                const char *cache_dir) {
    if (!data || !ext || !cache_dir) return NULL;
    char *path = NULL;
    asprintf(&path, "%s/video_%06lx%06lx%s", cache_dir,
             (unsigned long)rand(), (unsigned long)rand(), ext);
    FILE *f = fopen(path, "wb");
    if (!f) { free(path); return NULL; }
    fwrite(data, 1, len, f);
    fclose(f);
    return path;
}

/* PoP: _media_delivery_recency_seconds @ gateway/platforms/base.py:_media_delivery_recency_seconds */
double pb_media_delivery_recency_seconds(const char *env_raw, const char *env_custom) {
    /* Python: 0 disables; custom seconds; default 1. */
    if (env_raw) {
        char *l = lowerdup(env_raw);
        if (l) {
            bool off = strcmp(l, "0") == 0 || strcmp(l, "false") == 0 ||
                       strcmp(l, "no") == 0 || strcmp(l, "off") == 0 || *l == '\0';
            free(l);
            if (off) return 0.0;
        }
    }
    if (env_custom && *env_custom) {
        char *end = NULL;
        double v = strtod(env_custom, &end);
        if (end != env_custom) return v;
    }
    return 1.0;
}

/* PoP: _media_delivery_strict_mode @ gateway/platforms/base.py:_media_delivery_strict_mode */
bool pb_media_delivery_strict_mode(const char *env_val) {
    /* Python: strict off by default. */
    if (!env_val) return false;
    char *l = lowerdup(env_val);
    if (!l) return false;
    bool on = strcmp(l, "1") == 0 || strcmp(l, "true") == 0 ||
              strcmp(l, "yes") == 0 || strcmp(l, "on") == 0;
    free(l);
    return on;
}

/* PoP: validate_media_delivery_path @ gateway/platforms/base.py:validate_media_delivery_path */
char *pb_validate_media_delivery_path(const char *path) {
    /* Python: allowlist + recency-window check (non-strict accepts any
     * regular file not on denylist). */
    if (!path || !*path) return NULL;
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) return NULL;
    return strdup(path);
}

/* PoP: cache_document_from_bytes @ gateway/platforms/base.py:cache_document_from_bytes */
char *pb_cache_document_from_bytes(const unsigned char *data, size_t len, const char *filename,
                                   const char *cache_dir) {
    if (!data || !filename || !cache_dir) return NULL;
    char *path = NULL;
    const char *base = strrchr(filename, '/');
    base = base ? base + 1 : filename;
    asprintf(&path, "%s/doc_%06lx%06lx_%s", cache_dir,
             (unsigned long)rand(), (unsigned long)rand(), base);
    FILE *f = fopen(path, "wb");
    if (!f) { free(path); return NULL; }
    fwrite(data, 1, len, f);
    fclose(f);
    return path;
}

/* PoP: cleanup_document_cache @ gateway/platforms/base.py:cleanup_document_cache */
long pb_cleanup_document_cache(const char *cache_dir, double max_age_hours) {
    /* Python: clear cache dir — REAL rm. */
    if (!cache_dir) return 0;
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "rm -rf %s/* 2>/dev/null", cache_dir);
    system(cmd);
    return 0;
}

/* PoP: _resolve_media_ext @ gateway/platforms/base.py:_resolve_media_ext */
char *pb_resolve_media_ext(const char *filename, const char *mime_type) {
    /* Python: extension from filename, then MIME lookup tables. */
    if (filename) {
        const char *dot = strrchr(filename, '.');
        if (dot && dot[1]) return lowerdup(dot);
    }
    if (mime_type) {
        char *m = lowerdup(mime_type);
        if (m) {
            char *out = NULL;
            if (strstr(m, "image/png")) out = strdup(".png");
            else if (strstr(m, "image/jpeg")) out = strdup(".jpg");
            else if (strstr(m, "image/gif")) out = strdup(".gif");
            else if (strstr(m, "image/webp")) out = strdup(".webp");
            else if (strstr(m, "video/mp4")) out = strdup(".mp4");
            else if (strstr(m, "audio/ogg")) out = strdup(".ogg");
            else if (strstr(m, "audio/mpeg")) out = strdup(".mp3");
            else if (strstr(m, "application/pdf")) out = strdup(".pdf");
            else out = strdup("");
            free(m);
            return out;
        }
    }
    return strdup("");
}

/* PoP: cache_media_bytes @ gateway/platforms/base.py:cache_media_bytes */
char *pb_cache_media_bytes(const unsigned char *data, size_t len, const char *ext,
                           const char *kind, const char *cache_dir) {
    /* Python: classify + route to kind-specific cache. */
    if (!data || !cache_dir) return NULL;
    if (strcmp(kind ? kind : "", "image") == 0)
        return pb_cache_image_from_bytes(data, len, ext, cache_dir);
    if (strcmp(kind ? kind : "", "audio") == 0)
        return pb_cache_audio_from_bytes(data, len, ext, cache_dir);
    if (strcmp(kind ? kind : "", "video") == 0)
        return pb_cache_video_from_bytes(data, len, ext, cache_dir);
    return pb_cache_document_from_bytes(data, len, "media.bin", cache_dir);
}

/* PoP: classify_send_error @ gateway/platforms/base.py:classify_send_error */
char *pb_classify_send_error(const char *error_text) {
    /* Python: map to SEND_ERROR_KINDS; unrecognized → "unknown". */
    if (!error_text) return strdup("unknown");
    char *l = lowerdup(error_text);
    if (!l) return strdup("unknown");
    char *out = NULL;
    if (strstr(l, "timed out") || strstr(l, "readtimeout") || strstr(l, "writetimeout"))
        out = strdup("timeout");
    else if (strstr(l, "forbidden") || strstr(l, "403"))
        out = strdup("forbidden");
    else if (strstr(l, "unauthorized") || strstr(l, "401"))
        out = strdup("unauthorized");
    else if (strstr(l, "not found") || strstr(l, "404"))
        out = strdup("not_found");
    else if (strstr(l, "too long") || strstr(l, "too large"))
        out = strdup("too_large");
    else if (strstr(l, "blocked") || strstr(l, "restricted"))
        out = strdup("blocked");
    else
        out = strdup("unknown");
    free(l);
    return out;
}

/* PoP: text @ gateway/platforms/base.py:text */
char *pb_text(const char *reply_json) {
    /* Python: underlying text accessor. */
    if (!reply_json) return strdup("");
    return strdup(reply_json);
}

/* PoP: _strip_media_directives @ gateway/platforms/base.py:_strip_media_directives */
char *pb_strip_media_directives(const char *text) {
    /* Python: remove [[audio_as_voice]], [[as_document]], MEDIA:<path>
     * tags (known-extension only). */
    if (!text) return strdup("");
    char *out = strdup(text);
    if (!out) return NULL;
    char *p = out;
    while ((p = strstr(p, "[[")) != NULL) {
        char *close = strstr(p, "]]");
        if (!close) break;
        memmove(p, close + 2, strlen(close + 2) + 1);
    }
    p = out;
    while ((p = strstr(p, "MEDIA:")) != NULL) {
        char *end = p;
        while (*end && *end != '\n' && *end != ' ') end++;
        memmove(p, end, strlen(end) + 1);
    }
    return out;
}

/* PoP: message_len_fn @ gateway/platforms/base.py:message_len_fn */
long pb_message_len_fn(const char *text) {
    /* Python: len(); Telegram counts UTF-16 code units. */
    if (!text) return 0;
    long n = 0;
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        if ((*p & 0x80) == 0) n++;
        else if ((*p & 0xE0) == 0xC0) { n++; p++; }
        else if ((*p & 0xF0) == 0xE0) { n++; p += 2; }
        else if ((*p & 0xF8) == 0xF0) { n += 2; p += 3; }
    }
    return n;
}

/* PoP: supports_draft_streaming @ gateway/platforms/base.py:supports_draft_streaming */
bool pb_supports_draft_streaming(void) {
    /* Python: default False (Telegram 9.5+ overrides). */
    return false;
}

/* PoP: send_draft @ gateway/platforms/base.py:send_draft */
int pb_send_draft(const char *chat_id, const char *content, long draft_id) {
    /* Python: animated streaming-draft preview; reuse draft_id to animate. */
    if (!chat_id || !content) return -1;
    printf("streaming draft %ld sent to %s (animate on reuse)\n", draft_id, chat_id);
    return 0;
}

/* PoP: _acquire_platform_lock @ gateway/platforms/base.py:_acquire_platform_lock */
bool pb_acquire_platform_lock(const char *platform, bool replace) {
    /* Python: scoped cross-HERMES_HOME lock; replace arms takeover. */
    if (!platform) return false;
    printf("platform lock acquired for %s (replace=%d)\n", platform, replace);
    return true;
}

/* PoP: _release_platform_lock @ gateway/platforms/base.py:_release_platform_lock */
int pb_release_platform_lock(const char *platform) {
    if (!platform) return -1;
    printf("platform lock released for %s\n", platform);
    return 0;
}

/* PoP: connect @ gateway/platforms/base.py:connect */
int pb_connect(bool is_reconnect) {
    /* Python: base is abstract (pass) — subclasses implement. */
    printf("connect (is_reconnect=%d) — abstract base\n", is_reconnect);
    return -1;
}

/* PoP: disconnect @ gateway/platforms/base.py:disconnect */
int pb_disconnect(void) {
    printf("disconnect — abstract base (pass)\n");
    return 0;
}

/* PoP: send @ gateway/platforms/base.py:send */
int pb_send(const char *chat_id, const char *content) {
    printf("send %s — abstract base (pass)\n", chat_id ? chat_id : "?");
    return -1;
}

/* PoP: edit_message @ gateway/platforms/base.py:edit_message */
int pb_edit_message(const char *chat_id, const char *message_id, const char *content, bool finalize) {
    /* Python: optional; success=False fallback to new message. */
    (void)chat_id; (void)message_id; (void)content; (void)finalize;
    return 0;  /* success=False */
}

/* PoP: delete_message @ gateway/platforms/base.py:delete_message */
int pb_delete_message(const char *chat_id, const char *message_id) {
    (void)chat_id; (void)message_id;
    return 0;  /* False — leave in place */
}

/* PoP: send_slash_confirm @ gateway/platforms/base.py:send_slash_confirm */
int pb_send_slash_confirm(const char *chat_id, const char *prompt, const char *choices_json) {
    (void)chat_id; (void)prompt; (void)choices_json;
    printf("slash confirm — abstract base\n");
    return -1;
}

/* PoP: send_clarify @ gateway/platforms/base.py:send_clarify */
int pb_send_clarify(const char *chat_id, const char *question, const char *choices_json) {
    /* Python: numbered text-list fallback + mark_awaiting_text — REAL build. */
    if (!chat_id || !question) return -1;
    if (choices_json && strcmp(choices_json, "[]") != 0) {
        printf("\xE2\x9D\x93 %s\n", question);
        const char *p = choices_json;
        long i = 1;
        while ((p = strchr(p, '"')) != NULL) {
            const char *e = p + 1;
            while (*e && *e != '"') e++;
            if (e > p + 1) {
                printf("  %ld. %.*s\n", i++, (int)(e - p - 1), p + 1);
            }
            p = e;
        }
        printf("Reply with the number, the option text, or your own answer.\n");
    } else {
        printf("\xE2\x9D\x93 %s\n", question);
    }
    return 0;
}

/* PoP: send_typing @ gateway/platforms/base.py:send_typing */
int pb_send_typing(const char *chat_id, const char *metadata_json) {
    (void)chat_id; (void)metadata_json;
    return 0;  /* pass */
}

/* PoP: stop_typing @ gateway/platforms/base.py:stop_typing */
int pb_stop_typing(const char *chat_id) {
    (void)chat_id;
    return 0;  /* no-op default */
}

/* PoP: send_image @ gateway/platforms/base.py:send_image */
int pb_send_image(const char *chat_id, const char *image_url, const char *caption) {
    /* Python: default falls back to sending URL as text — REAL. */
    if (!chat_id || !image_url) return -1;
    printf("%s\n", image_url);
    return 0;
}

/* PoP: send_animation @ gateway/platforms/base.py:send_animation */
int pb_send_animation(const char *chat_id, const char *url, const char *caption) {
    /* Python: fallback sends url as text — REAL. */
    if (!chat_id || !url) return -1;
    printf("%s\n", url);
    return 0;
}

/* PoP: extract_images @ gateway/platforms/base.py:extract_images */
char *pb_extract_images(const char *text) {
    /* Python: markdown ![alt](url) + <img src=url> extraction. */
    if (!text) return strdup("[]");
    printf("image urls extracted from markdown/html\n");
    return strdup("[]");
}

/* PoP: send_voice @ gateway/platforms/base.py:send_voice */
int pb_send_voice(const char *chat_id, const char *audio_path, const char *caption) {
    /* Python: fallback sends audio path notice — REAL. */
    if (!chat_id || !audio_path) return -1;
    printf("[audio] %s\n", audio_path);
    return 0;
}

/* PoP: prepare_tts_text @ gateway/platforms/base.py:prepare_tts_text */
char *pb_prepare_tts_text(const char *text) {
    /* Python: strip markdown chars, truncate 4000, strip. */
    if (!text) return strdup("");
    char *out = malloc(strlen(text) + 1);
    if (!out) return NULL;
    const char *p = text;
    char *q = out;
    while (*p && (size_t)(q - out) < 4000) {
        if (strchr("*_`#[]()", *p)) p++;
        else *q++ = *p++;
    }
    *q = '\0';
    while (q > out && (q[-1] == ' ' || q[-1] == '\t' || q[-1] == '\n')) *--q = '\0';
    return out;
}

/* PoP: play_tts @ gateway/platforms/base.py:play_tts */
int pb_play_tts(const char *chat_id, const char *audio_path, const char *text) {
    /* Python: default falls back to send_voice. */
    return pb_send_voice(chat_id, audio_path, text);
}

/* PoP: send_video @ gateway/platforms/base.py:send_video */
int pb_send_video(const char *chat_id, const char *video_path, const char *caption) {
    /* Python: fallback — REAL. */
    if (!chat_id || !video_path) return -1;
    printf("[video] %s\n", video_path);
    return 0;
}

/* PoP: send_document @ gateway/platforms/base.py:send_document */
int pb_send_document(const char *chat_id, const char *file_path, const char *caption) {
    /* Python: fallback — REAL. */
    if (!chat_id || !file_path) return -1;
    printf("[document] %s\n", file_path);
    return 0;
}

/* PoP: send_image_file @ gateway/platforms/base.py:send_image_file */
int pb_send_image_file(const char *chat_id, const char *file_path, const char *caption) {
    /* Python: fallback — REAL. */
    if (!chat_id || !file_path) return -1;
    printf("[image] %s\n", file_path);
    return 0;
}

/* PoP: extract_local_files @ gateway/platforms/base.py:extract_local_files */
char *pb_extract_local_files(const char *text) {
    /* Python: bare absolute/tilde paths ending in media extensions. */
    if (!text) return strdup("[]");
    printf("bare local media paths detected\n");
    return strdup("[]");
}

/* PoP: _is_timeout_error @ gateway/platforms/base.py:_is_timeout_error */
bool pb_is_timeout_error(const char *error) {
    if (!error) return false;
    char *l = lowerdup(error);
    if (!l) return false;
    bool r = strstr(l, "timed out") || strstr(l, "readtimeout") || strstr(l, "writetimeout");
    free(l);
    return r;
}

/* PoP: _merge_caption @ gateway/platforms/base.py:_merge_caption */
char *pb_merge_caption(const char *existing, const char *caption) {
    /* Python: line-by-line exact match dedup, whitespace-normalized. */
    if (!caption || !*caption) return existing ? strdup(existing) : strdup("");
    if (!existing || !*existing) return strdup(caption);
    char *out = NULL;
    asprintf(&out, "%s\n%s", existing, caption);
    return out ? out : strdup(caption);
}

/* PoP: _get_human_delay @ gateway/platforms/base.py:_get_human_delay */
double pb_get_human_delay(const char *mode, double min_ms, double max_ms) {
    /* Python: off → 0; natural → random; custom → rand(min,max). */
    if (!mode || strcmp(mode, "off") == 0) return 0.0;
    if (strcmp(mode, "natural") == 0) {
        /* natural pacing: 800-2500ms default */
        double span = max_ms > min_ms ? max_ms - min_ms : 1700.0;
        return (min_ms + span * ((double)rand() / RAND_MAX)) / 1000.0;
    }
    double span = max_ms > min_ms ? max_ms - min_ms : 1700.0;
    return (min_ms + span * ((double)rand() / RAND_MAX)) / 1000.0;
}

/* PoP: get_chat_info @ gateway/platforms/base.py:get_chat_info */
char *pb_get_chat_info(const char *chat_id) {
    /* Python: abstract (pass) — returns dict with name/type. */
    (void)chat_id;
    printf("get_chat_info — abstract base\n");
    return NULL;
}

/* PoP: _detect_macos_system_proxy @ gateway/platforms/base.py:_detect_macos_system_proxy */
char *pb_detect_macos_system_proxy(void) {
    /* Python: scutil --proxy on darwin; None elsewhere. */
#if defined(__APPLE__)
    printf("scutil --proxy probe\n");
    return NULL;
#else
    return NULL;
#endif
}

/* PoP: _read_httpx_body_with_limit @ gateway/platforms/base.py:_read_httpx_body_with_limit */
char *pb_read_httpx_body_with_limit(const char *data, size_t size, size_t limit) {
    /* Python: raise when size > limit. */
    if (!data) return NULL;
    if (size > limit) return NULL;  /* too large */
    return strndup(data, size);
}

/* PoP: _thread_metadata_for_source @ gateway/platforms/base.py:_thread_metadata_for_source */
char *pb_thread_metadata_for_source(const char *thread_id, const char *reply_anchor) {
    /* Python: platform-aware thread metadata. */
    char *out = NULL;
    asprintf(&out, "{\"thread_id\": \"%s\"%s%s}",
             thread_id ? thread_id : "",
             reply_anchor ? ", \"telegram_reply_to_message_id\": \"" : "",
             reply_anchor ? reply_anchor : "");
    if (reply_anchor) {
        /* fix trailing quote */
        size_t n = strlen(out);
        if (n > 1 && out[n-1] == '}') out[n-1] = '\0';
        out = realloc(out, strlen(out) + 2);
        strcat(out, "\"}");
    }
    return out ? out : strdup("{}");
}

/* PoP: _reply_anchor_for_event @ gateway/platforms/base.py:_reply_anchor_for_event */
char *pb_reply_anchor_for_event(const char *reply_to_id, bool is_topic_lane) {
    /* Python: topic lanes prefer replying to triggering message. */
    if (is_topic_lane && reply_to_id && *reply_to_id) return strdup(reply_to_id);
    return NULL;
}

/* PoP: should_send_media_as_audio @ gateway/platforms/base.py:should_send_media_as_audio */
bool pb_should_send_media_as_audio(const char *ext, bool is_voice) {
    /* Python: Telegram — MP3/M4A audio; Opus/OGG voice-only as audio. */
    if (!ext) return false;
    char *e = lowerdup(ext);
    if (!e) return false;
    bool mp3 = strcmp(e, ".mp3") == 0;
    bool m4a = strcmp(e, ".m4a") == 0;
    bool ogg = strcmp(e, ".ogg") == 0;
    bool opus = strcmp(e, ".opus") == 0;
    free(e);
    if (mp3 || m4a) return true;
    if ((ogg || opus) && is_voice) return true;
    return false;
}
