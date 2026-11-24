/*
 * delegate_tool_resumable.c — faithful C11 port of the pure helpers in
 * tools/delegate_tool_resumable.py (upstream Hermes quarry).
 *
 * PoP: w18_delegateto_d76c3e_classify @ tools/delegate_tool_resumable.py:classify
 * PoP: w18_delegateto_d76c3e_endpoint_identity @ tools/delegate_tool_resumable.py:endpoint_identity
 *
 * Self-contained: a minimal SHA-256 (for endpoint_identity) and no external
 * dependencies. `time.time()` is injected as `now` so the port is deterministic.
 */
#include "noc_w18_delegateto_d76c3e.h"
#include "hermes_json.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ---------- minimal SHA-256 ---------- */
typedef struct {
    uint32_t h[8];
    uint64_t bitlen;
    unsigned char buf[64];
    size_t buflen;
} dtr_sha256_ctx;

static const uint32_t DTR_K256[64] = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
};
#define DTR_ROR(x,n) (((x) >> (n)) | ((x) << (32 - (n))))
static void dtr_sha256_block(dtr_sha256_ctx *c, const unsigned char *p) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i)
        w[i] = ((uint32_t)p[i*4] << 24) | ((uint32_t)p[i*4+1] << 16) |
               ((uint32_t)p[i*4+2] << 8) | (uint32_t)p[i*4+3];
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = DTR_ROR(w[i-15],7) ^ DTR_ROR(w[i-15],18) ^ (w[i-15] >> 3);
        uint32_t s1 = DTR_ROR(w[i-2],17) ^ DTR_ROR(w[i-2],19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a=c->h[0],b=c->h[1],cc=c->h[2],d=c->h[3],e=c->h[4],f=c->h[5],g=c->h[6],hh=c->h[7];
    for (int i = 0; i < 64; ++i) {
        uint32_t S1 = DTR_ROR(e,6) ^ DTR_ROR(e,11) ^ DTR_ROR(e,25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = hh + S1 + ch + DTR_K256[i] + w[i];
        uint32_t S0 = DTR_ROR(a,2) ^ DTR_ROR(a,13) ^ DTR_ROR(a,22);
        uint32_t maj = (a & b) ^ (a & cc) ^ (b & cc);
        uint32_t t2 = S0 + maj;
        hh=g; g=f; f=e; e=d+t1; d=cc; cc=b; b=a; a=t1+t2;
    }
    c->h[0]+=a; c->h[1]+=b; c->h[2]+=cc; c->h[3]+=d;
    c->h[4]+=e; c->h[5]+=f; c->h[6]+=g; c->h[7]+=hh;
}
static void dtr_sha256_init(dtr_sha256_ctx *c) {
    c->h[0]=0x6a09e667u; c->h[1]=0xbb67ae85u; c->h[2]=0x3c6ef372u; c->h[3]=0xa54ff53au;
    c->h[4]=0x510e527fu; c->h[5]=0x9b05688cu; c->h[6]=0x1f83d9abu; c->h[7]=0x5be0cd19u;
    c->bitlen=0; c->buflen=0;
}
static void dtr_sha256_update(dtr_sha256_ctx *c, const unsigned char *data, size_t n) {
    c->bitlen += (uint64_t)n * 8;
    while (n) {
        size_t take = 64 - c->buflen; if (take > n) take = n;
        memcpy(c->buf + c->buflen, data, take);
        c->buflen += take; data += take; n -= take;
        if (c->buflen == 64) { dtr_sha256_block(c, c->buf); c->buflen = 0; }
    }
}
static char *dtr_sha256_hex(const unsigned char *data, size_t n) {
    dtr_sha256_ctx c; dtr_sha256_init(&c); dtr_sha256_update(&c, data, n);
    uint64_t bits = c.bitlen;
    unsigned char pad = 0x80; dtr_sha256_update(&c, &pad, 1);
    unsigned char zero = 0; while (c.buflen != 56) dtr_sha256_update(&c, &zero, 1);
    unsigned char len[8];
    for (int i = 0; i < 8; ++i) len[i] = (unsigned char)(bits >> (56 - 8*i));
    dtr_sha256_update(&c, len, 8);
    static const char hx[] = "0123456789abcdef";
    char *out = (char *)malloc(65);
    if (out) {
        for (int i = 0; i < 8; ++i) {
            out[i*8]   = hx[(c.h[i]>>28)&15]; out[i*8+1] = hx[(c.h[i]>>24)&15];
            out[i*8+2] = hx[(c.h[i]>>20)&15]; out[i*8+3] = hx[(c.h[i]>>16)&15];
            out[i*8+4] = hx[(c.h[i]>>12)&15]; out[i*8+5] = hx[(c.h[i]>>8)&15];
            out[i*8+6] = hx[(c.h[i]>>4)&15];  out[i*8+7] = hx[c.h[i]&15];
        }
        out[64] = '\0';
    }
    return out;
}

static json_t *dtr_result(const char *state, const char *mode, int has_next, double next) {
    json_t *arr = json_array();
    json_append(arr, json_string(state));
    json_append(arr, mode ? json_string(mode) : json_null());
    json_append(arr, has_next ? json_number(next) : json_null());
    return arr;
}

/* classify(entry, stop_cause) -> (state, resume_mode, next_attempt_at) */
json_t *w18_delegateto_d76c3e_classify(const json_t *entry, const char *stop_cause, double now) {
    const char *status = json_get_str(entry, "status", "");
    const char *exit_reason = json_get_str(entry, "exit_reason", "");
    const char *sc = stop_cause ? stop_cause : "";

    if (status && !strcmp(status, "completed") && exit_reason && !strcmp(exit_reason, "completed"))
        return dtr_result("completed", NULL, 0, 0);
    if (status && !strcmp(status, "interrupted")) {
        if (!strcmp(sc, "gateway_shutdown")) return dtr_result("resumable", "AUTO_ON_BOOT", 0, 0);
        return dtr_result("stopped", NULL, 0, 0);
    }
    if (status && !strcmp(status, "unknown"))
        return dtr_result("resumable", "AUTO_ON_BOOT", 0, 0);

    const char *reason = json_get_str(entry, "failure_reason", "");
    if (!reason) reason = "";
    json_t *resets = entry ? json_obj_get(entry, "failure_resets_at") : NULL;
    if (resets && resets->type == JSON_NUMBER &&
        (!strcmp(reason, "billing") || !strcmp(reason, "rate_limit") || !strcmp(reason, "upstream_rate_limit")))
        return dtr_result("resumable", "AUTO_AT", 1, resets->num_val);

    const char *mode = "ON_FIX";
    double delay = 0.0; int has_delay = 0;
    if      (!strcmp(reason, "rate_limit"))         { mode = "AUTO_AT"; delay = 15*60.0; has_delay = 1; }
    else if (!strcmp(reason, "upstream_rate_limit")){ mode = "AUTO_AT"; delay = 15*60.0; has_delay = 1; }
    else if (!strcmp(reason, "timeout"))            { mode = "AUTO_AT"; delay = 5*60.0;  has_delay = 1; }
    else if (!strcmp(reason, "overloaded"))         { mode = "AUTO_AT"; delay = 5*60.0;  has_delay = 1; }
    else if (!strcmp(reason, "server_error"))       { mode = "AUTO_AT"; delay = 5*60.0;  has_delay = 1; }
    else if (!strcmp(reason, "session_busy"))       { mode = "AUTO_AT"; delay = 30.0;    has_delay = 1; }
    else if (!strcmp(reason, "interpreter_shutdown"))  mode = "AUTO_ON_BOOT";
    else if (!strcmp(reason, "content_policy_blocked"))mode = "HANDOFF";
    else if (!strcmp(reason, "context_overflow") || !strcmp(reason, "payload_too_large") ||
             !strcmp(reason, "image_too_large") || !strcmp(reason, "image_corrupt") ||
             !strcmp(reason, "thinking_signature") || !strcmp(reason, "invalid_encrypted_content"))
        mode = "REPAIR_THEN_CONTINUE";

    return dtr_result("resumable", mode, has_delay ? 1 : 0, has_delay ? (now + delay) : 0);
}

/* endpoint_identity(provider, base_url) -> (provider, sha256(str(base_url)) | None) */
json_t *w18_delegateto_d76c3e_endpoint_identity(const json_t *provider, const char *base_url_or_null) {
    json_t *arr = json_array();
    json_append(arr, provider ? json_copy(provider) : json_null());
    if (base_url_or_null) {
        char *h = dtr_sha256_hex((const unsigned char *)base_url_or_null, strlen(base_url_or_null));
        json_append(arr, h ? json_string(h) : json_null());
        free(h);
    } else {
        json_append(arr, json_null());
    }
    return arr;
}
