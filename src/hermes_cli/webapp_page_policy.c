/*
 * webapp_page_policy.c — C port of upstream Hermes hermes_cli/webapp_page_policy.py.
 *
 * PoP: w17_webapppage_7856b6_script_hashes @ hermes_cli/webapp_page_policy.py:_ScriptHashes
 * PoP: w17_webapppage_7856b6_page_headers_ex @ hermes_cli/webapp_page_policy.py:page_headers
 * PoP: w17_webapppage_7856b6_constants @ hermes_cli/webapp_page_policy.py:page_headers
 */
#include "noc_w17_webapppage_7856b6.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupstr(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *p = (char *)malloc(n + 1);
    if (p) memcpy(p, s, n + 1);
    return p;
}

/* ---- SHA-256 (FIPS 180-4), reused verbatim from the repo's proven helper. ---- */
typedef struct { uint32_t h[8]; uint64_t bitlen; unsigned char buf[64]; size_t buflen; } sha256_ctx;
static const uint32_t K256[64] = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u,
};
static uint32_t rotr32(uint32_t x, unsigned n) { return (x >> n) | (x << (32 - n)); }
static void sha_block(sha256_ctx *c, const unsigned char *p) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++) w[i] = ((uint32_t)p[i*4]<<24)|((uint32_t)p[i*4+1]<<16)|((uint32_t)p[i*4+2]<<8)|p[i*4+3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0=rotr32(w[i-15],7)^rotr32(w[i-15],18)^(w[i-15]>>3);
        uint32_t s1=rotr32(w[i-2],17)^rotr32(w[i-2],19)^(w[i-2]>>10);
        w[i]=w[i-16]+s0+w[i-7]+s1;
    }
    uint32_t a=c->h[0],b=c->h[1],cc=c->h[2],d=c->h[3],e=c->h[4],f=c->h[5],g=c->h[6],hh=c->h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1=rotr32(e,6)^rotr32(e,11)^rotr32(e,25), ch=(e&f)^((~e)&g);
        uint32_t t1=hh+S1+ch+K256[i]+w[i];
        uint32_t S0=rotr32(a,2)^rotr32(a,13)^rotr32(a,22), maj=(a&b)^(a&cc)^(b&cc);
        uint32_t t2=S0+maj; hh=g; g=f; f=e; e=d+t1; d=cc; cc=b; b=a; a=t1+t2;
    }
    c->h[0]+=a;c->h[1]+=b;c->h[2]+=cc;c->h[3]+=d;c->h[4]+=e;c->h[5]+=f;c->h[6]+=g;c->h[7]+=hh;
}
static void sha_init(sha256_ctx *c) {
    c->h[0]=0x6a09e667u;c->h[1]=0xbb67ae85u;c->h[2]=0x3c6ef372u;c->h[3]=0xa54ff53au;
    c->h[4]=0x510e527fu;c->h[5]=0x9b05688cu;c->h[6]=0x1f83d9abu;c->h[7]=0x5be0cd19u;
    c->bitlen=0;c->buflen=0;
}
static void sha_update(sha256_ctx *c, const unsigned char *data, size_t len) {
    c->bitlen += (uint64_t)len * 8;
    while (len) {
        size_t n = 64 - c->buflen; if (n > len) n = len;
        memcpy(c->buf + c->buflen, data, n); c->buflen += n; data += n; len -= n;
        if (c->buflen == 64) { sha_block(c, c->buf); c->buflen = 0; }
    }
}
static void sha_final(sha256_ctx *c, unsigned char out[32]) {
    uint64_t bits=c->bitlen; unsigned char pad=0x80; sha_update(c,&pad,1);
    unsigned char zero=0; while (c->buflen!=56) sha_update(c,&zero,1);
    unsigned char tail[8]; for (int i=0;i<8;i++) tail[i]=(unsigned char)(bits>>(56-8*i));
    sha_update(c,tail,8);
    for (int i=0;i<8;i++){out[i*4]=(unsigned char)(c->h[i]>>24);out[i*4+1]=(unsigned char)(c->h[i]>>16);
        out[i*4+2]=(unsigned char)(c->h[i]>>8);out[i*4+3]=(unsigned char)(c->h[i]);}
}
static char *b64encode(const unsigned char *data, size_t len) {
    static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t cap = ((len + 2) / 3) * 4 + 1;
    char *out = (char *)malloc(cap);
    if (!out) return dupstr("");
    size_t j = 0;
    for (size_t i = 0; i < len; i += 3) {
        unsigned a = data[i], b = i + 1 < len ? data[i+1] : 0, c = i + 2 < len ? data[i+2] : 0;
        out[j++] = B64[a >> 2];
        out[j++] = B64[((a & 3) << 4) | (b >> 4)];
        out[j++] = i + 1 < len ? B64[((b & 15) << 2) | (c >> 6)] : '=';
        out[j++] = i + 2 < len ? B64[c & 63] : '=';
    }
    out[j] = '\0';
    return out;
}
static char *script_hash_token(const char *data, size_t len) {
    sha256_ctx ctx; sha_init(&ctx);
    sha_update(&ctx, (const unsigned char *)data, len);
    unsigned char d[32]; sha_final(&ctx, d);
    char *b64 = b64encode(d, 32);
    size_t n = strlen("'sha256-'") + strlen(b64) + 2;
    char *out = (char *)malloc(n);
    if (out) snprintf(out, n, "'sha256-%s'", b64);
    free(b64);
    return out;
}

/* ---- Minimal HTML tag scanner matching html.parser's script handling. ---- */
static int ci_n(const char *s, const char *lit, size_t n) {
    for (size_t i = 0; i < n; i++)
        if (tolower((unsigned char)s[i]) != tolower((unsigned char)lit[i])) return 0;
    return 1;
}
/* p points at '<'. Return 1 when this is a <script ...> start tag. */
static int tag_is_script(const char *p, const char *end, const char **after) {
    if (end - p < 8) return 0;
    const char *q = p + 1;
    if (!ci_n(q, "script", 6)) return 0;
    q += 6;
    if (q >= end) return 0;
    char c = *q;
    if (!(isspace((unsigned char)c) || c == '>' || c == '/')) return 0;
    *after = q;
    return 1;
}
static const char *find_tag_end(const char *p, const char *end) {
    char quote = 0;
    for (const char *x = p; x < end; x++) {
        char c = *x;
        if (quote) { if (c == quote) quote = 0; }
        else if (c == '"' || c == '\'') quote = c;
        else if (c == '>') return x;
    }
    return NULL;
}
static int attr_has_src(const char *p, const char *gt) {
    const char *x = p;
    while (x < gt) {
        while (x < gt && isspace((unsigned char)*x)) x++;
        if (x >= gt) break;
        if (*x == '/') { x++; continue; }
        const char *n0 = x;
        while (x < gt && !isspace((unsigned char)*x) && *x != '=' && *x != '/' && *x != '>') x++;
        size_t nlen = (size_t)(x - n0);
        if (nlen == 3 && ci_n(n0, "src", 3)) return 1;
        while (x < gt && isspace((unsigned char)*x)) x++;
        if (x < gt && *x == '=') {
            x++;
            while (x < gt && isspace((unsigned char)*x)) x++;
            if (x < gt && (*x == '"' || *x == '\'')) {
                char q = *x++;
                while (x < gt && *x != q) x++;
                if (x < gt) x++;
            } else {
                while (x < gt && !isspace((unsigned char)*x)) x++;
            }
        }
    }
    return 0;
}
static const char *find_close_script(const char *p, const char *end) {
    if (end - p < 9) return NULL;
    for (const char *x = p; x + 9 <= end; x++) {
        if (*x == '<' && x[1] == '/' && ci_n(x + 2, "script", 6)) {
            char c = x[8];
            if (isspace((unsigned char)c) || c == '>') return x;
        }
    }
    return NULL;
}

json_t *w17_webapppage_7856b6_script_hashes(const char *html) {
    json_t *out = json_array();
    if (!html) return out;
    const char *p = html, *end = html + strlen(html);
    while (p < end) {
        if (*p != '<') { p++; continue; }
        const char *after = NULL;
        if (!tag_is_script(p, end, &after)) { p++; continue; }
        const char *gt = find_tag_end(after, end);
        if (!gt) break;
        int has_src = attr_has_src(after, gt);
        const char *content = gt + 1;
        const char *close = find_close_script(content, end);
        if (!close) break;                      /* unclosed: parser emits no end tag */
        if (!has_src) {
            char *tok = script_hash_token(content, (size_t)(close - content));
            json_append(out, json_string(tok ? tok : ""));
            free(tok);
        }
        const char *cgt = find_tag_end(close + 2, end);
        if (!cgt) break;
        p = cgt + 1;
    }
    return out;
}

json_t *w17_webapppage_7856b6_page_headers_ex(const char *ui_surface,
                                              const char *html,
                                              const json_t *headers) {
    json_t *out = (headers && headers->type == JSON_OBJECT) ? json_copy(headers) : json_object();
    if (!ui_surface || strcmp(ui_surface, "webapp") != 0) return out;
    json_t *hashes = w17_webapppage_7856b6_script_hashes(html ? html : "");
    size_t cap = 512;
    for (size_t i = 0; i < json_len(hashes); i++) {
        json_t *h = json_get(hashes, i);
        cap += (h && h->str_val) ? strlen(h->str_val) + 2 : 4;
    }
    char *joined = (char *)calloc(1, cap);
    if (joined) {
        size_t used = 0;
        for (size_t i = 0; i < json_len(hashes); i++) {
            json_t *h = json_get(hashes, i);
            const char *s = (h && h->str_val) ? h->str_val : "";
            if (i) { joined[used++] = ' '; }
            size_t n = strlen(s);
            memcpy(joined + used, s, n); used += n;
        }
        joined[used] = '\0';
    }
    const char *fmt =
        "default-src 'none'; script-src 'self' blob: 'wasm-unsafe-eval' %s; "
        "script-src-attr 'none'; style-src 'self' 'unsafe-inline' https://fonts.googleapis.com; "
        "font-src 'self' data: https://fonts.gstatic.com; connect-src * ws: wss: data: blob:; "
        "img-src 'self' data: blob: https: http:; media-src * data: blob:; worker-src 'self' blob:; "
        "frame-src 'self' blob: data: https: http:; manifest-src 'self'; object-src 'none'; "
        "base-uri 'none'; form-action 'none'; frame-ancestors 'none'";
    size_t pc = strlen(fmt) + (joined ? strlen(joined) : 0) + 8;
    char *policy = (char *)malloc(pc);
    if (policy) {
        snprintf(policy, pc, fmt, joined ? joined : "");
        json_set(out, "Content-Security-Policy", json_string(policy));
        json_set(out, "X-Content-Type-Options", json_string("nosniff"));
        free(policy);
    }
    free(joined);
    json_free(hashes);
    return out;
}

json_t *w17_webapppage_7856b6_constants(void) {
    json_t *out = json_object();
    json_set(out, "GONE_SURFACE", json_string("webapp"));
    json_set(out, "NOSNIFF", json_string("nosniff"));
    json_set(out, "CSP_HEADER", json_string("Content-Security-Policy"));
    return out;
}
