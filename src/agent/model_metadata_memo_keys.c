/*
 * model_metadata_memo_keys.c — C port of upstream Hermes
 * agent/model_metadata_memo_keys.py (and the fingerprint it depends on from
 * agent/credential_persistence.py).
 *
 * PoP: memokeys_fingerprint @ agent/credential_persistence.py:fingerprint_secret_value
 * PoP: memokeys_endpoint_memo_key @ agent/model_metadata_memo_keys.py:endpoint_memo_key
 * PoP: memokeys_endpoint_disk_slot @ agent/model_metadata_memo_keys.py:endpoint_disk_slot
 * PoP: memokeys_minted_slot @ agent/model_metadata_memo_keys.py:endpoint_memo_key
 *
 * SHA-256 is implemented locally so the port has no external object deps.
 */
#include "noc_memokeys.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ------------------------------- SHA-256 ------------------------------- */
static const uint32_t SHA_K[64] = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
};
static uint32_t rotr32(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
static void sha256_block(uint32_t h[8], const unsigned char b[64]) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
        w[i] = ((uint32_t)b[4*i] << 24) | ((uint32_t)b[4*i+1] << 16) |
               ((uint32_t)b[4*i+2] << 8) | (uint32_t)b[4*i+3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = rotr32(w[i-15],7) ^ rotr32(w[i-15],18) ^ (w[i-15] >> 3);
        uint32_t s1 = rotr32(w[i-2],17) ^ rotr32(w[i-2],19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a=h[0],bb=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = rotr32(e,6) ^ rotr32(e,11) ^ rotr32(e,25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = hh + S1 + ch + SHA_K[i] + w[i];
        uint32_t S0 = rotr32(a,2) ^ rotr32(a,13) ^ rotr32(a,22);
        uint32_t maj = (a & bb) ^ (a & c) ^ (bb & c);
        uint32_t t2 = S0 + maj;
        hh=g; g=f; f=e; e=d+t1; d=c; c=bb; bb=a; a=t1+t2;
    }
    h[0]+=a; h[1]+=bb; h[2]+=c; h[3]+=d; h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=hh;
}
static void sha256(const unsigned char *data, size_t len, unsigned char out[32]) {
    static const uint32_t init[8] = {
        0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,
        0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u };
    uint32_t h[8]; memcpy(h, init, sizeof h);
    size_t total = len, pos = 0;
    while (len >= 64) { sha256_block(h, data + pos); pos += 64; len -= 64; }
    unsigned char block[64];
    memcpy(block, data + pos, len);
    block[len] = 0x80;
    if (len >= 56) {
        memset(block + len + 1, 0, 64 - len - 1);
        sha256_block(h, block);
        memset(block, 0, 64);
    } else {
        memset(block + len + 1, 0, 56 - len - 1);
    }
    uint64_t bits = (uint64_t)total * 8u;
    for (int i = 0; i < 8; i++) block[56 + i] = (unsigned char)(bits >> (56 - 8*i));
    sha256_block(h, block);
    static const char hex[] = "0123456789abcdef";
    /* First 16 hex chars of the digest == full 8 hex digits of h[0] and h[1]. */
    for (int i = 0; i < 2; i++) {
        for (int j = 7; j >= 0; j--)
            out[i*8 + (7 - j)] = (unsigned char)hex[(h[i] >> (4*j)) & 0xf];
    }
}

/* Python: text = "" if value is None else str(value); if not text: return None;
 *         digest = sha256(text.encode("utf-8")).hexdigest(); return f"sha256:{digest[:16]}" */
char *memokeys_fingerprint(const char *value) {
    if (!value || !value[0]) return NULL;
    unsigned char digest[32];
    sha256((const unsigned char *)value, strlen(value), digest);
    char *out = (char *)malloc(7 + 16 + 1);
    if (!out) return NULL;
    memcpy(out, "sha256:", 7);
    memcpy(out + 7, digest, 16);
    out[23] = '\0';
    return out;
}

/* Python: if api_key is None or isinstance(api_key, str):
 *            return normalized, fingerprint_secret_value(api_key) or ""
 *         (minted-object path is not reproducible off-process; see memokeys_minted_slot). */
void memokeys_endpoint_memo_key(const char *normalized, const char *api_key,
                                int api_key_is_null, char **out_norm, char **out_slot) {
    *out_norm = strdup(normalized ? normalized : "");
    if (api_key_is_null) {
        *out_slot = strdup("");
    } else {
        char *fp = memokeys_fingerprint(api_key);
        *out_slot = fp ? fp : strdup("");
    }
}

/* Python: normalized, slot = memo_key
 *         if not slot: return normalized
 *         return f"{normalized} [{'minted' if slot.startswith('minted:') else slot}]" */
char *memokeys_endpoint_disk_slot(const char *normalized, const char *slot) {
    const char *n = normalized ? normalized : "";
    if (!slot || !slot[0]) return strdup(n);
    const char *label = (strncmp(slot, "minted:", 7) == 0) ? "minted" : slot;
    size_t len = strlen(n) + strlen(" [") + strlen(label) + strlen("]");
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    int k = 0;
    memcpy(out + k, n, strlen(n)); k += (int)strlen(n);
    memcpy(out + k, " [", 2); k += 2;
    memcpy(out + k, label, strlen(label)); k += (int)strlen(label);
    out[k++] = ']';
    out[k] = '\0';
    return out;
}

/* Python: f"minted:{id(api_key):x}" -- id supplied explicitly as a decimal string. */
char *memokeys_minted_slot(const char *id_decimal) {
    long long v = strtoll(id_decimal ? id_decimal : "0", NULL, 10);
    char *out = (char *)malloc(7 + 32 + 1);
    if (!out) return NULL;
    snprintf(out, 40, "minted:%llx", (unsigned long long)v);
    return out;
}
