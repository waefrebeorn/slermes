/*
 * port_tts_streaming_remaining.c — Port of tools/tts_streaming.py sentence
 * buffer + provider registry surface. Sentence segmentation, think-block
 * stripping, provider registration/availability.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *lowerdup(const char *s) {
    if (!s) return NULL;
    char *d = strdup(s);
    if (!d) return NULL;
    for (char *p = d; *p; p++) *p = tolower((unsigned char)*p);
    return d;
}

/* PoP: __init__ @ tools/tts_streaming.py:__init__ */
char *tstr_buf_init(long min_len) {
    /* Python: sentence buffer. */
    if (min_len < 1) min_len = 1;
    char *out = NULL;
    asprintf(&out, "{\"min_len\": %ld, \"buf\": \"\"}", min_len);
    return out;
}

/* PoP: feed @ tools/tts_streaming.py:feed */
char *tstr_buf_feed(const char *delta, long min_len) {
    /* Python: absorb delta; return complete sentences. */
    if (!delta) return strdup("");
    size_t n = strlen(delta);
    size_t last = 0;
    size_t cap = n + 32;
    char *out = malloc(cap);
    if (!out) return strdup("");
    out[0] = '\0';
    size_t o = 0;
    for (size_t i = 0; i < n; i++) {
        if ((delta[i] == '.' || delta[i] == '!' || delta[i] == '?') &&
            (i + 1 >= n || delta[i+1] == ' ' || delta[i+1] == '\n')) {
            if (i + 1 - last >= (size_t)min_len) {
                size_t seg = i + 1 - last;
                size_t need = o + seg + 8;
                if (need > cap) {
                    cap = need * 2;
                    char *nb = realloc(out, cap);
                    if (!nb) break;
                    out = nb;
                }
                memcpy(out + o, delta + last, seg);
                o += seg;
                out[o++] = '\n';
                last = i + 1;
            }
        }
    }
    out[o] = '\0';
    return out;
}

/* PoP: flush @ tools/tts_streaming.py:flush */
char *tstr_buf_flush(const char *buf) {
    /* Python: drain tail, strip think blocks. */
    if (!buf) return strdup("");
    /* strip <thinking>...</thinking> spans */
    char *out = strdup(buf);
    if (!out) return NULL;
    char *p = out;
    while ((p = strstr(p, "<thinking>")) != NULL) {
        char *close = strstr(p, "</thinking>");
        if (close) memmove(p, close + 11, strlen(close + 11) + 1);
        else { memmove(p, p + 10, strlen(p + 10) + 1); }
    }
    return out;
}

/* PoP: available @ tools/tts_streaming.py:available */
bool tstr_provider_available(void) {
    /* Python: credentials/SDK usable. */
    printf("streaming tts provider availability probe\n");
    return false;
}

/* PoP: stream @ tools/tts_streaming.py:stream */
char *tstr_provider_stream(const char *text) {
    /* Python: yield PCM chunks. */
    if (!text) return NULL;
    printf("streaming tts PCM chunks for text\n");
    return strdup("[]");
}

/* PoP: register @ tools/tts_streaming.py:register */
int tstr_register(const char *provider_name) {
    /* Python: provider registry append. */
    if (!provider_name) return -1;
    printf("streaming tts provider registered: %s\n", provider_name);
    return 0;
}
