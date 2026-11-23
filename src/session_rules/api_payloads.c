/*
 * api_payloads.c — C port of upstream Hermes session_rules/api_payloads.py.
 *
 * PoP: w14_apipayload_e70b8f_data_url_bytes @ session_rules/api_payloads.py:data_url_bytes
 * PoP: w14_apipayload_e70b8f_compacted_api_payload @ session_rules/api_payloads.py:compacted_api_payload
 *
 * Inline data:image compaction for a settled API admission. base64 decode and
 * SHA-256 are implemented here so the gate is self-contained (no libcrypto link).
 */
#include "noc_w14_apipayload_e70b8f.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RETAINED "hermes-retained:"

static char *ap_dupstr(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (p) { memcpy(p, s, n + 1); }
    return p;
}
static const char *ap_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}
static void ap_lower(char *s) { for (; *s; s++) *s = (char)tolower((unsigned char)*s); }
static char *ap_strip(const char *s) {
    size_t n = strlen(s), a = 0, b = n;
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    char *p = (char *)malloc(b - a + 1);
    if (!p) return NULL;
    memcpy(p, s + a, b - a);
    p[b - a] = '\0';
    return p;
}

/* strict base64 (Python base64.b64decode(..., validate=True)) */
static int ap_b64val(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}
static int ap_b64_decode(const char *s, unsigned char *out, size_t *outlen) {
    size_t n = strlen(s);
    if (n % 4) return 0;
    *outlen = 0;
    if (n == 0) return 1;
    size_t pad = 0;
    if (s[n - 1] == '=') pad = 1;
    if (n >= 2 && s[n - 2] == '=') pad = 2;
    size_t data = n - pad;
    for (size_t i = 0; i < n; i++) {
        if (i >= data) { if (s[i] != '=') return 0; }
        else if (ap_b64val((unsigned char)s[i]) < 0) return 0;
    }
    size_t o = 0;
    for (size_t i = 0; i < data; i += 4) {
        int v0 = ap_b64val((unsigned char)s[i]), v1 = ap_b64val((unsigned char)s[i + 1]);
        out[o++] = (unsigned char)((v0 << 2) | (v1 >> 4));
        if (i + 2 < data) {
            int v2 = ap_b64val((unsigned char)s[i + 2]);
            out[o++] = (unsigned char)((v1 << 4) | (v2 >> 2));
            if (i + 3 < data) {
                int v3 = ap_b64val((unsigned char)s[i + 3]);
                out[o++] = (unsigned char)((v2 << 6) | v3);
            }
        }
    }
    *outlen = o;
    return 1;
}

/* self-contained SHA-256 */
static uint32_t ap_rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
static void ap_sha_block(uint32_t h[8], const unsigned char p[64]) {
    static const uint32_t K[64] = {
        0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
        0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
        0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
        0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
        0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
        0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
        0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
        0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u};
    uint32_t w[64];
    for (int t = 0; t < 16; t++)
        w[t] = ((uint32_t)p[t*4]<<24)|((uint32_t)p[t*4+1]<<16)|((uint32_t)p[t*4+2]<<8)|(uint32_t)p[t*4+3];
    for (int t = 16; t < 64; t++) {
        uint32_t s0 = ap_rotr(w[t-15],7)^ap_rotr(w[t-15],18)^(w[t-15]>>3);
        uint32_t s1 = ap_rotr(w[t-2],17)^ap_rotr(w[t-2],19)^(w[t-2]>>10);
        w[t] = w[t-16] + s0 + w[t-7] + s1;
    }
    uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
    for (int t = 0; t < 64; t++) {
        uint32_t S1 = ap_rotr(e,6)^ap_rotr(e,11)^ap_rotr(e,25);
        uint32_t ch = (e&f)^((~e)&g);
        uint32_t t1 = hh + S1 + ch + K[t] + w[t];
        uint32_t S0 = ap_rotr(a,2)^ap_rotr(a,13)^ap_rotr(a,22);
        uint32_t maj = (a&b)^(a&c)^(b&c);
        uint32_t t2 = S0 + maj;
        hh=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
    }
    h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d; h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hh;
}
static void ap_sha256(const unsigned char *data, size_t len, unsigned char out[32]) {
    uint32_t h[8] = {0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,
                     0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u};
    size_t i = 0;
    while (len - i >= 64) { ap_sha_block(h, data + i); i += 64; }
    unsigned char block[128];
    size_t rem = len - i;
    memset(block, 0, sizeof(block));
    if (rem) memcpy(block, data + i, rem);
    block[rem] = 0x80;
    size_t z = rem + 1;
    if (z > 56) {
        ap_sha_block(h, block);
        memset(block, 0, sizeof(block));
        z = 0;
    }
    uint64_t bits = (uint64_t)len * 8u;
    for (int j = 0; j < 8; j++) block[56 + j] = (unsigned char)(bits >> (56 - 8*j));
    ap_sha_block(h, block);
    for (int j = 0; j < 8; j++) {
        out[4*j]   = (unsigned char)(h[j] >> 24);
        out[4*j+1] = (unsigned char)(h[j] >> 16);
        out[4*j+2] = (unsigned char)(h[j] >> 8);
        out[4*j+3] = (unsigned char)(h[j]);
    }
    (void)z;
}
static void ap_hex(const unsigned char h[32], char out[65]) {
    static const char *d = "0123456789abcdef";
    for (int i = 0; i < 32; i++) { out[2*i] = d[h[i]>>4]; out[2*i+1] = d[h[i]&15]; }
    out[64] = '\0';
}

json_t *w14_apipayload_e70b8f_data_url_bytes(const json_t *url) {
    const char *u = ap_jstr(url);
    if (!u) return NULL;
    const char *comma = strchr(u, ',');
    size_t hlen = comma ? (size_t)(comma - u) : strlen(u);
    char *header = (char *)malloc(hlen + 1);
    if (!header) return NULL;
    memcpy(header, u, hlen); header[hlen] = '\0';
    const char *encoded = comma ? comma + 1 : "";

    const char *base = header + (hlen >= 5 ? 5 : hlen);
    const char *semi = strchr(base, ';');
    size_t mlen = semi ? (size_t)(semi - base) : strlen(base);
    char mime[512];
    if (mlen >= sizeof(mime)) { free(header); return NULL; }
    memcpy(mime, base, mlen); mime[mlen] = '\0';
    char *ms = ap_strip(mime);
    if (!ms) { free(header); return NULL; }
    ap_lower(ms);

    int ok = strlen(u) >= 5;
    for (int i = 0; i < 5 && ok; i++) {
        if (tolower((unsigned char)u[i]) != "data:"[i]) ok = 0;
    }
    int mok = !strcmp(ms, "image/png") || !strcmp(ms, "image/jpeg") ||
              !strcmp(ms, "image/gif") || !strcmp(ms, "image/webp");
    char *hl = ap_dupstr(header);
    if (!hl) { free(ms); free(header); return NULL; }
    ap_lower(hl);
    int hasb64 = strstr(hl, ";base64") != NULL;
    free(hl);
    if (!(ok && mok && hasb64)) { free(ms); free(header); return NULL; }

    size_t cap = strlen(encoded) / 4 * 3 + 1;
    unsigned char *bytes = (unsigned char *)malloc(cap ? cap : 1);
    size_t blen = 0;
    int dec = bytes ? ap_b64_decode(encoded, bytes, &blen) : 0;
    json_t *out = NULL;
    if (dec) {
        out = json_object();
        json_set(out, "mime", json_string(ms));
        json_t *arr = json_array();
        for (size_t i = 0; i < blen; i++) json_append(arr, json_number((double)bytes[i]));
        json_set(out, "bytes", arr);
    }
    free(bytes);
    free(ms);
    free(header);
    return out;
}

static json_t *ap_compacted(const json_t *content, const json_t *media) {
    if (!json_is_array(content) || !json_is_array(media) || json_len(media) == 0) return NULL;
    json_t *parts = json_array();
    size_t n = json_len(content), dcount = 0;
    int changed = 0;
    for (size_t i = 0; i < n; i++) {
        const json_t *part = json_get(content, i);
        const json_t *img = NULL;
        if (json_is_object(part)) {
            const json_t *t = json_obj_get(part, "type");
            if (t && t->type == JSON_STRING && t->str_val && !strcmp(t->str_val, "image_url"))
                img = json_obj_get(part, "image_url");
        }
        const char *url = "";
        if (json_is_object(img)) {
            const char *s = ap_jstr(json_obj_get(img, "url"));
            if (s) url = s;
        }
        json_t *dec = (*url) ? w14_apipayload_e70b8f_data_url_bytes(json_string(url)) : NULL;
        if (dec) {
            const char *digest = NULL;
            if (dcount < json_len(media)) {
                const json_t *ref = json_get(media, dcount++);
                digest = json_get_str(ref, "sha256", NULL);
            }
            if (digest) {
                const json_t *barr = json_obj_get(dec, "bytes");
                if (json_is_array(barr)) {
                    size_t bn = json_len(barr);
                    unsigned char *buf = (unsigned char *)malloc(bn ? bn : 1);
                    if (buf) {
                        for (size_t k = 0; k < bn; k++) {
                            const json_t *bv = json_get(barr, k);
                            buf[k] = (unsigned char)(bv ? bv->num_val : 0);
                        }
                        unsigned char h[32];
                        char hex[65];
                        ap_sha256(buf, bn, h);
                        ap_hex(h, hex);
                        free(buf);
                        if (!strcmp(hex, digest)) {
                            json_t *newimg = json_copy(img);
                            char refurl[128];
                            snprintf(refurl, sizeof(refurl), "%s%s", RETAINED, digest);
                            json_set(newimg, "url", json_string(refurl));
                            json_t *newpart = json_copy(part);
                            json_set(newpart, "image_url", newimg);
                            json_append(parts, newpart);
                            changed = 1;
                            json_free(dec);
                            continue;
                        }
                    }
                }
            }
            json_free(dec);
        }
        json_append(parts, json_copy(part));
    }
    if (!changed) { json_free(parts); return NULL; }
    return parts;
}

json_t *w14_apipayload_e70b8f_compacted_api_payload(const json_t *payload, const json_t *request_id) {
    if (!json_is_object(payload)) return NULL;
    const json_t *data = json_obj_get(payload, "api_turn_v1");
    if (!json_is_object(data)) return NULL;
    json_t *text = ap_compacted(json_obj_get(payload, "text"), json_obj_get(data, "media"));
    const json_t *hist = json_obj_get(data, "history");
    const char *rid = ap_jstr(request_id);
    if (!rid) rid = "";
    int drop = json_is_array(hist) && strncmp(rid, "responses:", 10) != 0;
    if (!text && !drop) return NULL;
    json_t *out = json_copy(payload);
    if (text) json_set(out, "text", text);
    else {
        const json_t *orig = json_obj_get(payload, "text");
        json_set(out, "text", orig ? json_copy(orig) : json_null());
    }
    if (drop) {
        json_t *nd = json_copy(data);
        json_set(nd, "history", json_null());
        json_set(out, "api_turn_v1", nd);
    }
    return out;
}
