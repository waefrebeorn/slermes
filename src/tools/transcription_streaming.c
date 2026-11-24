/*
 * transcription_streaming.c — faithful C11 port of
 * tools/transcription_streaming.py: Resampler.__call__ and StreamingSession._error.
 *
 * PoP: w18_transcripti_9f815d_resample @ tools/transcription_streaming.py:Resampler.__call__
 * PoP: w18_transcripti_9f815d_error @ tools/transcription_streaming.py:StreamingSession._error
 *
 * Resampler is stateful; `chunks_hex` holds the ordered input chunks (hex-encoded
 * s16le bytes) and the function returns the concatenated output as hex. This
 * mirrors array("h").frombytes / .tobytes on a little-endian host.
 */
#include "noc_w18_transcripti_9f815d.h"
#include "hermes_json.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ---- dynamic signed-16 sample vector ---- */
typedef struct { int16_t *v; size_t n, cap; } smp_t;
static void smp_reserve(smp_t *s, size_t need) {
    if (s->cap >= need) return;
    size_t nc = s->cap ? s->cap * 2 : 16;
    while (nc < need) nc *= 2;
    int16_t *nv = (int16_t *)realloc(s->v, nc * sizeof(int16_t));
    if (nv) { s->v = nv; s->cap = nc; }
}
static void smp_push(smp_t *s, int16_t x) {
    smp_reserve(s, s->n + 1);
    if (s->n < s->cap) s->v[s->n++] = x;
}
static void smp_append(smp_t *s, const int16_t *p, size_t n) {
    smp_reserve(s, s->n + n);
    if (s->n + n <= s->cap) { memcpy(s->v + s->n, p, n * sizeof(int16_t)); s->n += n; }
}

/* ---- dynamic byte buffer ---- */
typedef struct { unsigned char *v; size_t n, cap; } byt_t;
static void byt_reserve(byt_t *b, size_t need) {
    if (b->cap >= need) return;
    size_t nc = b->cap ? b->cap * 2 : 64;
    while (nc < need) nc *= 2;
    unsigned char *nv = (unsigned char *)realloc(b->v, nc);
    if (nv) { b->v = nv; b->cap = nc; }
}
static void byt_push16(byt_t *b, int16_t s) {
    byt_reserve(b, b->n + 2);
    if (b->n + 2 <= b->cap) {
        uint16_t u = (uint16_t)s;
        b->v[b->n++] = (unsigned char)(u & 0xff);
        b->v[b->n++] = (unsigned char)(u >> 8);
    }
}
static void byt_pushbytes(byt_t *b, const unsigned char *p, size_t n) {
    byt_reserve(b, b->n + n);
    if (b->n + n <= b->cap) { memcpy(b->v + b->n, p, n); b->n += n; }
}
static char *byt_hex(const byt_t *b) {
    static const char H[] = "0123456789abcdef";
    char *out = (char *)malloc(b->n * 2 + 1);
    if (out) {
        for (size_t i = 0; i < b->n; ++i) {
            out[i*2]   = H[b->v[i] >> 4];
            out[i*2+1] = H[b->v[i] & 15];
        }
        out[b->n*2] = '\0';
    }
    return out;
}

static int hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
static unsigned char *hex_decode(const char *s, size_t *n) {
    size_t len = strlen(s);
    size_t nb = len / 2;
    unsigned char *out = (unsigned char *)malloc(nb ? nb : 1);
    *n = 0;
    if (!out) return NULL;
    for (size_t i = 0; i < nb; ++i) {
        int hi = hexval(s[i*2]), lo = hexval(s[i*2+1]);
        if (hi < 0 || lo < 0) break;
        out[i] = (unsigned char)((hi << 4) | lo);
        ++(*n);
    }
    return out;
}

/* Python floor division for positive divisor. */
static long long st_floordiv(long long a, long long b) {
    long long q = a / b, r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) --q;
    return q;
}

char *w18_transcripti_9f815d_resample(int src_rate, int dst_rate, const json_t *chunks_hex) {
    int src = src_rate, dst = dst_rate;
    double pos = 0.0;
    smp_t carry = {0};
    byt_t out = {0};

    size_t nchunks = (chunks_hex && chunks_hex->type == JSON_ARRAY) ? json_len(chunks_hex) : 0;
    for (size_t ci = 0; ci < nchunks; ++ci) {
        json_t *node = json_get(chunks_hex, ci);
        const char *hex = (node && node->type == JSON_STRING) ? node->str_val : "";
        size_t nbytes = 0;
        unsigned char *pcm = hex_decode(hex ? hex : "", &nbytes);

        if (src == dst || nbytes == 0) {
            if (pcm && nbytes) byt_pushbytes(&out, pcm, nbytes);
            free(pcm);
            continue;
        }

        size_t even = nbytes - (nbytes % 2);
        size_t nsamp = even / 2;
        smp_t samples = {0};
        smp_reserve(&samples, nsamp);
        for (size_t i = 0; i < nsamp; ++i) {
            uint16_t u = (uint16_t)((uint16_t)pcm[i*2] | ((uint16_t)pcm[i*2+1] << 8));
            samples.v[samples.n++] = (int16_t)u;
        }
        free(pcm);

        smp_t data = {0};
        smp_append(&data, carry.v, carry.n);
        smp_append(&data, samples.v, samples.n);
        free(samples.v);
        free(carry.v); carry.v = NULL; carry.n = carry.cap = 0;

        if (src % dst == 0) {
            long long step = src / dst;
            size_t usable = data.n - (data.n % (size_t)step);
            for (size_t i = 0; i < usable; i += (size_t)step) {
                long long sum = 0;
                for (size_t j = i; j < i + (size_t)step; ++j) sum += data.v[j];
                long long v = st_floordiv(sum, step);
                if (v > 32767) v = 32767; if (v < -32768) v = -32768;
                byt_push16(&out, (int16_t)v);
            }
            smp_append(&carry, data.v + usable, data.n - usable);
        } else {
            double ratio = (double)src / (double)dst;
            size_t last = data.n - 1;
            double p = pos;
            while (p < (double)last) {
                long long i = (long long)p;
                double frac = p - (double)i;
                double val = (double)data.v[i] +
                             ((double)data.v[i+1] - (double)data.v[i]) * frac;
                long long iv = (long long)val;
                if (iv > 32767) iv = 32767; if (iv < -32768) iv = -32768;
                byt_push16(&out, (int16_t)iv);
                p += ratio;
            }
            size_t keep = (size_t)(long long)p;
            if (keep <= data.n) smp_append(&carry, data.v + keep, data.n - keep);
            pos = p - (double)keep;
        }
        free(data.v);
    }

    char *result = byt_hex(&out);
    free(out.v);
    free(carry.v);
    return result;
}

json_t *w18_transcripti_9f815d_error(const char *provider, const char *message) {
    json_t *obj = json_object();
    json_set(obj, "success", json_bool(0));
    json_set(obj, "transcript", json_string(""));
    json_set(obj, "provider", json_string(provider ? provider : "unknown"));
    json_set(obj, "error", json_string(message ? message : ""));
    return obj;
}
