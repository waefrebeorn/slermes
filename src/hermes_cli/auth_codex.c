/*
 * auth_codex.c -- faithful C11 port of the pure helpers in
 *                 hermes_cli/auth_codex.py.
 *
 * PoP: w8_authcodex_1aa766_stripped                    @ hermes_cli/auth_codex.py:_stripped
 * PoP: w8_authcodex_1aa766_is_codex_rate_limit_shaped  @ hermes_cli/auth_codex.py:_is_codex_rate_limit_shaped
 * PoP: w8_authcodex_1aa766_entry_is_rate_limit_exhausted @ hermes_cli/auth_codex.py:_entry_is_rate_limit_exhausted
 * PoP: w8_authcodex_1aa766_codex_usage_probe_url       @ hermes_cli/auth_codex.py:_codex_usage_probe_url
 * PoP: w8_authcodex_1aa766_codex_quota_probe_cache_key @ hermes_cli/auth_codex.py:_codex_quota_probe_cache_key
 */
#include "noc_w8_authcodex_1aa766.h"
#include "hermes_json.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W8AC_CANON "https://chatgpt.com/backend-api/codex"

static json_t *w8ac_get(const json_t *o, const char *k) {
    return (o && o->type == JSON_OBJECT) ? json_obj_get(o, k) : NULL;
}

static int w8ac_truthy(const json_t *v) {
    if (!v || v->type == JSON_NULL) return 0;
    switch (v->type) {
        case JSON_BOOL: return v->bool_val ? 1 : 0;
        case JSON_NUMBER: return v->num_val != 0.0;
        case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
        case JSON_ARRAY: case JSON_OBJECT: return v->c.count != 0;
        default: return 0;
    }
}

/* str(value or "") for the JSON shapes used by fixtures. */
static char *w8ac_str(const json_t *v) {
    if (!w8ac_truthy(v)) return (char *)calloc(1, 1);
    if (v->type == JSON_STRING) {
        char *out = (char *)malloc(strlen(v->str_val) + 1);
        strcpy(out, v->str_val);
        return out;
    }
    if (v->type == JSON_BOOL) {
        const char *s = v->bool_val ? "True" : "False";
        char *out = (char *)malloc(strlen(s) + 1);
        strcpy(out, s);
        return out;
    }
    if (v->type == JSON_NUMBER) {
        char buf[64];
        if (v->num_val == (double)(long long)v->num_val)
            snprintf(buf, sizeof(buf), "%.0f", v->num_val);
        else
            snprintf(buf, sizeof(buf), "%g", v->num_val);
        char *out = (char *)malloc(strlen(buf) + 1);
        strcpy(out, buf);
        return out;
    }
    return (char *)calloc(1, 1);
}

static void w8ac_lower(char *s) {
    for (; *s; s++) *s = (char)tolower((unsigned char)*s);
}

static int w8ac_contains(const char *hay, const char *needle) {
    return needle && *needle && strstr(hay, needle) != NULL;
}

static char *w8ac_rstrip_slash(const char *s) {
    size_t n = strlen(s);
    while (n > 0 && s[n - 1] == '/') n--;
    char *out = (char *)malloc(n + 1);
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

static char *w8ac_removesuffix_codex(const char *s) {
    size_t n = strlen(s);
    if (n >= 6 && !strcmp(s + n - 6, "/codex")) {
        char *out = (char *)malloc(n - 6 + 1);
        memcpy(out, s, n - 6);
        out[n - 6] = '\0';
        return out;
    }
    char *out = (char *)malloc(n + 1);
    strcpy(out, s);
    return out;
}

/* ---- self-contained SHA-256 (links with only libjson) ---- */
#define W8AC_XROTR(x,n) (((x)>>(n))|((x)<<(32-(n))))
static const uint32_t W8AC_K[64] = {
 0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
 0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
 0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
 0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
 0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
 0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
 0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
 0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u};
static void w8ac_sha256(const unsigned char *data, size_t len, unsigned char out[32]) {
    size_t total = ((len + 9 + 63) / 64) * 64;
    unsigned char *buf = (unsigned char *)calloc(total ? total : 64, 1);
    if (!buf) { memset(out, 0, 32); return; }
    memcpy(buf, data, len);
    buf[len] = 0x80;
    unsigned long long bits = (unsigned long long)len * 8ull;
    for (int i = 0; i < 8; i++) buf[total - 1 - i] = (unsigned char)(bits >> (8 * i));
    uint32_t h[8] = {0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,
                     0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u};
    for (size_t off = 0; off < total; off += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; i++)
            w[i] = ((uint32_t)buf[off+4*i]<<24)|((uint32_t)buf[off+4*i+1]<<16)|
                   ((uint32_t)buf[off+4*i+2]<<8)|((uint32_t)buf[off+4*i+3]);
        for (int i = 16; i < 64; i++) {
            uint32_t s0 = W8AC_XROTR(w[i-15],7)^W8AC_XROTR(w[i-15],18)^(w[i-15]>>3);
            uint32_t s1 = W8AC_XROTR(w[i-2],17)^W8AC_XROTR(w[i-2],19)^(w[i-2]>>10);
            w[i] = w[i-16]+s0+w[i-7]+s1;
        }
        uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
        for (int i = 0; i < 64; i++) {
            uint32_t S1 = W8AC_XROTR(e,6)^W8AC_XROTR(e,11)^W8AC_XROTR(e,25);
            uint32_t ch = (e&f)^((~e)&g);
            uint32_t t1 = hh+S1+ch+W8AC_K[i]+w[i];
            uint32_t S0 = W8AC_XROTR(a,2)^W8AC_XROTR(a,13)^W8AC_XROTR(a,22);
            uint32_t maj = (a&b)^(a&c)^(b&c);
            uint32_t t2 = S0+maj;
            hh=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
        }
        h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d; h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hh;
    }
    for (int i = 0; i < 8; i++) {
        out[4*i]=(unsigned char)(h[i]>>24); out[4*i+1]=(unsigned char)(h[i]>>16);
        out[4*i+2]=(unsigned char)(h[i]>>8); out[4*i+3]=(unsigned char)h[i];
    }
    free(buf);
}

struct json_t *w8_authcodex_1aa766_stripped(const struct json_t *value) {
    char *s = w8ac_str(value);
    size_t a = 0, b = strlen(s);
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    char *out = (char *)malloc(b - a + 1);
    memcpy(out, s + a, b - a);
    out[b - a] = '\0';
    free(s);
    json_t *res = json_string(out);
    free(out);
    return res;
}

struct json_t *w8_authcodex_1aa766_is_codex_rate_limit_shaped(const struct json_t *code,
        const struct json_t *reason, const struct json_t *message) {
    int shaped = (code && code->type == JSON_NUMBER && code->num_val == 429.0);
    if (!shaped) {
        char *r = w8ac_str(reason); w8ac_lower(r);
        char *m = w8ac_str(message); w8ac_lower(m);
        shaped = w8ac_contains(r, "rate_limit") || w8ac_contains(r, "usage_limit")
                 || w8ac_contains(r, "quota") || w8ac_contains(m, "rate limit")
                 || w8ac_contains(m, "usage limit") || w8ac_contains(m, "quota");
        free(r); free(m);
    }
    return json_bool(shaped);
}

struct json_t *w8_authcodex_1aa766_entry_is_rate_limit_exhausted(const struct json_t *entry) {
    const char *status = json_get_str(entry, "last_status", "");
    if (!status || strcmp(status, "exhausted")) return json_bool(0);
    json_t *res = w8_authcodex_1aa766_is_codex_rate_limit_shaped(
        w8ac_get(entry, "last_error_code"), w8ac_get(entry, "last_error_reason"),
        w8ac_get(entry, "last_error_message"));
    int v = res && res->type == JSON_BOOL && res->bool_val;
    json_free(res);
    return json_bool(v);
}

struct json_t *w8_authcodex_1aa766_codex_usage_probe_url(const struct json_t *base_url) {
    char *raw = w8ac_str(base_url);
    size_t a = 0, b = strlen(raw);
    while (a < b && isspace((unsigned char)raw[a])) a++;
    while (b > a && isspace((unsigned char)raw[b - 1])) b--;
    char *stripped_raw = (char *)malloc(b - a + 1);
    memcpy(stripped_raw, raw + a, b - a);
    stripped_raw[b - a] = '\0';
    free(raw);
    char *stripped = w8ac_rstrip_slash(stripped_raw);
    free(stripped_raw);
    const char *normalized_in = stripped[0] ? stripped : W8AC_CANON;
    char *normalized = w8ac_removesuffix_codex(normalized_in);
    free(stripped);
    int wham = w8ac_contains(normalized, "/backend-api");
    size_t len = strlen(normalized) + (wham ? 5 : 11) + 8;
    char *out = (char *)malloc(len);
    snprintf(out, len, "%s%s/usage", normalized, wham ? "/wham" : "/api/codex");
    free(normalized);
    json_t *res = json_string(out);
    free(out);
    return res;
}

struct json_t *w8_authcodex_1aa766_codex_quota_probe_cache_key(const char *token) {
    if (!token) token = "";
    unsigned char dig[32];
    w8ac_sha256((const unsigned char *)token, strlen(token), dig);
    static const char *HEX = "0123456789abcdef";
    char out[17];
    for (int i = 0; i < 8; i++) { out[2*i] = HEX[dig[i] >> 4]; out[2*i+1] = HEX[dig[i] & 15]; }
    out[16] = '\0';
    return json_string(out);
}
