/*
 * desktop_build_lock.c — faithful C11 port of hermes_cli/desktop_build_lock.py
 * (gate w17_desktopbui_731b67).
 *
 * PoP: w17_desktopbui_731b67_checkout_key @ hermes_cli/desktop_build_lock.py:DesktopBuildLock.__init__
 * PoP: w17_desktopbui_731b67_lock_name @ hermes_cli/desktop_build_lock.py:DesktopBuildLock.__init__
 *
 * DesktopBuildLock keys its profile-common lock file by the SHA-256 of the
 * normcase'd, resolved checkout path. On Windows str(Path(...)) emits
 * backslashes and normcase lowercases, so a root passed as "C:/a/../b"
 * resolves to the single string "c:\\a\\b" before hashing.
 */
#include "noc_w17_desktopbui_731b67.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- self-contained SHA-256 (no external .o dependency) --- */
typedef struct { unsigned int h[8]; unsigned char buf[64]; size_t used; unsigned long long total; } Sha;

static const unsigned int SHA_K[64] = {
    0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
    0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
    0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
    0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
    0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
    0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
    0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
    0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
};

static unsigned int rotr32(unsigned int x, int n) { return (x >> n) | (x << (32 - n)); }

static void sha_compress(Sha *s, const unsigned char *p) {
    unsigned int w[64], a, b, c, d, e, f, g, h, t1, t2, x, y;
    int i;
    for (i = 0; i < 16; ++i)
        w[i] = ((unsigned int)p[i*4] << 24) | ((unsigned int)p[i*4+1] << 16) |
               ((unsigned int)p[i*4+2] << 8) | (unsigned int)p[i*4+3];
    for (i = 16; i < 64; ++i) {
        x = rotr32(w[i-15], 7) ^ rotr32(w[i-15], 18) ^ (w[i-15] >> 3);
        y = rotr32(w[i-2], 17) ^ rotr32(w[i-2], 19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + x + w[i-7] + y;
    }
    a=s->h[0]; b=s->h[1]; c=s->h[2]; d=s->h[3];
    e=s->h[4]; f=s->h[5]; g=s->h[6]; h=s->h[7];
    for (i = 0; i < 64; ++i) {
        x = rotr32(e,6) ^ rotr32(e,11) ^ rotr32(e,25);
        t1 = h + x + ((e & f) ^ (~e & g)) + SHA_K[i] + w[i];
        y = rotr32(a,2) ^ rotr32(a,13) ^ rotr32(a,22);
        t2 = y + ((a & b) ^ (a & c) ^ (b & c));
        h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
    }
    s->h[0]+=a; s->h[1]+=b; s->h[2]+=c; s->h[3]+=d;
    s->h[4]+=e; s->h[5]+=f; s->h[6]+=g; s->h[7]+=h;
}

static void sha_init(Sha *s) {
    s->h[0]=0x6a09e667u; s->h[1]=0xbb67ae85u; s->h[2]=0x3c6ef372u; s->h[3]=0xa54ff53au;
    s->h[4]=0x510e527fu; s->h[5]=0x9b05688cu; s->h[6]=0x1f83d9abu; s->h[7]=0x5be0cd19u;
    s->used=0; s->total=0;
}

static void sha_update(Sha *s, const unsigned char *data, size_t n) {
    while (n) {
        size_t k = 64 - s->used;
        if (k > n) k = n;
        memcpy(s->buf + s->used, data, k);
        s->used += k; s->total += k; data += k; n -= k;
        if (s->used == 64) { sha_compress(s, s->buf); s->used = 0; }
    }
}

static void sha_final(Sha *s, unsigned char out[32]) {
    unsigned long long bits = s->total * 8;
    int i;
    s->buf[s->used++] = 0x80;
    if (s->used > 56) {
        memset(s->buf + s->used, 0, 64 - s->used);
        sha_compress(s, s->buf);
        s->used = 0;
    }
    memset(s->buf + s->used, 0, 56 - s->used);
    for (i = 0; i < 8; ++i) s->buf[56 + i] = (unsigned char)(bits >> (56 - 8*i));
    sha_compress(s, s->buf);
    for (i = 0; i < 8; ++i) {
        out[i*4]   = (unsigned char)(s->h[i] >> 24);
        out[i*4+1] = (unsigned char)(s->h[i] >> 16);
        out[i*4+2] = (unsigned char)(s->h[i] >> 8);
        out[i*4+3] = (unsigned char)(s->h[i]);
    }
}

/* str(Path(root).resolve(strict=False)) followed by os.path.normcase, Windows. */
static void resolve_normcase(const char *in, char *out, size_t outsz) {
    char comps[64][160];
    int nc = 0, i;
    size_t o = 0, t;
    char drive = 'C';
    const char *p = in ? in : "";
    char part[256];
    if (((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z')) && p[1] == ':') {
        drive = (char)toupper((unsigned char)*p);
        p += 2;
    }
    while (*p == '/' || *p == '\\') p++;
    while (*p) {
        t = 0;
        while (*p && *p != '/' && *p != '\\') { if (t < sizeof(part)-1) part[t++] = *p; p++; }
        part[t] = '\0';
        while (*p == '/' || *p == '\\') p++;
        if (t == 0 || !strcmp(part, ".")) continue;
        if (!strcmp(part, "..")) { if (nc > 0) nc--; continue; }
        if (nc < 64) { snprintf(comps[nc], sizeof(comps[nc]), "%s", part); nc++; }
    }
    if (outsz < 4) return;
    out[o++] = drive; out[o++] = ':'; out[o++] = '\\';
    for (i = 0; i < nc; ++i) {
        size_t len = strlen(comps[i]);
        if (o + len + 2 >= outsz) break;
        if (i) out[o++] = '\\';
        memcpy(out + o, comps[i], len); o += len;
    }
    out[o] = '\0';
    for (i = 0; i < (int)o; ++i) out[i] = (char)tolower((unsigned char)out[i]);
}

static char *checkout_key_str(const char *root) {
    static const char hexdig[] = "0123456789abcdef";
    char norm[2048];
    unsigned char digest[32];
    char *out;
    Sha s;
    int i;
    resolve_normcase(root, norm, sizeof(norm));
    sha_init(&s);
    sha_update(&s, (const unsigned char *)norm, strlen(norm));
    sha_final(&s, digest);
    out = (char *)malloc(25);
    if (!out) return NULL;
    for (i = 0; i < 12; ++i) {
        out[i*2] = hexdig[digest[i] >> 4];
        out[i*2+1] = hexdig[digest[i] & 15];
    }
    out[24] = '\0';
    return out;
}

json_t *w17_desktopbui_731b67_checkout_key(const char *project_root) {
    char *key = checkout_key_str(project_root);
    json_t *out;
    if (!key) return json_null();
    out = json_string(key);
    free(key);
    return out;
}

json_t *w17_desktopbui_731b67_lock_name(const char *project_root) {
    char *key = checkout_key_str(project_root);
    char name[80];
    json_t *out;
    if (!key) return json_null();
    snprintf(name, sizeof(name), "desktop-build-%s.lock", key);
    free(key);
    out = json_string(name);
    return out;
}
