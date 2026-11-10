/*
 * port_media_labels.c — pure helpers (see header).
 */

#include "port_media_labels.h"
#include "port_aux_outcome_event.h"
#include "../lib/libcrypto/crypto.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* ---- _content_disposition_filename ---- */

static char *ml_strndup(const char *s, size_t n)
{
    char *o = malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s, n);
    o[n] = '\0';
    return o;
}

static int ml_hexval(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* RFC 2231 extended value decode: percent-decode, then interpret the octets
 * as `charset` and re-emit as UTF-8. iso-8859-1/latin-1 bytes (<0x100) expand
 * to 2-byte UTF-8; utf-8/ascii pass through. Unknown charset -> latin-1
 * interpretation (mirrors collapse_rfc2231_value's LookupError fallback).
 * Returns malloc'd UTF-8, or "" when the encoded string was empty. */
static char *ml_decode_extended(const char *charset, const char *enc, size_t n)
{
    char *bytes = malloc(n + 1);
    size_t bn = 0, i;
    if (!bytes) return NULL;
    for (i = 0; i < n && enc[i] != '%'; i++)
        ;
    bytes[bn++] = '\0';
    /* percent-decode into bytes[] */
    {
        size_t w = 0;
        for (i = 0; i < n; i++) {
            if (enc[i] == '%' && i + 2 < n &&
                ml_hexval((unsigned char)enc[i + 1]) >= 0 &&
                ml_hexval((unsigned char)enc[i + 2]) >= 0) {
                bytes[w++] = (char)(ml_hexval((unsigned char)enc[i + 1]) * 16 +
                                    ml_hexval((unsigned char)enc[i + 2]));
                i += 2;
            } else {
                bytes[w++] = enc[i];
            }
        }
        bytes[w] = '\0';
        bn = w;
    }
    if (bn == 0) { free(bytes); return ml_strndup("", 0); }
    /* charset detection (case-insensitive) */
    {
        size_t cl = strlen(charset);
        int unicode = 0; /* iso-8859-1/latin-1 -> each byte is a codepoint */
        int latin = 0;
        if (cl == 9 && strncasecmp(charset, "iso-8859-", 9) == 0) latin = 1;
        else if (cl == 6 && strcasecmp(charset, "latin1") == 0) latin = 1;
        else if (cl == 5 && strcasecmp(charset, "latin-1") == 0) latin = 1;
        unicode = latin;
        if (!unicode) {
            /* utf-8 / utf8 / us-ascii / ascii / iso-8859-1 handled above;
             * anything else (incl. unknown) treats bytes as latin-1 like the
             * stdlib fallback. */
            if (cl >= 5 && strncasecmp(charset, "utf-", 4) == 0) unicode = 0;
            else if (cl >= 5 && strncasecmp(charset, "ascii", 5) == 0) unicode = 0;
            else if (cl == 4 && strcasecmp(charset, "utf8") == 0) unicode = 0;
            else latin = 1;
        }
        if (latin) {
            /* each byte -> codepoint, expanded to UTF-8 */
            unsigned char *b = (unsigned char *)bytes;
            char *out = malloc(bn * 2 + 1);
            size_t w = 0;
            if (!out) { free(bytes); return NULL; }
            for (i = 0; i < bn; i++) {
                if (b[i] < 0x80) out[w++] = (char)b[i];
                else {
                    out[w++] = (char)(0xC0 | (b[i] >> 6));
                    out[w++] = (char)(0x80 | (b[i] & 0x3F));
                }
            }
            out[w] = '\0';
            free(bytes);
            return out;
        }
        /* utf-8/ascii: bytes are the string already */
        return bytes;
    }
}

/* email.utils.unquote: strip a surrounding double-quote pair (unescaping
 * \\ and \") or <...>; else return unchanged. Returns malloc'd. */
static char *ml_unquote(const char *v, size_t n)
{
    if (n > 1 && v[0] == '"' && v[n - 1] == '"') {
        char *out = malloc(n + 1);
        size_t w = 0, i;
        if (!out) return NULL;
        for (i = 1; i + 1 < n; i++) {
            if (v[i] == '\\' && i + 2 < n && (v[i + 1] == '\\' || v[i + 1] == '"')) {
                out[w++] = v[i + 1];
                i++;
            } else {
                out[w++] = v[i];
            }
        }
        out[w] = '\0';
        return out;
    }
    if (n > 1 && v[0] == '<' && v[n - 1] == '>')
        return ml_strndup(v + 1, n - 2);
    return ml_strndup(v, n);
}

/* PoP: _content_disposition_filename @ gateway/relay/media.py:_content_disposition_filename */
char *ml_content_disposition_filename(const char *header)
{
    const char *s = header ? header : "";
    size_t len = strlen(s);
    char *plain = NULL, *extended = NULL;
    size_t pos = 0;

    /* _parseparam: split on ';' scanning quote balance so a ';' inside a
     * quoted value is not a separator. The first segment (disposition type,
     * e.g. "attachment") is params[0] and is skipped. */
    {
        int first = 1;
        while (pos < len) {
            size_t start, end;
            while (pos < len && s[pos] == ';') pos++;
            start = pos;
            if (start >= len) break;
            {
                long quota = 0;
                size_t k = pos;
                while (k < len) {
                    if (s[k] == ';' && quota % 2 == 0) break;
                    if (s[k] == '"' && (k == pos || s[k - 1] != '\\')) quota++;
                    k++;
                }
                end = k;
            }
            {
                /* segment [start,end) ; find first '=' */
                const char *name, *val;
                size_t nl, vl;
                size_t eq = start;
                while (eq < end && s[eq] != '=') eq++;
                if (eq < end) {
                    name = s + start;
                    nl = eq - start;
                    val = s + eq + 1;
                    vl = end - eq - 1;
                    while (nl && isspace((unsigned char)name[0])) { name++; nl--; }
                    while (nl && isspace((unsigned char)name[nl - 1])) nl--;
                    while (vl && isspace((unsigned char)val[0])) { val++; vl--; }
                    while (vl && isspace((unsigned char)val[vl - 1])) vl--;
                } else {
                    name = NULL; val = NULL; nl = vl = 0;
                }
                if (!first && nl > 0) {
                    /* lowercased key, strip trailing '*' */
                    size_t kl = nl;
                    char *key = NULL;
                    int is_ext = (kl > 0 && name[kl - 1] == '*');
                    if (is_ext) kl--;
                    key = malloc(kl + 1);
                    if (key) {
                        size_t i;
                        for (i = 0; i < kl; i++)
                            key[i] = (char)tolower((unsigned char)name[i]);
                        key[kl] = '\0';
                        if (strcmp(key, "filename") == 0 && val) {
                            if (is_ext) {
                                /* charset'lang'encoded ; unquote first */
                                char *uq = ml_unquote(val, vl);
                                if (uq) {
                                    const char *pl = strchr(uq, '\'');
                                    if (pl) {
                                        const char *pl2 = strchr(pl + 1, '\'');
                                        if (pl2 && pl2 + 1 <= uq + strlen(uq)) {
                                            size_t cs_len = (size_t)(pl - uq);
                                            const char *enc = pl2 + 1;
                                            char *cs = ml_strndup(uq, cs_len);
                                            char *dec = cs ?
                                                ml_decode_extended(cs, enc, strlen(enc)) : NULL;
                                            if (!extended && dec && dec[0] != '\0')
                                                extended = dec;
                                            else free(dec);
                                            free(cs);
                                        }
                                    }
                                    free(uq);
                                }
                            } else {
                                char *u = ml_unquote(val, vl);
                                if (!plain && u && u[0] != '\0') plain = u;
                                else free(u);
                            }
                        }
                        free(key);
                    }
                }
                first = 0;
            }
            pos = end;
        }
    }

    {
        char *chosen = (extended && extended[0] != '\0') ? extended : plain;
        char *result = chosen ? eve_clean_attachment_filename(chosen) : ml_strndup("", 0);
        free(extended);
        free(plain);
        return result;
    }
}

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
