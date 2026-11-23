/*
 * image_eviction_policy.c — faithful C11 port of agent/image_eviction_policy.py
 * (gate w14_imageevict_e64946).
 *
 * PoP: w14_imageevict_e64946_image_dimensions_from_bytes @ agent/image_eviction_policy.py:image_dimensions_from_bytes
 * PoP: w14_imageevict_e64946_image_max_side_from_data @ agent/image_eviction_policy.py:image_max_side_from_data
 * PoP: w14_imageevict_e64946_image_within_many_image_limit @ agent/image_eviction_policy.py:image_within_many_image_limit
 * PoP: w14_imageevict_e64946_images_all_within_many_image_limit @ agent/image_eviction_policy.py:images_all_within_many_image_limit
 * PoP: w14_imageevict_e64946_outbound_image_block_limit @ agent/image_eviction_policy.py:outbound_image_block_limit
 * PoP: w14_imageevict_e64946_outbound_image_retire_count @ agent/image_eviction_policy.py:outbound_image_retire_count
 * PoP: w14_imageevict_e64946_latched_retire_count @ agent/image_eviction_policy.py:latched_retire_count
 */
#include "noc_w14_imageevict_e64946.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OUTBOUND_IMAGE_LIMIT 20
#define OUTBOUND_IMAGE_HARD_LIMIT 100
#define MANY_IMAGE_DIMENSION_LIMIT 2000
#define DIMENSION_PROBE_CHARS (32 * 1024)
#define DEFAULT_BUDGET 24000000LL
#define DEFAULT_BATCH 8
#define DEFAULT_FLOOR 3

static char *dupstr(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *p = (char *)malloc(n + 1);
    if (p) { if (n) memcpy(p, s, n); p[n] = '\0'; }
    return p;
}

static char *strip_dup(const char *s) {
    char *t = dupstr(s ? s : "");
    size_t a = 0, b;
    if (!t) return NULL;
    b = strlen(t);
    while (a < b && isspace((unsigned char)t[a])) a++;
    while (b > a && isspace((unsigned char)t[b - 1])) b--;
    if (a && b > a) memmove(t, t + a, b - a);
    t[b - a] = '\0';
    return t;
}

/* ---- base64 decode (validate=False: discard non-alphabet chars incl. '=') ---- */
static int b64val(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}
static unsigned char *b64decode(const char *s, size_t *out_len) {
    size_t n = s ? strlen(s) : 0;
    unsigned char *buf = (unsigned char *)malloc(n + 1);
    unsigned acc = 0;
    int bits = 0;
    size_t k = 0;
    if (!buf) { *out_len = 0; return NULL; }
    for (size_t i = 0; i < n; ++i) {
        int v = b64val((unsigned char)s[i]);
        if (v < 0) continue;
        acc = (acc << 6) | (unsigned)v;
        bits += 6;
        if (bits >= 8) { bits -= 8; buf[k++] = (unsigned char)((acc >> bits) & 0xFF); }
    }
    *out_len = k;
    return buf;
}

static bool is_sof(int m) {
    return m == 0xC0 || m == 0xC1 || m == 0xC2 || m == 0xC3 || m == 0xC5 ||
           m == 0xC6 || m == 0xC7 || m == 0xC9 || m == 0xCA || m == 0xCB ||
           m == 0xCD || m == 0xCE || m == 0xCF;
}

/* image_dimensions_from_bytes: PNG IHDR / JPEG SOF header walk. */
static bool dims_from_raw(const unsigned char *raw, size_t n, long long *w, long long *h) {
    static const unsigned char PNG_SIG[8] = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    if (n >= 8 && !memcmp(raw, PNG_SIG, 8) && n >= 24) {
        *w = ((long long)raw[16] << 24) | ((long long)raw[17] << 16) | ((long long)raw[18] << 8) | raw[19];
        *h = ((long long)raw[20] << 24) | ((long long)raw[21] << 16) | ((long long)raw[22] << 8) | raw[23];
        return true;
    }
    if (n > 4 && raw[0] == 0xFF && raw[1] == 0xD8) {
        size_t i = 2;
        while (i + 9 < n) {
            int marker;
            size_t seglen;
            if (raw[i] != 0xFF) { i++; continue; }
            marker = raw[i + 1];
            i += 2;
            while (marker == 0xFF && i < n) { marker = raw[i]; i++; }
            if (marker == 0xD8 || marker == 0xD9) continue;
            if (marker == 0xDA || i + 2 > n) break;
            seglen = ((size_t)raw[i] << 8) | raw[i + 1];
            if (seglen < 2 || i + seglen > n) break;
            if (is_sof(marker) && seglen >= 7) {
                *w = ((long long)raw[i + 5] << 8) | raw[i + 6];
                *h = ((long long)raw[i + 3] << 8) | raw[i + 4];
                return true;
            }
            i += seglen;
        }
    }
    return false;
}

json_t *w14_imageevict_e64946_image_dimensions_from_bytes(const char *raw_b64) {
    size_t n = 0;
    unsigned char *raw = b64decode(raw_b64 ? raw_b64 : "", &n);
    long long w = 0, h = 0;
    json_t *out;
    bool ok = dims_from_raw(raw, n, &w, &h);
    free(raw);
    if (!ok) return json_null();
    out = json_array();
    json_append(out, json_number((double)w));
    json_append(out, json_number((double)h));
    return out;
}

bool w14_imageevict_e64946_image_max_side_from_data(const char *data, long long *out_max) {
    const char *payload = data;
    char *pl, *pref;
    size_t len;
    size_t rn;
    unsigned char *raw;
    long long w, h;
    bool ok;

    if (!data || !data[0]) return false;
    if (!strncmp(payload, "data:", 5)) {
        const char *comma = strchr(payload, ',');
        char *hdr;
        size_t hlen;
        if (!comma) return false;
        hlen = (size_t)(comma - payload);
        hdr = (char *)malloc(hlen + 1);
        if (!hdr) return false;
        memcpy(hdr, payload, hlen);
        hdr[hlen] = '\0';
        if (!strstr(hdr, ";base64")) { free(hdr); return false; }
        free(hdr);
        payload = comma + 1;
    }
    pl = strip_dup(payload);
    len = strlen(pl);
    if (len > DIMENSION_PROBE_CHARS) len = DIMENSION_PROBE_CHARS;
    len -= len % 4;
    if (!len) { free(pl); return false; }
    pref = (char *)malloc(len + 1);
    if (!pref) { free(pl); return false; }
    memcpy(pref, pl, len);
    pref[len] = '\0';
    free(pl);
    raw = b64decode(pref, &rn);
    free(pref);
    ok = dims_from_raw(raw, rn, &w, &h);
    free(raw);
    if (!ok) return false;
    *out_max = (w > h) ? w : h;
    return true;
}

bool w14_imageevict_e64946_image_within_many_image_limit(const char *data) {
    long long longest = 0;
    return w14_imageevict_e64946_image_max_side_from_data(data, &longest) &&
           longest <= MANY_IMAGE_DIMENSION_LIMIT;
}

bool w14_imageevict_e64946_images_all_within_many_image_limit(const json_t *datas) {
    if (!datas || datas->type != JSON_ARRAY) return true;
    for (size_t i = 0; i < datas->c.count; ++i) {
        const json_t *d = json_get(datas, i);
        const char *s = (d && d->type == JSON_STRING) ? d->str_val : NULL;
        if (!w14_imageevict_e64946_image_within_many_image_limit(s)) return false;
    }
    return true;
}

int w14_imageevict_e64946_outbound_image_block_limit(bool many_image_safe) {
    return many_image_safe ? OUTBOUND_IMAGE_HARD_LIMIT : OUTBOUND_IMAGE_LIMIT;
}

/* One request in isolation; `start` is the oldest-carrier offset in blocks/sizes. */
static long long retire_for_request(const json_t *blocks, size_t start, size_t len, long long reserved,
        const json_t *sizes, long long reserved_bytes, long long limit, long long budget,
        long long batch, long long floor) {
    long long *bk = (long long *)calloc(len + 1, sizeof(long long));
    long long *by = (long long *)calloc(len + 1, sizeof(long long));
    bool hs = (sizes && sizes->type == JSON_ARRAY);
    long long fl, result = 0;

    if (!bk || !by) { free(bk); free(by); return 0; }
    for (size_t i = 0; i < len; ++i) {
        const json_t *b = json_get(blocks, start + i);
        long long bv = (b && b->type == JSON_NUMBER) ? (long long)b->num_val : 0;
        bk[i + 1] = bk[i] + bv;
        if (hs) {
            const json_t *s = json_get(sizes, start + i);
            long long sv = (s && s->type == JSON_NUMBER) ? (long long)s->num_val : 0;
            by[i + 1] = by[i] + sv;
        }
    }
#define BYFIT(k) (!hs || reserved_bytes + by[(k)] <= budget)
#define FITS(k)  (reserved + bk[(k)] <= limit && BYFIT(k))
    if (FITS(len)) { free(bk); free(by); return 0; }
    fl = floor;
    if (fl < 0) fl = 0;
    if (fl > (long long)len) fl = (long long)len;
    if (!FITS(0) && BYFIT(fl)) {
        long long bw = fl;
        long long q, excess;
        for (long long k = fl; k <= (long long)len; ++k) if (BYFIT(k) && k > bw) bw = k;
        q = batch < (bw - fl + 1) ? batch : (bw - fl + 1);
        if (q < 1) q = 1;
        excess = (long long)len - fl;
        result = excess - excess % q;
    } else {
        long long window = 0, q, retire = 0;
        if (FITS(0)) {
            for (long long k = 0; k <= (long long)len; ++k) if (FITS(k) && k > window) window = k;
        }
        q = batch < (window - fl) ? batch : (window - fl);
        if (q < 1) q = 1;
        while (retire < (long long)len) {
            retire += q;
            if (retire > (long long)len) retire = (long long)len;
            if (FITS((long long)len - retire)) break;
        }
        result = retire;
    }
#undef FITS
#undef BYFIT
    free(bk); free(by);
    return result;
}

int w14_imageevict_e64946_outbound_image_retire_count(const json_t *blocks, long long reserved,
        const json_t *sizes, long long reserved_bytes, bool many_image_safe, long long limit_or_neg1,
        long long budget, long long batch, long long floor) {
    long long limit = limit_or_neg1;
    size_t total = (blocks && blocks->type == JSON_ARRAY) ? blocks->c.count : 0;
    long long best = 0;
    if (limit < 0) limit = many_image_safe ? OUTBOUND_IMAGE_HARD_LIMIT : OUTBOUND_IMAGE_LIMIT;
    for (long long m = 1; m <= (long long)total; ++m) {
        size_t start = total - (size_t)m;
        long long r = retire_for_request(blocks, start, (size_t)m, reserved, sizes, reserved_bytes,
                                         limit, budget, batch, floor);
        if (r > best) best = r;
    }
    return (int)best;
}

static bool set_contains(const json_t *arr, const char *s) {
    if (!arr || arr->type != JSON_ARRAY) return false;
    for (size_t i = 0; i < arr->c.count; ++i) {
        const json_t *v = json_get(arr, i);
        if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, s)) return true;
    }
    return false;
}
static int cmp_str_ptr(const void *a, const void *b) {
    const char *sa = *(const char *const *)a;
    const char *sb = *(const char *const *)b;
    return strcmp(sa, sb);
}

json_t *w14_imageevict_e64946_latched_retire_count(long long retire, const json_t *ids,
                                                  const json_t *retired_ids) {
    json_t *o = json_object();
    bool has_set = (retired_ids && retired_ids->type == JSON_ARRAY);
    size_t n = (ids && ids->type == JSON_ARRAY) ? ids->c.count : 0;
    long long first_latched = -1;
    long long r = retire;
    char **vals = NULL;
    size_t nv = 0, cap = 0;
    long long start;
    json_t *out_set = json_array();

    if (!has_set) {
        json_set(o, "retire", json_number((double)retire));
        json_set(o, "retired", json_null());
        return o;
    }
    for (size_t i = 0; i < n; ++i) {
        const json_t *cid = json_get(ids, i);
        if (!cid || cid->type != JSON_STRING || !cid->str_val || !cid->str_val[0]) continue;
        if (set_contains(retired_ids, cid->str_val)) { first_latched = (long long)i; break; }
    }
    if (first_latched >= 0 && (long long)n - first_latched > r) r = (long long)n - first_latched;
    start = (long long)n - r;
    if (start < 0) start += (long long)n; /* Python negative slice */
    if (start < 0) start = 0;
    if (start > (long long)n) start = (long long)n;

    for (size_t i = 0; i < retired_ids->c.count; ++i) {
        const json_t *v = json_get(retired_ids, i);
        if (v && v->type == JSON_STRING && v->str_val) {
            bool seen = false;
            for (size_t j = 0; j < nv; ++j) if (!strcmp(vals[j], v->str_val)) { seen = true; break; }
            if (!seen) {
                if (nv == cap) { cap = cap ? cap * 2 : 8; vals = (char **)realloc(vals, cap * sizeof(char *)); }
                vals[nv++] = dupstr(v->str_val);
            }
        }
    }
    for (long long i = start; i < (long long)n; ++i) {
        const json_t *cid = json_get(ids, (size_t)i);
        const char *s = (cid && cid->type == JSON_STRING && cid->str_val && cid->str_val[0]) ? cid->str_val : NULL;
        if (!s) continue;
        bool seen = false;
        for (size_t j = 0; j < nv; ++j) if (!strcmp(vals[j], s)) { seen = true; break; }
        if (!seen) {
            if (nv == cap) { cap = cap ? cap * 2 : 8; vals = (char **)realloc(vals, cap * sizeof(char *)); }
            vals[nv++] = dupstr(s);
        }
    }
    if (nv > 1) qsort(vals, nv, sizeof(char *), cmp_str_ptr);
    for (size_t i = 0; i < nv; ++i) { json_append(out_set, json_string(vals[i])); free(vals[i]); }
    free(vals);
    json_set(o, "retire", json_number((double)r));
    json_set(o, "retired", out_set);
    return o;
}
