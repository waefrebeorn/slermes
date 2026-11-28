#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hermes_json.h"

/* PoP: agent_transports_codex_u_bounded_prompt_cache_key
 * @ agent/transports/codex.py:_bounded_prompt_cache_key */
/* None when the stripped key is empty; the key itself when len <= 64 (CODEPOINTS);
 * else "pck_" + sha256(key)[:24]. */
#include "crypto.h"

static void _hx2(const unsigned char *d, char *out, int n) {
    static const char *H = "0123456789abcdef";
    for (int i = 0; i < n; i++) { out[2*i] = H[(d[i] >> 4) & 15]; out[2*i+1] = H[d[i] & 15]; }
    out[2*n] = 0;
}
char *agent_transports_codex_u_bounded_prompt_cache_key(const char *arg) {
    const char *v = arg ? arg : "";
    size_t n = strlen(v), a = 0, b = n;
    while (a < b && (v[a]==' '||v[a]=='\t'||v[a]=='\n'||v[a]=='\r'||v[a]=='\f'||v[a]=='\v')) a++;
    while (b > a && (v[b-1]==' '||v[b-1]=='\t'||v[b-1]=='\n'||v[b-1]=='\r'||v[b-1]=='\f'||v[b-1]=='\v')) b--;
    if (b == a) return strdup("");
    char *key = malloc(b-a+1); memcpy(key, v+a, b-a); key[b-a] = 0;
    size_t cps = 0;
    for (size_t i = 0; i < b-a; i++) if (((unsigned char)key[i] & 0xC0) != 0x80) cps++;
    if (cps <= 64) return key;
    unsigned char d[32]; crypto_sha256((const unsigned char *)key, strlen(key), d);
    char hex[33]; _hx2(d, hex, 32);
    char *out = malloc(32);
    snprintf(out, 32, "pck_%.24s", hex);
    free(key);
    return out;
}
