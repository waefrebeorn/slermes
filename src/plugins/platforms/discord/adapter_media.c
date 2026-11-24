/*
 * adapter_media.c - real C11 port of the pure media helpers in upstream Hermes
 *   plugins/platforms/discord/adapter_media.py.
 * The async send paths (aiohttp, discord.py, ffmpeg) stay in Python; what is ported is the
 * Ogg/Opus duration reader, the attachment-cap policy and the oversized-upload preflight.
 *
 * PoP: w19_adaptermed_c40d0e_ogg_opus_duration @ plugins/platforms/discord/adapter_media.py:_ogg_opus_duration
 * PoP: w19_adaptermed_c40d0e_upload_limit @ plugins/platforms/discord/adapter_media.py:_discord_upload_limit_bytes
 * PoP: w19_adaptermed_c40d0e_oversized @ plugins/platforms/discord/adapter_media.py:_reject_oversized_upload
 * PoP: w19_adaptermed_c40d0e_reply_count @ plugins/platforms/discord/adapter_media.py:reply_message_count
 */
#include "hermes_json.h"
#include "noc_w19_adaptermed_c40d0e.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_UPLOAD_LIMIT (20LL * 1024 * 1024)
#define OPUS_GRANULE_RATE 48000.0

static char *format_dup(const char *fmt, const char *s, double a, double b) {
    int n = snprintf(NULL, 0, fmt, s, a, b);
    char *buf;
    if (n < 0) return NULL;
    buf = (char *)malloc((size_t)n + 1);
    if (!buf) return NULL;
    snprintf(buf, (size_t)n + 1, fmt, s, a, b);
    return buf;
}

/* Little-endian read. The 8-byte case is git-style two's complement: shifting the bytes into a
 * signed long long reproduces Python's int.from_bytes(..., signed=True) exactly. */
static long long read_le(const unsigned char *p, int n, int signed_val) {
    long long v = 0;
    int i;
    (void)signed_val;
    for (i = n - 1; i >= 0; i--) v = (v << 8) | (long long)p[i];
    return v;
}

json_t *w19_adaptermed_c40d0e_ogg_opus_duration(const unsigned char *data, unsigned long n) {
    unsigned long head;
    long long pre_skip;
    long long granule = 0;
    int have_granule = 0;
    unsigned long pos = 0;
    double secs;
    if (!data || n < 27) return json_null();
    if (memcmp(data, "OggS", 4) != 0) return json_null();
    head = 27UL + (unsigned long)data[26];
    if (head + 8 > n || memcmp(data + head, "OpusHead", 8) != 0) return json_null();
    if (n < head + 12) return json_null();
    pre_skip = read_le(data + head + 10, 2, 0);
    while (pos + 4 <= n && memcmp(data + pos, "OggS", 4) == 0 && pos + 27 <= n) {
        unsigned long body_start = pos + 27UL + (unsigned long)data[pos + 26];
        long long page_granule;
        long long body_len = 0;
        unsigned long k;
        if (body_start > n) break;
        page_granule = read_le(data + pos + 6, 8, 1);
        if (page_granule != -1) {
            granule = page_granule;
            have_granule = 1;
        }
        for (k = pos + 27; k < body_start; k++) body_len += (long long)data[k];
        pos = (unsigned long)((long long)body_start + body_len);
    }
    if (!have_granule) return json_null();
    secs = ((double)granule - (double)pre_skip) / OPUS_GRANULE_RATE;
    if (!(secs > 0)) return json_null();
    return json_number(secs);
}

json_t *w19_adaptermed_c40d0e_upload_limit(const json_t *guild) {
    if (guild && guild->type == JSON_OBJECT) {
        json_t *limit = json_obj_get(guild, "filesize_limit");
        if (limit && (limit->type == JSON_BOOL || limit->type == JSON_NUMBER)) {
            long long v = (limit->type == JSON_BOOL) ? (limit->bool_val ? 1 : 0)
                                                     : (long long)limit->num_val;
            if (v > 0) return json_number((double)(v > DEFAULT_UPLOAD_LIMIT ? v : DEFAULT_UPLOAD_LIMIT));
        }
    }
    return json_number((double)DEFAULT_UPLOAD_LIMIT);
}

json_t *w19_adaptermed_c40d0e_oversized(const char *filename, long long file_size,
                                        long long limit) {
    json_t *out = json_object();
    /* the preflight always compares against the floored effective cap */
    if (limit <= 0 || limit < DEFAULT_UPLOAD_LIMIT) limit = DEFAULT_UPLOAD_LIMIT;
    if (file_size <= limit) {
        json_set(out, "rejected", json_bool(0));
        json_set(out, "error", json_null());
        return out;
    }
    json_set(out, "rejected", json_bool(1));
    {
        char *msg = format_dup("File too large for Discord upload: %s is %.1f MB (limit %.0f MB)",
                               filename ? filename : "",
                               (double)file_size / (1024.0 * 1024.0),
                               (double)limit / (1024.0 * 1024.0));
        json_set(out, "error", msg ? json_string(msg) : json_null());
        free(msg);
    }
    return out;
}

json_t *w19_adaptermed_c40d0e_reply_count(const char *content, long long max_len) {
    json_t *out = json_object();
    size_t n = content ? strlen(content) : 0;
    long long count = 1;
    if (max_len > 0 && (long long)n > max_len) {
        count = (long long)((n + (size_t)max_len - 1) / (size_t)max_len);
    }
    json_set(out, "count", json_number((double)count));
    return out;
}
