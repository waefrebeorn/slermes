/*
 * local.c -- real C11 port of upstream Hermes agent/vault_backends/local.py.
 *
 * PoP: w22_local_259750_backend_info @ agent/vault_backends/local.py:LocalLoginBackend
 * PoP: w22_local_259750_list_items @ agent/vault_backends/local.py:LocalLoginBackend.list_items
 * PoP: w22_local_259750_get_meta @ agent/vault_backends/local.py:LocalLoginBackend.get_meta
 * PoP: w22_local_259750_resolve_password @ agent/vault_backends/local.py:LocalLoginBackend.resolve_password
 * PoP: w22_local_259750_resolve_secret @ agent/vault_backends/local.py:LocalLoginBackend.resolve_secret
 * PoP: w22_local_259750_resolve_otp @ agent/vault_backends/local.py:LocalLoginBackend.resolve_otp
 *
 * The backend is the always-on default login backend: it lists the Fernet
 * vault's metadata and resolves one item's secret at fill time. The store
 * (agent.vault_store.VaultStore -- a decrypted file walk plus a Fernet key) is
 * the DB seam, and the wall clock is the second one. Everything the backend
 * does with what comes back is computed here:
 *
 *   - the VaultItemMeta rendering of a record (VaultStore._meta + to_dict),
 *   - the `str(v)` coercion of every secret value in resolve_secret,
 *   - the `or ""` fallback of resolve_password,
 *   - the RFC 6238 code resolve_otp mints (agent.vault_store.totp_now), with
 *     the SHA1 / SHA256 / SHA512 digests carried in this file.
 *
 * Records are the store's decrypted rows, supplied by the caller that owns the
 * key: {"id","kind","label","origin","created_at","identifier",
 * "identifier_type","secret":{...}}. A handle that names no row raises
 * VaultError("no vault item with id <handle!r>") upstream; here that is the
 * {"ok":false,"error":...} answer.
 */
#include "hermes_json.h"
#include "noc_w22_local_259750.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- SHA-1 / SHA-256 / SHA-512 one-shot digests --------------------------- */

static unsigned int vl_ror32(unsigned int x, unsigned int n) {
    return (x >> n) | (x << (32 - n));
}

static unsigned int vl_rol32(unsigned int x, unsigned int n) {
    return (x << n) | (x >> (32 - n));
}

static unsigned long long vl_ror64(unsigned long long x, unsigned int n) {
    return (x >> n) | (x << (64 - n));
}

static unsigned char *vl_pad(const unsigned char *msg, size_t len, size_t block,
                             size_t length_bytes, size_t *padded_len) {
    size_t padlen = (len + 1 + length_bytes + block - 1) / block * block;
    unsigned char *buf = (unsigned char *)calloc(1, padlen ? padlen : block);
    unsigned long long bits = (unsigned long long)len * 8ull;
    size_t i;
    if (!buf) return NULL;
    if (len) memcpy(buf, msg, len);
    buf[len] = 0x80;
    /* The 128-bit length field SHA-512 wants is this 64-bit count, high half
     * zero; writing past the low 8 bytes would shift by >= 64 (undefined). */
    for (i = 0; i < length_bytes && i < 8; i++) {
        buf[padlen - 1 - i] = (unsigned char)((bits >> (8 * i)) & 0xFFu);
    }
    *padded_len = padlen;
    return buf;
}

static void vl_sha1(const unsigned char *msg, size_t len, unsigned char out[20]) {
    unsigned int h[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};
    unsigned char *buf;
    size_t padlen = 0, off, i;
    buf = vl_pad(msg, len, 64, 8, &padlen);
    if (!buf) { memset(out, 0, 20); return; }
    for (off = 0; off < padlen; off += 64) {
        unsigned int w[80];
        unsigned int a, b, c, d, e;
        for (i = 0; i < 16; i++) {
            w[i] = ((unsigned int)buf[off + 4 * i] << 24)
                 | ((unsigned int)buf[off + 4 * i + 1] << 16)
                 | ((unsigned int)buf[off + 4 * i + 2] << 8)
                 | (unsigned int)buf[off + 4 * i + 3];
        }
        for (i = 16; i < 80; i++) {
            w[i] = vl_rol32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        }
        a = h[0]; b = h[1]; c = h[2]; d = h[3]; e = h[4];
        for (i = 0; i < 80; i++) {
            unsigned int f, k, tmp;
            if (i < 20) { f = (b & c) | ((~b) & d); k = 0x5A827999u; }
            else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1u; }
            else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDCu; }
            else { f = b ^ c ^ d; k = 0xCA62C1D6u; }
            tmp = vl_rol32(a, 5) + f + e + k + w[i];
            e = d; d = c; c = vl_rol32(b, 30); b = a; a = tmp;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
    }
    free(buf);
    for (i = 0; i < 5; i++) {
        out[4 * i] = (unsigned char)((h[i] >> 24) & 0xFFu);
        out[4 * i + 1] = (unsigned char)((h[i] >> 16) & 0xFFu);
        out[4 * i + 2] = (unsigned char)((h[i] >> 8) & 0xFFu);
        out[4 * i + 3] = (unsigned char)(h[i] & 0xFFu);
    }
}

static void vl_sha256(const unsigned char *msg, size_t len, unsigned char out[32]) {
    static const unsigned int K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u,
    0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u,
    0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u,
    0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
    };
    unsigned int h[8] = {
    0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au, 0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u,
    };
    unsigned char *buf;
    size_t padlen = 0, off, i;
    buf = vl_pad(msg, len, 64, 8, &padlen);
    if (!buf) { memset(out, 0, 32); return; }
    for (off = 0; off < padlen; off += 64) {
        unsigned int w[64];
        unsigned int a, b, c, d, e, f, g, hh;
        for (i = 0; i < 16; i++) {
            w[i] = ((unsigned int)buf[off + 4 * i] << 24)
                 | ((unsigned int)buf[off + 4 * i + 1] << 16)
                 | ((unsigned int)buf[off + 4 * i + 2] << 8)
                 | (unsigned int)buf[off + 4 * i + 3];
        }
        for (i = 16; i < 64; i++) {
            unsigned int s0 = vl_ror32(w[i - 15], 7) ^ vl_ror32(w[i - 15], 18) ^ (w[i - 15] >> 3);
            unsigned int s1 = vl_ror32(w[i - 2], 17) ^ vl_ror32(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        a = h[0]; b = h[1]; c = h[2]; d = h[3]; e = h[4]; f = h[5]; g = h[6]; hh = h[7];
        for (i = 0; i < 64; i++) {
            unsigned int S1 = vl_ror32(e, 6) ^ vl_ror32(e, 11) ^ vl_ror32(e, 25);
            unsigned int ch = (e & f) ^ ((~e) & g);
            unsigned int t1 = hh + S1 + ch + K[i] + w[i];
            unsigned int S0 = vl_ror32(a, 2) ^ vl_ror32(a, 13) ^ vl_ror32(a, 22);
            unsigned int maj = (a & b) ^ (a & c) ^ (b & c);
            unsigned int t2 = S0 + maj;
            hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    free(buf);
    for (i = 0; i < 8; i++) {
        out[4 * i] = (unsigned char)((h[i] >> 24) & 0xFFu);
        out[4 * i + 1] = (unsigned char)((h[i] >> 16) & 0xFFu);
        out[4 * i + 2] = (unsigned char)((h[i] >> 8) & 0xFFu);
        out[4 * i + 3] = (unsigned char)(h[i] & 0xFFu);
    }
}

static void vl_sha512(const unsigned char *msg, size_t len, unsigned char out[64]) {
    static const unsigned long long K[80] = {
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL,
    0xe9b5dba58189dbbcULL, 0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL,
    0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL, 0xd807aa98a3030242ULL,
    0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL,
    0xc19bf174cf692694ULL, 0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL,
    0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL, 0x2de92c6f592b0275ULL,
    0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL,
    0xbf597fc7beef0ee4ULL, 0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL,
    0x06ca6351e003826fULL, 0x142929670a0e6e70ULL, 0x27b70a8546d22ffcULL,
    0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
    0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL,
    0x92722c851482353bULL, 0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL,
    0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL, 0xd192e819d6ef5218ULL,
    0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL,
    0x34b0bcb5e19b48a8ULL, 0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL,
    0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL, 0x748f82ee5defb2fcULL,
    0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL,
    0xc67178f2e372532bULL, 0xca273eceea26619cULL, 0xd186b8c721c0c207ULL,
    0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL, 0x06f067aa72176fbaULL,
    0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
    0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL,
    0x431d67c49c100d4cULL, 0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL,
    0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL,
    };
    unsigned long long h[8] = {
    0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL, 0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
    0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL, 0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL,
    };
    unsigned char *buf;
    size_t padlen = 0, off, i;
    buf = vl_pad(msg, len, 128, 16, &padlen);
    if (!buf) { memset(out, 0, 64); return; }
    for (off = 0; off < padlen; off += 128) {
        unsigned long long w[80];
        unsigned long long a, b, c, d, e, f, g, hh;
        for (i = 0; i < 16; i++) {
            unsigned long long v = 0;
            int j;
            for (j = 0; j < 8; j++) v = (v << 8) | (unsigned long long)buf[off + 8 * i + j];
            w[i] = v;
        }
        for (i = 16; i < 80; i++) {
            unsigned long long s0 = vl_ror64(w[i - 15], 1) ^ vl_ror64(w[i - 15], 8) ^ (w[i - 15] >> 7);
            unsigned long long s1 = vl_ror64(w[i - 2], 19) ^ vl_ror64(w[i - 2], 61) ^ (w[i - 2] >> 6);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        a = h[0]; b = h[1]; c = h[2]; d = h[3]; e = h[4]; f = h[5]; g = h[6]; hh = h[7];
        for (i = 0; i < 80; i++) {
            unsigned long long S1 = vl_ror64(e, 14) ^ vl_ror64(e, 18) ^ vl_ror64(e, 41);
            unsigned long long ch = (e & f) ^ ((~e) & g);
            unsigned long long t1 = hh + S1 + ch + K[i] + w[i];
            unsigned long long S0 = vl_ror64(a, 28) ^ vl_ror64(a, 34) ^ vl_ror64(a, 39);
            unsigned long long maj = (a & b) ^ (a & c) ^ (b & c);
            unsigned long long t2 = S0 + maj;
            hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
        h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    free(buf);
    for (i = 0; i < 8; i++) {
        int j;
        for (j = 0; j < 8; j++) {
            out[8 * i + j] = (unsigned char)((h[i] >> (56 - 8 * j)) & 0xFFull);
        }
    }
}

/* --- HMAC over any of the three digests ---------------------------------- */

typedef struct {
    size_t block;
    size_t digest_len;
    void (*hash)(const unsigned char *, size_t, unsigned char *);
} vl_digest;

/* out has room for 128 + 64: the largest block plus the largest digest. */
static void vl_hmac(const vl_digest *d, const unsigned char *key, size_t key_len,
                    const unsigned char *msg, size_t msg_len,
                    unsigned char *out, size_t *out_len) {
    unsigned char k[128];
    unsigned char inner[192];
    unsigned char outer[192];
    unsigned char khash[64];
    size_t i;

    memset(k, 0, sizeof(k));
    if (key_len > d->block) {
        d->hash(key, key_len, khash);
        memcpy(k, khash, d->digest_len);
    } else if (key_len) {
        memcpy(k, key, key_len);
    }
    for (i = 0; i < d->block; i++) {
        inner[i] = (unsigned char)(k[i] ^ 0x36u);
        outer[i] = (unsigned char)(k[i] ^ 0x5cu);
    }
    memcpy(inner + d->block, msg, msg_len);
    d->hash(inner, d->block + msg_len, out);
    memcpy(outer + d->block, out, d->digest_len);
    d->hash(outer, d->block + d->digest_len, out);
    *out_len = d->digest_len;
}

/* --- base32 (RFC 4648, casefold) ----------------------------------------- */

static const char VL_B32[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

static int vl_b32_val(int c) {
    if (c >= 'a' && c <= 'z') c -= 32;
    if (c == '=') return -2;               /* padding: skipped */
    {
        const char *p = strchr(VL_B32, c);
        if (!p) return -1;
        return (int)(p - VL_B32);
    }
}

/* base64.b32decode(seed + "=" * (-len(seed) % 8), casefold=True) */
static int vl_b32_decode(const char *seed, unsigned char *out, size_t out_cap,
                         size_t *out_len) {
    unsigned int acc = 0;
    int bits = 0;
    size_t len = 0;
    const char *p = seed;
    while (*p) {
        int v = vl_b32_val((unsigned char)*p++);
        if (v == -2) continue;
        if (v < 0) return 0;
        acc = (acc << 5) | (unsigned int)v;
        bits += 5;
        if (bits >= 8) {
            bits -= 8;
            if (len >= out_cap) return 0;
            out[len++] = (unsigned char)((acc >> bits) & 0xFFu);
        }
    }
    *out_len = len;
    return 1;
}

/* --- totp_now ----------------------------------------------------------- */

static int vl_digits_of(const char *s, int fallback) {
    long v;
    char *end = NULL;
    if (!s || !*s) return fallback;
    v = strtol(s, &end, 10);
    if (!end || *end) return fallback;
    return (int)v;
}

static void vl_slice(const char *s, size_t n, char *out, size_t cap) {
    if (n >= cap) n = cap - 1;
    if (n) memcpy(out, s, n);
    out[n] = '\0';                         /* NUL-terminate the shortened slice */
}

static char *vl_totp_now(const char *seed, double at) {
    static const vl_digest D_SHA1 = {64, 20, vl_sha1};
    static const vl_digest D_SHA256 = {64, 32, vl_sha256};
    static const vl_digest D_SHA512 = {128, 64, vl_sha512};
    const vl_digest *digest = &D_SHA1;
    const char *rest;
    int digits = 6;
    int period = 30;
    char key_part[512];
    unsigned char key[256];
    size_t key_len = 0;
    unsigned char mac[64];
    size_t mac_len = 0;
    unsigned char counter[8];
    unsigned long long c;
    unsigned int offset, code;
    unsigned long long mod = 1;
    char buf[32];
    char *out;
    int i;

    if (strchr(seed, '|')) {
        /* `seed, d, p, a = seed.split("|", 3)` */
        const char *parts[4];
        size_t lens[4];
        const char *p = seed;
        int nparts = 0;
        char scratch[64];
        while (nparts < 4) {
            const char *bar = strchr(p, '|');
            parts[nparts] = p;
            lens[nparts] = bar ? (size_t)(bar - p) : strlen(p);
            nparts++;
            if (!bar) break;
            p = bar + 1;
        }
        if (nparts < 3) return NULL;       /* ValueError upstream: not enough parts */
        vl_slice(parts[0], lens[0], key_part, sizeof(key_part));
        rest = key_part;
        vl_slice(parts[1], lens[1], scratch, sizeof(scratch));
        digits = vl_digits_of(scratch, digits);
        vl_slice(parts[2], lens[2], scratch, sizeof(scratch));
        period = vl_digits_of(scratch, period);
        if (nparts > 3) {
            /* _OTP_ALGOS.get(a.upper(), "sha1") */
            size_t k;
            vl_slice(parts[3], lens[3], scratch, sizeof(scratch));
            for (k = 0; scratch[k]; k++) {
                if (scratch[k] >= 'a' && scratch[k] <= 'z') scratch[k] = (char)(scratch[k] - 32);
            }
            if (!strcmp(scratch, "SHA256")) digest = &D_SHA256;
            else if (!strcmp(scratch, "SHA512")) digest = &D_SHA512;
        }
    } else {
        vl_slice(seed, strlen(seed), key_part, sizeof(key_part));
        rest = key_part;
    }
    if (period <= 0) return NULL;
    if (!vl_b32_decode(rest, key, sizeof(key), &key_len)) return NULL;
    if (!key_len) return NULL;

    c = (unsigned long long)floor(at / (double)period);
    for (i = 0; i < 8; i++) counter[i] = (unsigned char)((c >> (56 - 8 * i)) & 0xFFull);
    vl_hmac(digest, key, key_len, counter, sizeof(counter), mac, &mac_len);
    if (mac_len > 64) mac_len = 64;
    offset = (unsigned int)(mac[mac_len - 1] & 0x0Fu);
    if (offset + 4 > mac_len) offset = (unsigned int)(mac_len - 4);
    code = ((unsigned int)mac[offset] << 24) | ((unsigned int)mac[offset + 1] << 16)
         | ((unsigned int)mac[offset + 2] << 8) | (unsigned int)mac[offset + 3];
    code &= 0x7FFFFFFFu;
    for (i = 0; i < digits && i < 10; i++) mod *= 10ull;
    code %= (unsigned int)mod;
    snprintf(buf, sizeof(buf), "%0*u", digits, code);
    out = (char *)malloc(strlen(buf) + 1);
    if (out) memcpy(out, buf, strlen(buf) + 1);
    return out;
}

/* --- Python str() / repr() over a decoded value -------------------------- */

static int vl_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0 ? 1 : 0;
    case JSON_STRING: return (v->str_val && v->str_val[0]) ? 1 : 0;
    case JSON_ARRAY:
    case JSON_OBJECT: return v->c.count != 0 ? 1 : 0;
    }
    return 0;
}

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
    int bad;
} vl_buf;

static void vl_buf_init(vl_buf *b) {
    b->cap = 64;
    b->len = 0;
    b->bad = 0;
    b->buf = (char *)malloc(b->cap);
    if (b->buf) b->buf[0] = '\0';
    else b->bad = 1;
}

static void vl_buf_add(vl_buf *b, const char *s, size_t n) {
    if (b->bad) return;
    if (!s || n == 0) return;             /* empty path: nothing appended */
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap;
        char *grown;
        while (b->len + n + 1 > cap) cap *= 2;
        grown = (char *)realloc(b->buf, cap);
        if (!grown) { b->bad = 1; return; }
        b->buf = grown;
        b->cap = cap;
    }
    memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = '\0';                /* NUL-terminate after each append */
}

static void vl_buf_adds(vl_buf *b, const char *s) {
    if (s) vl_buf_add(b, s, strlen(s));
}

static char *vl_buf_take(vl_buf *b) {
    if (b->bad) { free(b->buf); b->buf = NULL; return NULL; }
    b->buf[b->len] = '\0';
    return b->buf;
}

/* Python float repr: the shortest of %.15g/%.16g/%.17g that round-trips. */
static void vl_float_repr(double v, char *out, size_t cap) {
    char tmp[64];
    int prec;
    for (prec = 15; prec <= 17; prec++) {
        snprintf(tmp, sizeof(tmp), "%.*g", prec, v);
        if (strtod(tmp, NULL) == v) break;
    }
    snprintf(out, cap, "%s", tmp);
}

static void vl_repr_into(vl_buf *b, const json_t *v);

static void vl_repr_str_into(vl_buf *b, const char *s) {
    char quote = '\'';
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    if (!s) s = "";
    if (strchr(s, '\'') && !strchr(s, '"')) quote = '"';
    vl_buf_add(b, &quote, 1);
    while (*p) {
        unsigned char c = *p++;
        if (c == (unsigned char)quote || c == '\\') {
            char esc[2];
            esc[0] = '\\';
            esc[1] = (char)c;
            vl_buf_add(b, esc, 2);
        } else if (c == '\n') {
            vl_buf_adds(b, "\\n");
        } else if (c == '\r') {
            vl_buf_adds(b, "\\r");
        } else if (c == '\t') {
            vl_buf_adds(b, "\\t");
        } else if (c < 0x20 || c == 0x7f) {
            char esc[8];
            snprintf(esc, sizeof(esc), "\\x%02x", (unsigned)c);
            vl_buf_adds(b, esc);
        } else {
            vl_buf_add(b, (const char *)&c, 1);
        }
    }
    vl_buf_add(b, &quote, 1);
}

static void vl_str_into(vl_buf *b, const json_t *v) {
    char num[64];
    if (!v) { vl_buf_adds(b, "None"); return; }
    switch (v->type) {
    case JSON_NULL:   vl_buf_adds(b, "None"); break;
    case JSON_BOOL:   vl_buf_adds(b, v->bool_val ? "True" : "False"); break;
    case JSON_STRING: vl_buf_adds(b, v->str_val ? v->str_val : ""); break;
    case JSON_NUMBER:
        if (v->num_val == (double)(long long)v->num_val
            && v->num_val >= -9.2e18 && v->num_val <= 9.2e18) {
            snprintf(num, sizeof(num), "%lld", (long long)v->num_val);
        } else {
            vl_float_repr(v->num_val, num, sizeof(num));
        }
        vl_buf_adds(b, num);
        break;
    default:
        vl_repr_into(b, v);
        break;
    }
}

static void vl_repr_into(vl_buf *b, const json_t *v) {
    size_t i;
    if (!v) { vl_buf_adds(b, "None"); return; }
    switch (v->type) {
    case JSON_STRING:
        vl_repr_str_into(b, v->str_val);
        break;
    case JSON_ARRAY:
        vl_buf_adds(b, "[");
        for (i = 0; i < v->c.count; i++) {
            if (i) vl_buf_adds(b, ", ");
            vl_repr_into(b, v->c.items[i]);
        }
        vl_buf_adds(b, "]");
        break;
    case JSON_OBJECT:
        vl_buf_adds(b, "{");
        for (i = 0; i < v->c.count; i++) {
            if (i) vl_buf_adds(b, ", ");
            vl_repr_str_into(b, v->c.keys[i]);
            vl_buf_adds(b, ": ");
            vl_repr_into(b, v->c.items[i]);
        }
        vl_buf_adds(b, "}");
        break;
    default:
        vl_str_into(b, v);
        break;
    }
}

static char *vl_str(const json_t *v) {
    vl_buf b;
    vl_buf_init(&b);
    vl_str_into(&b, v);
    return vl_buf_take(&b);
}

/* --- the store seam ------------------------------------------------------ */

static json_t *vl_wrap_ok(json_t *value) {
    json_t *out = json_object();
    json_set(out, "ok", json_bool(1));
    json_set(out, "value", value ? value : json_null());
    return out;
}

static json_t *vl_wrap_err(const char *message) {
    json_t *out = json_object();
    json_set(out, "ok", json_bool(0));
    json_set(out, "error", json_string(message ? message : ""));
    return out;
}

/* VaultError(f"no vault item with id {item_id!r}") */
static json_t *vl_no_item(const char *handle) {
    vl_buf b;
    char *message;
    json_t *out;
    vl_buf_init(&b);
    vl_buf_adds(&b, "no vault item with id ");
    if (handle) vl_repr_str_into(&b, handle);
    else vl_buf_adds(&b, "None");
    message = vl_buf_take(&b);
    out = vl_wrap_err(message ? message : "");
    free(message);
    return out;
}

/* rec.get("id") == handle: a handle is the string a caller was given, so an
 * integer (or null) id never matches one; a NULL handle is None, which does
 * match a row whose id is None. */
static int vl_id_matches(const json_t *rec, const char *handle) {
    json_t *id = json_obj_get(rec, "id");
    if (!handle) return !vl_truthy(id);
    if (!id || id->type != JSON_STRING) return 0;
    return !strcmp(id->str_val ? id->str_val : "", handle);
}

static const json_t *vl_find(const json_t *records, const char *handle) {
    size_t i;
    if (!records || records->type != JSON_ARRAY) return NULL;
    for (i = 0; i < records->c.count; i++) {
        json_t *rec = records->c.items[i];
        if (!rec || rec->type != JSON_OBJECT) continue;
        if (vl_id_matches(rec, handle)) return rec;
    }
    return NULL;
}

/* rec.get(key, "") through str(): an absent key is the "" default, a present
 * null is Python's "None". */
static json_t *vl_str_default(const json_t *rec, const char *key) {
    json_t *v = json_obj_get(rec, key);
    char *text;
    json_t *out;
    if (!v) return json_string("");
    text = vl_str(v);
    out = json_string(text ? text : "");
    free(text);
    return out;
}

/* VaultStore._meta(rec).to_dict() */
static json_t *vl_meta(const json_t *rec) {
    json_t *out = json_object();
    json_t *identifier = json_obj_get(rec, "identifier");
    json_t *secret = json_obj_get(rec, "secret");
    json_t *otp;

    json_set(out, "id", vl_str_default(rec, "id"));
    json_set(out, "kind", vl_str_default(rec, "kind"));
    json_set(out, "label", vl_str_default(rec, "label"));
    {
        json_t *origin = json_obj_get(rec, "origin");
        json_set(out, "origin", origin ? json_copy(origin) : json_null());
    }
    json_set(out, "created_at", vl_str_default(rec, "created_at"));
    if (vl_truthy(identifier)) {
        json_set(out, "identifier", json_copy(identifier));
        {
            json_t *itype = json_obj_get(rec, "identifier_type");
            json_set(out, "identifier_type", itype ? json_copy(itype) : json_null());
        }
    }
    otp = (secret && secret->type == JSON_OBJECT && secret->c.count)
            ? json_obj_get(secret, "otp_secret") : NULL;
    if (vl_truthy(otp)) json_set(out, "has_otp", json_bool(1));
    /* allowed_origins is () on this backend, so to_dict never adds it. */
    return out;
}

/* --- exported surface ---------------------------------------------------- */

json_t *w22_local_259750_backend_info(void) {
    json_t *out = json_object();
    json_set(out, "name", json_string("local"));
    json_set(out, "display_name", json_string("Hermes vault"));
    json_set(out, "prefix", json_string("vault_"));
    json_set(out, "needs_unlock", json_bool(0));
    return out;
}

json_t *w22_local_259750_list_items(const json_t *records) {
    json_t *arr = json_array();
    size_t i;
    if (records && records->type == JSON_ARRAY) {
        for (i = 0; i < records->c.count; i++) {
            json_t *rec = records->c.items[i];
            if (!rec || rec->type != JSON_OBJECT) continue;
            json_append(arr, vl_meta(rec));
        }
    }
    return vl_wrap_ok(arr);
}

json_t *w22_local_259750_get_meta(const json_t *records, const char *handle) {
    const json_t *rec = vl_find(records, handle);
    if (!rec) return vl_wrap_ok(json_null());
    return vl_wrap_ok(vl_meta(rec));
}

json_t *w22_local_259750_resolve_password(const json_t *records, const char *handle) {
    const json_t *rec = vl_find(records, handle);
    const json_t *secret;
    json_t *password;
    json_t *out;
    char *text;
    if (!rec) return vl_no_item(handle);
    secret = json_obj_get(rec, "secret");
    password = (secret && secret->type == JSON_OBJECT) ? json_obj_get(secret, "password") : NULL;
    /* str(secret.get("password") or "") */
    if (!vl_truthy(password)) return vl_wrap_ok(json_string(""));
    text = vl_str(password);
    out = vl_wrap_ok(json_string(text ? text : ""));
    free(text);
    return out;
}

json_t *w22_local_259750_resolve_secret(const json_t *records, const char *handle) {
    const json_t *rec = vl_find(records, handle);
    const json_t *secret;
    json_t *out;
    size_t i;
    if (!rec) return vl_no_item(handle);
    secret = json_obj_get(rec, "secret");
    out = json_object();
    if (secret && secret->type == JSON_OBJECT) {
        for (i = 0; i < secret->c.count; i++) {
            const char *key = secret->c.keys[i];
            char *text;
            if (!key) continue;
            text = vl_str(secret->c.items[i]);
            json_set(out, key, json_string(text ? text : ""));
            free(text);
        }
    }
    return vl_wrap_ok(out);
}

json_t *w22_local_259750_resolve_otp(const json_t *records, const char *handle,
                                     const char *at_epoch) {
    const json_t *rec = vl_find(records, handle);
    const json_t *secret;
    json_t *seed_value;
    char *seed;
    double at;
    json_t *out;
    if (!rec) return vl_no_item(handle);
    secret = json_obj_get(rec, "secret");
    seed_value = (secret && secret->type == JSON_OBJECT) ? json_obj_get(secret, "otp_secret") : NULL;
    /* seed = str(secret.get("otp_secret") or ""); return totp_now(seed) if seed else None */
    if (!vl_truthy(seed_value)) return vl_wrap_ok(json_null());
    seed = vl_str(seed_value);
    if (!seed || !seed[0]) { free(seed); return vl_wrap_ok(json_null()); }
    at = at_epoch ? strtod(at_epoch, NULL) : 0.0;
    {
        char *code = vl_totp_now(seed, at);
        free(seed);
        out = vl_wrap_ok(code ? json_string(code) : json_null());
        free(code);
    }
    return out;
}
