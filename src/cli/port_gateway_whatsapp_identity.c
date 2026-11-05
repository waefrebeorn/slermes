/*
 * port_gateway_whatsapp_identity.c — pure JID helpers of gateway/whatsapp_identity.py.
 */

#include "port_gateway_whatsapp_identity.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *dupn(const char *s, size_t n)
{
    char *o = (char *)malloc(n + 1);
    if (!o) return NULL;
    memcpy(o, s ? s : "", n);
    o[n] = '\0';
    return o;
}

static int is_dig(char c) { return c >= '0' && c <= '9'; }

/* PoP: wi_normalize_identifier @ gateway/whatsapp_identity.py:normalize_whatsapp_identifier */
char *wi_normalize_identifier(const char *value)
{
    /* str(value or "").strip().replace("+","",1).split(":",1)[0].split("@",1)[0] */
    const char *raw = value ? value : "";
    size_t len = strlen(raw), s = 0;
    while (s < len && isspace((unsigned char)raw[s])) s++;
    size_t e = len;
    while (e > s && isspace((unsigned char)raw[e - 1])) e--;
    /* remove the FIRST '+' anywhere in [s,e) */
    size_t plus = (size_t)-1;
    for (size_t i = s; i < e; i++)
        if (raw[i] == '+') { plus = i; break; }
    size_t b = s;
    while (b < e) {
        if (b == plus) { b++; continue; }
        if (raw[b] == ':' || raw[b] == '@') break;
        b++;
    }
    /* build [s,b) skipping `plus` */
    size_t outn = (plus != (size_t)-1 && plus < b) ? b - s - 1 : b - s;
    char *out = (char *)malloc(outn + 1);
    if (!out) return NULL;
    size_t o = 0;
    for (size_t i = s; i < b; i++) {
        if (i == plus) continue;
        out[o++] = raw[i];
    }
    out[o] = '\0';
    return out;
}

/* BARE_PHONE_RE: ^\+?[\d\s().\-]+$ — at least one char, starting '+' optional,
 * then only digits/space/( ) . -; must contain the pattern to end. Python
 * fullmatch requires the ENTIRE string to match [\d\s().\-]+. */
static int bare_phone(const char *s)
{
    size_t i = 0, len = strlen(s);
    if (!len) return 0;
    if (s[0] == '+') i++;
    if (i == len) return 0; /* just "+" -> [\d...]+ needs >=1 */
    for (; i < len; i++) {
        char c = s[i];
        if (is_dig(c) || c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
            c == '(' || c == ')' || c == '.' || c == '-')
            continue;
        return 0;
    }
    return 1;
}

/* PoP: wi_to_jid @ gateway/whatsapp_identity.py:to_whatsapp_jid */
char *wi_to_jid(const char *value)
{
    if (!value || !value[0])
        return dupn("", 0);
    size_t len = strlen(value), s = 0, e = len;
    while (s < e && isspace((unsigned char)value[s])) s++;
    while (e > s && isspace((unsigned char)value[e - 1])) e--;
    char *cur = dupn(value + s, e - s + 1);
    if (!cur) return NULL;
    cur[e - s] = '\0';
    size_t clen = e - s;

    char *colon = memchr(cur, ':', clen);
    char *at = memchr(cur, '@', clen);
    if (colon && at && colon < at) {
        /* user:device@domain -> user@domain */
        size_t u = (size_t)(colon - cur);
        char *out = (char *)malloc(clen + 2);
        if (!out) { free(cur); return NULL; }
        memcpy(out, cur, u);
        out[u] = '@';
        strcpy(out + u + 1, at + 1);
        free(cur);
        return out;
    }
    if (at) {
        return cur;
    }
    if (bare_phone(cur)) {
        char digits[80];
        size_t dn = 0;
        for (size_t i = 0; i < clen && dn < sizeof(digits) - 1; i++)
            if (is_dig(cur[i])) digits[dn++] = cur[i];
        digits[dn] = '\0';
        if (dn) {
            char *out = (char *)malloc(dn + 16); /* + "@s.whatsapp.net" (15) + NUL */
            if (!out) { free(cur); return NULL; }
            snprintf(out, dn + 16, "%s@s.whatsapp.net", digits);
            free(cur);
            return out;
        }
    }
    return cur;
}

/* PoP: wi_normalize_mention_jid @ gateway/whatsapp_identity.py:normalize_whatsapp_mention_jid */
char *wi_normalize_mention_jid(const char *value)
{
    char *jid = wi_to_jid(value);
    if (!jid) return NULL;
    char *at = strchr(jid, '@');
    if (!at) {
        free(jid);
        return dupn("", 0);
    }
    size_t ulen = (size_t)(at - jid);
    const char *domain = at + 1;
    int ok = ulen > 0;
    for (size_t i = 0; i < ulen; i++)
        if (!is_dig(jid[i])) { ok = 0; break; }
    int dom_ok = (strcmp(domain, "s.whatsapp.net") == 0 || strcmp(domain, "lid") == 0);
    if (ok && dom_ok)
        return jid;
    free(jid);
    return dupn("", 0);
}
