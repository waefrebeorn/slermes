/*
 * port_plugin_messages.c — pure helpers (see header).
 */

#include "port_plugin_messages.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *pm_json_string(const char *s)
{
    size_t len = strlen(s), i, j = 0;
    char *o = malloc(len * 2 + 3);
    if (!o) return NULL;
    o[j++] = '"';
    for (i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '"' || c == '\\') { o[j++] = '\\'; o[j++] = (char)c; }
        else if (c < 0x20) j += (size_t)sprintf(o + j, "\\u%04x", c);
        else o[j++] = (char)c;
    }
    o[j++] = '"';
    o[j] = '\0';
    return o;
}

char *plm_err(const char *code, const char *detail)
{
    const char *c = code ? code : "";
    const char *d = detail ? detail : "";
    char sc[256], sd[1024], *q;
    q = pm_json_string(c);
    snprintf(sc, sizeof sc, "%s", q ? q : "\"\"");
    free(q);
    if (d[0]) {
        q = pm_json_string(d);
        snprintf(sd, sizeof sd, ",\"detail\":%s", q ? q : "\"\"");
        free(q);
    } else {
        sd[0] = '\0';
    }
    {
        char buf[2048];
        snprintf(buf, sizeof buf, "{\"ok\":false,\"error\":%s%s}", sc, sd);
        return strdup(buf);
    }
}

/* ---- _fit (len-measured clamp with ellipsis) ---- */

static size_t pm_chars(const char *s)
{
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        if ((*p & 0xC0) != 0x80) n++;
    return n;
}

static char *pm_drop_chars(const char *s, int count)
{
    /* remove the last `count` characters (UTF-8 char boundaries) */
    size_t len = strlen(s);
    int dropped = 0;
    if (len == 0 || count <= 0) return strdup(s);
    /* walk from the end, counting char boundaries (non-continuation bytes) */
    for (size_t i = len; i > 0 && dropped < count; i--) {
        if (((unsigned char)s[i - 1] & 0xC0) != 0x80) dropped++;
    }
    /* i is the length of what remains */
    {
        size_t remain = 0;
        int d = 0;
        for (size_t i = len; i > 0; i--) {
            if (((unsigned char)s[i - 1] & 0xC0) != 0x80) d++;
            if (d == count) { remain = i - 1; break; }
            if (i == 1) { remain = 0; break; }
        }
        {
            char *o = malloc(remain + 1);
            if (!o) return NULL;
            memcpy(o, s, remain);
            o[remain] = '\0';
            return o;
        }
    }
    return strdup("");
}

static char *pm_append_ellipsis(const char *s)
{
    /* append U+2026 (…  = E2 80 A6) */
    size_t len = strlen(s);
    char *o = malloc(len + 4);
    if (!o) return NULL;
    memcpy(o, s, len);
    o[len] = (char)0xE2; o[len + 1] = (char)0x80; o[len + 2] = (char)0xA6;
    o[len + 3] = '\0';
    return o;
}

char *plm_fit(const char *text, int limit)
{
    const char *t = text ? text : "";
    char *cut;
    if (limit <= 0 || (int)pm_chars(t) <= limit)
        return strdup(t);
    cut = strdup(t);
    {
        int guard = 0;
        while ((int)pm_chars(cut) > 0 && (int)pm_chars(cut) + 1 > limit && guard < 10000) {
            int drop = (int)pm_chars(cut) + 1 - limit;
            if (drop < 1) drop = 1;
            char *nxt = pm_drop_chars(cut, drop);
            free(cut);
            cut = nxt;
            guard++;
        }
    }
    {
        char *r = pm_append_ellipsis(cut);
        free(cut);
        return r;
    }
}

int plm_supports_edit(int editing_flag, int has_custom_edit)
{
    if (!editing_flag) return 0;
    return has_custom_edit ? 1 : 0;
}
