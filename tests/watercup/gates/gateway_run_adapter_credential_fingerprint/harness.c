/* gate: gateway_run_adapter_credential_fingerprint — traced copy of
 * src/gateway/run_pure2.c::gw_adapter_credential_fingerprint.
 * LIVE twin: gateway/run.py::GatewayRunner._adapter_credential_fingerprint.
 * SHA-256 here is a compact public-domain-style implementation hashing the
 * exact same bytes as the port's lib/libhash call (hash.c needs OpenSSL at
 * link time, which wc_run's flag ordering can't satisfy — noted in gate.json).
 *
 * Input: one argv token per cup; the literal "null" encodes an adapter with
 * NO discoverable credential (both sides return None/NULL).
 * Rung order (must match twin.py EXACTLY):
 *   B1  credential discovered (non-empty after strip)
 *   V   fingerprint hex[:16], or "" when none
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "watercup.h"

/* ── compact SHA-256 ── */
typedef struct { uint32_t s[8]; uint64_t len; uint8_t buf[64]; size_t n; } sha256_ctx;
static const uint32_t K[64] = {
0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
#define ROR(x,n) (((x)>>(n))|((x)<<(32-(n))))
static void sha256_block(sha256_ctx *c, const uint8_t *p) {
    uint32_t w[64], a,b,cc,d,e,f,g,h,t1,t2;
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[i*4]<<24)|((uint32_t)p[i*4+1]<<16)|((uint32_t)p[i*4+2]<<8)|p[i*4+3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ROR(w[i-15],7)^ROR(w[i-15],18)^(w[i-15]>>3);
        uint32_t s1 = ROR(w[i-2],17)^ROR(w[i-2],19)^(w[i-2]>>10);
        w[i] = w[i-16]+s0+w[i-7]+s1;
    }
    a=c->s[0];b=c->s[1];cc=c->s[2];d=c->s[3];e=c->s[4];f=c->s[5];g=c->s[6];h=c->s[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = ROR(e,6)^ROR(e,11)^ROR(e,25);
        uint32_t ch = (e&f)^((~e)&g);
        t1 = h+S1+ch+K[i]+w[i];
        uint32_t S0 = ROR(a,2)^ROR(a,13)^ROR(a,22);
        uint32_t mj = (a&b)^(a&cc)^(b&cc);
        t2 = S0+mj;
        h=g;g=f;f=e;e=d+t1;d=cc;cc=b;b=a;a=t1+t2;
    }
    c->s[0]+=a;c->s[1]+=b;c->s[2]+=cc;c->s[3]+=d;
    c->s[4]+=e;c->s[5]+=f;c->s[6]+=g;c->s[7]+=h;
}
static void sha256_hex(const uint8_t *data, size_t len, char *hex /*65 bytes*/) {
    sha256_ctx c;
    static const uint32_t iv[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
                                   0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    for (int i = 0; i < 8; i++) c.s[i] = iv[i];
    c.len = 0; c.n = 0;
    size_t total = len;
    const uint8_t *p = data;
    while (len >= 64) { sha256_block(&c, p); p += 64; len -= 64; }
    uint8_t tail[128]; size_t tn = len;
    memcpy(tail, p, tn);
    tail[tn++] = 0x80;
    while (tn % 64 != 56) tail[tn++] = 0;
    uint64_t bits = (uint64_t)total * 8;
    for (int i = 7; i >= 0; i--) tail[tn++] = (uint8_t)(bits >> (i*8));
    for (size_t i = 0; i < tn; i += 64) sha256_block(&c, tail + i);
    static const char *HX = "0123456789abcdef";
    for (int i = 0; i < 8; i++)
        for (int j = 0; j < 4; j++)
            hex[i*8+j*2] = HX[(c.s[i] >> (24-j*8+4)) & 0xf],
            hex[i*8+j*2+1] = HX[(c.s[i] >> (24-j*8)) & 0xf];
    hex[64] = '\0';
}

/* Faithful traced copy of run_pure2.c::gw_adapter_credential_fingerprint
 * (token arrives pre-extracted from adapter attrs; attr discovery is env
 * plumbing outside this pure rung). */
static void wc_fingerprint(const char *token) {
    WCUP_BEGIN("adapter_credential_fingerprint");
    char result[17] = "";
    int have = 0;
    if (!token) {
        WCUP_BRANCH(0);            /* B1: no credential */
        goto out;
    }
    /* .strip() */
    while (*token == ' ' || *token == '\t' || *token == '\n' || *token == '\r')
        token++;
    {
        size_t tn = strlen(token);
        while (tn > 0 && (token[tn-1] == ' ' || token[tn-1] == '\t' ||
                          token[tn-1] == '\n' || token[tn-1] == '\r')) tn--;
        WCUP_BRANCH(tn > 0);       /* B1 */
        if (tn > 0) {
            static const char *PREFIX = "hermes-mux:";
            size_t pn = strlen(PREFIX);
            char *buf = malloc(pn + tn + 1);
            memcpy(buf, PREFIX, pn);
            memcpy(buf + pn, token, tn);
            buf[pn + tn] = '\0';
            char hex[65];
            sha256_hex((const unsigned char *)buf, pn + tn, hex);
            free(buf);
            memcpy(result, hex, 16);
            result[16] = '\0';
            have = 1;
        }
    }
out:
    {
        WCUP_VALSTR(result);
        char cup[32];
        WCUP_FINALIZE(cup, sizeof cup);
        printf("%s\n", cup);
        WCUP_END("adapter_credential_fingerprint");
        (void)have;
    }
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s wc_fingerprint <token|null>...\n", argv[0]); return 2; }
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "null") == 0) wc_fingerprint(NULL);
        else wc_fingerprint(argv[i]);
    }
    return 0;
}
