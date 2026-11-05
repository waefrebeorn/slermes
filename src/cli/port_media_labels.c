/*
 * port_media_labels.c — pure helpers (see header).
 */

#include "port_media_labels.h"
#include "../lib/libcrypto/crypto.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

char *ml_media_base_url(const char *relay_dial_url)
{
    const char *s = relay_dial_url ? relay_dial_url : "";
    size_t len = strlen(s);
    char *tmp, *out;
    size_t olen;
    while (len > 0 && isspace((unsigned char)*s)) { s++; len--; }
    while (len > 0 && (isspace((unsigned char)s[len - 1]) || s[len - 1] == '/'))
        len--;
    tmp = malloc(len + 4);
    if (!tmp) return NULL;
    memcpy(tmp, s, len);
    /* ws:// -> http:// (len+5), wss:// -> https:// (len+6) */
    if (len >= 5 && memcmp(tmp, "ws://", 5) == 0) {
        out = malloc(len + 4);
        if (!out) { free(tmp); return NULL; }
        memcpy(out, "http://", 7);
        memcpy(out + 7, tmp + 5, len - 5);
        olen = len + 2;
    } else if (len >= 6 && memcmp(tmp, "wss://", 6) == 0) {
        out = malloc(len + 4);
        if (!out) { free(tmp); return NULL; }
        memcpy(out, "https://", 8);
        memcpy(out + 8, tmp + 6, len - 6);
        olen = len + 2;
    } else {
        out = tmp;
        olen = len;
        tmp = NULL;
    }
    /* removesuffix("/relay") */
    if (olen >= 6 && memcmp(out + olen - 6, "/relay", 6) == 0)
        olen -= 6;
    out[olen] = '\0';
    free(tmp);
    return out;
}

int ml_is_relay_media_url(const char *url)
{
    return url && strstr(url, "/relay/media/") != NULL;
}

char *ml_plugin_label(const char *emoji, const char *label)
{
    const char *e = emoji ? emoji : "";
    const char *l = label ? label : "";
    if (e[0] == '\0')
        return strdup(l);
    {
        size_t el = strlen(e), ll = strlen(l);
        char *o = malloc(el + 2 + ll + 1);
        if (!o) return NULL;
        memcpy(o, e, el);
        o[el] = ' '; o[el + 1] = ' ';
        memcpy(o + el + 2, l, ll);
        o[el + 2 + ll] = '\0';
        return o;
    }
}

char *ml_generation_of(const char *credential)
{
    unsigned char digest[CRYPTO_SHA256_LEN];
    static const char *const H = "0123456789abcdef";
    char *out = malloc(2 * CRYPTO_SHA256_LEN + 1);
    int k;
    const char *c = credential ? credential : "";
    if (!out) return NULL;
    crypto_sha256((const unsigned char *)c, strlen(c), digest);
    for (k = 0; k < CRYPTO_SHA256_LEN; k++) {
        out[2 * k] = H[(digest[k] >> 4) & 0xf];
        out[2 * k + 1] = H[digest[k] & 0xf];
    }
    out[2 * CRYPTO_SHA256_LEN] = '\0';
    return out;
}
