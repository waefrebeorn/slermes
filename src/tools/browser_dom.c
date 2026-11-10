/*
 * browser_dom.c - browser DOM helpers.
 *
 * Split from browser.c (soul.md: self-contained modules). HTML-to-text
 * and title extraction. No file-scope state.
 */
#include "browser_dom.h"
#include "base64.h"
#include <websocket.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>



/* ================================================================
 *  HTML Helpers
 * ================================================================ */

/* Extract <title> from HTML. Returns empty string if not found. */
/* PoP: extract_title @ tools/browser_tool.py:_extract_title */ void extract_title(const char *html, char *title, size_t title_sz) {
    title[0] = '\0';
    if (!html) return;
    const char *t = strstr(html, "<title");
    if (!t) return;
    /* Find > after <title */
    t = strchr(t, '>');
    if (!t) return;
    t++;
    /* Find </title> */
    const char *end = strstr(t, "</title>");
    if (!end) return;
    size_t len = (size_t)(end - t);
    if (len > title_sz - 1) len = title_sz - 1;
    memcpy(title, t, len);
    title[len] = '\0';
    /* Trim whitespace */
    char *s = title, *e = title + strlen(title);
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    while (e > s && (*(e-1) == ' ' || *(e-1) == '\t' || *(e-1) == '\n' || *(e-1) == '\r')) e--;
    if (s != title) memmove(title, s, (size_t)(e - s));
    title[e - s] = '\0';
}


/* Strip HTML tags for a plain-text snapshot. Returns malloc'd string. */
/* PoP: html_to_text @ tools/browser_tool.py:_html_to_text */ char *html_to_text(const char *html) {
    if (!html) return strdup("");
    /* Simple tag stripper. For production, use a proper HTML parser. */
    size_t cap = strlen(html) + 1;
    char *out = (char *)malloc(cap);
    if (!out) return strdup("");
    size_t o = 0;
    int in_tag = 0;
    int in_script = 0;
    int in_style = 0;
    for (const char *p = html; *p && o < cap - 1; p++) {
        if (*p == '<') {
            in_tag = 1;
            /* Check for script/style */
            if (strncasecmp(p+1, "script", 6) == 0 &&
                (p[7] == '>' || p[7] == ' '))
                in_script = 1;
            if (strncasecmp(p+1, "style", 5) == 0 &&
                (p[6] == '>' || p[6] == ' '))
                in_style = 1;
            continue;
        }
        if (*p == '>') {
            in_tag = 0;
            if (in_script && p[-1] == '/' && p[-2] == 's') {
                /* self-closing <script /> */
                in_script = 0;
            }
            /* Check for closing tags */
            if (in_script && p > html + 8 &&
                strncasecmp(p-8, "</script", 8) == 0)
                in_script = 0;
            if (in_style && p > html + 6 &&
                strncasecmp(p-6, "</style", 6) == 0)
                in_style = 0;
            continue;
        }
        if (in_tag || in_script || in_style) continue;
        /* Replace whitespace runs with single space */
        if (isspace((unsigned char)*p)) {
            if (o > 0 && !isspace((unsigned char)out[o-1]))
                out[o++] = ' ';
        } else {
            out[o++] = *p;
        }
    }
    out[o] = '\0';
    /* Trim */
    while (o > 0 && (out[o-1] == ' ' || out[o-1] == '\n')) out[--o] = '\0';
    return out;
}
