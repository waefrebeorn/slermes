/*
 * media_cache.c — C port of gateway/platforms/media_cache.py
 *
 * Shared mime↔extension dispatch for inbound (downloaded) platform media.
 * Owns DEFAULT_MIME_TO_EXT / DEFAULT_EXT_TO_MIME tables, ext_for_mime /
 * mime_for_ext lookups, and the one-call cache_media_bytes dispatch.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <time.h>
#include "hermes_json.h"
#include "base.h"

/* ── Shared tables ───────────────────────────────────────────────── */

/* Union of the per-adapter maps where the adapters already agree. */
static const struct { const char *mime; const char *ext; } DEFAULT_MIME_TO_EXT[] = {
    { "image/jpeg",     ".jpg" },
    { "image/png",      ".png" },
    { "image/gif",      ".gif" },
    { "image/webp",     ".webp" },
    { "audio/ogg",      ".ogg" },
    { "audio/x-opus+ogg", ".ogg" },
    { "audio/opus",     ".ogg" },
    { "audio/mpeg",     ".mp3" },
    { "audio/mp3",      ".mp3" },
    { "audio/wav",      ".wav" },
    { "audio/mp4",      ".m4a" },
    { "audio/x-m4a",    ".m4a" },
    { "audio/aac",      ".aac" },
    { "video/mp4",      ".mp4" },
    { "application/pdf", ".pdf" },
    { "application/zip", ".zip" },
    { NULL, NULL }
};

static const struct { const char *ext; const char *mime; } DEFAULT_EXT_TO_MIME[] = {
    { ".jpg", "image/jpeg" }, { ".jpeg", "image/jpeg" }, { ".png", "image/png" },
    { ".gif", "image/gif" },  { ".webp", "image/webp" },
    { ".ogg", "audio/ogg" },  { ".mp3", "audio/mpeg" }, { ".wav", "audio/wav" },
    { ".m4a", "audio/mp4" },  { ".aac", "audio/aac" },
    { ".mp4", "video/mp4" },  { ".pdf", "application/pdf" },
    { ".zip", "application/zip" },
    { NULL, NULL }
};

/* PoP: mc_normalize_mime @ gateway/platforms/media_cache.py:_normalize_mime */
void mc_normalize_mime(const char *mime, char *out, size_t out_sz) {
    /* Python: lowercase and strip any ; charset=... style parameters. */
    if (!out || out_sz == 0) return;
    out[0] = '\0';
    if (!mime) return;
    size_t i = 0;
    while (mime[i] && mime[i] != ';' && i + 1 < out_sz) {
        out[i] = (char)tolower((unsigned char)mime[i]);
        i++;
    }
    out[i] = '\0';
    /* strip trailing whitespace */
    while (i > 0 && isspace((unsigned char)out[i - 1])) out[--i] = '\0';
    /* strip leading whitespace */
    size_t lead = 0;
    while (out[lead] && isspace((unsigned char)out[lead])) lead++;
    if (lead) memmove(out, out + lead, strlen(out + lead) + 1);
}

/* PoP: mc_ext_for_mime @ gateway/platforms/media_cache.py:ext_for_mime */
const char *mc_ext_for_mime(const char *mime, const char *overrides_json,
                            int use_defaults, int use_mimetypes,
                            const char *fallback) {
    /* Python: resolve a mime type to a file extension (including the dot).
     * Order: overrides → DEFAULT_MIME_TO_EXT → mimetypes → fallback.
     * overrides_json is a JSON object {mime: ext} of per-adapter overrides. */
    char primary[256];
    mc_normalize_mime(mime, primary, sizeof(primary));
    if (!primary[0]) return fallback;
    /* overrides first */
    if (overrides_json && overrides_json[0] && strcmp(overrides_json, "{}") != 0) {
        json_t *j = json_parse(overrides_json, NULL);
        if (j && j->type == JSON_OBJECT) {
            const char *ext = json_get_str(j, primary, NULL);
            if (ext && ext[0]) { json_free(j); return ext; }
        }
        if (j) json_free(j);
    }
    if (use_defaults) {
        for (int i = 0; DEFAULT_MIME_TO_EXT[i].mime; i++) {
            if (strcmp(DEFAULT_MIME_TO_EXT[i].mime, primary) == 0)
                return DEFAULT_MIME_TO_EXT[i].ext;
        }
    }
    if (use_mimetypes) {
        /* Limited builtin mimetypes fallback: common types not in the
         * default table (kept static; the full Python mimetypes DB is not
         * ported). */
        static const struct { const char *m; const char *e; } EXTRA[] = {
            { "text/plain", ".txt" }, { "text/html", ".html" },
            { "application/json", ".json" }, { "image/svg+xml", ".svg" },
            { "audio/flac", ".flac" }, { "video/webm", ".webm" },
            { "application/xml", ".xml" }, { NULL, NULL }
        };
        for (int i = 0; EXTRA[i].m; i++) {
            if (strcmp(EXTRA[i].m, primary) == 0) return EXTRA[i].e;
        }
    }
    return fallback;
}

/* PoP: mc_mime_for_ext @ gateway/platforms/media_cache.py:mime_for_ext */
const char *mc_mime_for_ext(const char *ext, const char *overrides_json,
                            const char *fallback) {
    /* Python: inverse lookup: file extension → canonical mime type.
     * Order: overrides → DEFAULT_EXT_TO_MIME → fallback. */
    if (!fallback) fallback = "application/octet-stream";
    if (!ext || !ext[0]) return fallback;
    char key[128];
    size_t i = 0;
    while (ext[i] && i + 1 < sizeof(key)) {
        key[i] = (char)tolower((unsigned char)ext[i]);
        i++;
    }
    key[i] = '\0';
    /* strip whitespace */
    size_t lead = 0;
    while (key[lead] && isspace((unsigned char)key[lead])) lead++;
    if (lead) memmove(key, key + lead, strlen(key + lead) + 1);
    size_t end = strlen(key);
    while (end > 0 && isspace((unsigned char)key[end - 1])) key[--end] = '\0';

    if (overrides_json && overrides_json[0] && strcmp(overrides_json, "{}") != 0) {
        json_t *j = json_parse(overrides_json, NULL);
        if (j && j->type == JSON_OBJECT) {
            const char *mime = json_get_str(j, key, NULL);
            if (mime && mime[0]) { json_free(j); return mime; }
        }
        if (j) json_free(j);
    }
    for (int i = 0; DEFAULT_EXT_TO_MIME[i].ext; i++) {
        if (strcmp(DEFAULT_EXT_TO_MIME[i].ext, key) == 0)
            return DEFAULT_EXT_TO_MIME[i].mime;
    }
    return fallback;
}

/* PoP: mc_cache_media_bytes @ gateway/platforms/media_cache.py:cache_media_bytes */
char *mc_cache_media_bytes(const unsigned char *data, size_t len,
                           const char *mime, const char *filename_hint,
                           const char *kind_hint, const char *ext_overrides) {
    /* Python: cache downloaded media bytes and return the local file path.
     * Picks image / audio / document cache primitive based on mime class. */
    if (!data || len == 0) return NULL;
    char primary[256];
    mc_normalize_mime(mime, primary, sizeof(primary));
    const char *kind = kind_hint;
    if (!kind || !kind[0]) {
        if (strncmp(primary, "image/", 6) == 0) kind = "image";
        else if (strncmp(primary, "audio/", 6) == 0) kind = "audio";
        else kind = "document";
    }
    if (strcmp(kind, "image") == 0) {
        const char *ext = mc_ext_for_mime(primary, ext_overrides, 1, 1, ".jpg");
        if (!ext || !ext[0]) ext = ".jpg";
        return cache_image_from_bytes(data, len, ext);
    }
    if (strcmp(kind, "audio") == 0) {
        const char *ext = mc_ext_for_mime(primary, ext_overrides, 1, 1, ".ogg");
        if (!ext || !ext[0]) ext = ".ogg";
        return cache_audio_from_bytes(data, len, ext);
    }
    /* document */
    char filename[512];
    if (filename_hint && filename_hint[0]) {
        snprintf(filename, sizeof(filename), "%s", filename_hint);
    } else {
        const char *ext = mc_ext_for_mime(primary, ext_overrides, 1, 1, ".bin");
        if (!ext || !ext[0]) ext = ".bin";
        unsigned int r = (unsigned int)time(NULL) ^ ((unsigned int)(uintptr_t)data);
        snprintf(filename, sizeof(filename), "file_%08x%s", r, ext);
    }
    return cache_document_from_bytes(data, len, filename);
}
