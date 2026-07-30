/*
 * web_base64_img.c — Port of Python: tools/web_tools.py:convert_base64_images_to_links
 *
 * Replaces inline base64 image blobs with labeled markdown placeholders so the
 * model never receives tens of thousands of base64 characters inline. Real
 * (http/https) markdown image links are left untouched.
 *
 * Transformations (1:1 with Python):
 *   ![alt](data:image/png;base64,AAAA...) -> [IMAGE: alt]   (or [IMAGE] if alt empty)
 *   (data:image/png;base64,AAAA...)        -> [IMAGE]
 *   bare data:image/...;base64,AAAA...         -> [IMAGE]
 *
 * POSIX ERE only (no PCRE). Uses capturing groups (glibc rejects (?:...)),
 * [[:space:]] for whitespace, and a generic re.sub-style engine.
 */

#ifndef SRC_TOOLS_WEB_BASE64_IMG_C
#define SRC_TOOLS_WEB_BASE64_IMG_C

#include "web_base64_img.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <regex.h>

/* ---- generic re.sub-style engine -------------------------------------------
 * For each non-overlapping match of `re` in `text`, copy the gap text
 * through, then call `repl` to append the replacement (which may use the
 * captured group 1, passed as offsets relative to the whole match). */

typedef void (*b64_repl_t)(const char *match, const regmatch_t *g1,
                            char **out, size_t *op, size_t *cap);

/* PoP: ensure @ tools/lazy_deps.py:ensure */
static int b64_ensure(char **out, size_t *cap, size_t need)
{
    if (*cap >= need) return 0;
    size_t ncap = (*cap == 0) ? 512 : *cap * 2;
    while (ncap < need) ncap *= 2;
    char *n = realloc(*out, ncap);
    if (!n) return -1;
    *out = n;
    *cap = ncap;
    return 0;
}

static char *b64_run(const regex_t *re, const char *text, b64_repl_t repl)
{
    size_t cap = 0, op = 0;
    char *out = NULL;
    const char *p = text;
    regmatch_t m[2];

    while (*p) {
        m[0].rm_so = m[0].rm_eo = -1;
        m[1].rm_so = m[1].rm_eo = -1;
        if (regexec(re, p, 2, m, 0) != 0) break; /* REG_NOMATCH */

        /* copy gap text before the match */
        regoff_t pre = m[0].rm_so;
        if (pre > 0) {
            if (b64_ensure(&out, &cap, op + (size_t)pre + 1) != 0) { free(out); return NULL; }
            memcpy(out + op, p, (size_t)pre);
            op += (size_t)pre;
        }
        /* replacement */
        const regmatch_t *g1 = (m[1].rm_so >= 0) ? &m[1] : NULL;
        /* g1 offsets are relative to `p` (the string passed to regexec),
         * so pass `p` as the match base, not `p + m[0].rm_so`. */
        repl(p, g1, &out, &op, &cap);
        p += m[0].rm_eo;
    }
    /* trailing text */
    if (b64_ensure(&out, &cap, op + strlen(p) + 1) != 0) { free(out); return NULL; }
    memcpy(out + op, p, strlen(p) + 1);
    return out;
}

/* ---- pass 1: markdown image with base64 source ---------------------------- */
static void b64_repl_md(const char *match, const regmatch_t *g1,
                         char **out, size_t *op, size_t *cap)
{
    const char *alt = "";
    size_t alen = 0;
    if (g1) {
        alt = match + g1->rm_so;
        alen = (size_t)(g1->rm_eo - g1->rm_so);
        /* strip leading/trailing whitespace (Python .strip()) */
        while (alen > 0 && isspace((unsigned char)alt[0])) { alt++; alen--; }
        while (alen > 0 && isspace((unsigned char)alt[alen - 1])) alen--;
    }
    if (alen == 0) {
        const char tag[] = "[IMAGE]";
        if (b64_ensure(out, cap, *op + sizeof(tag)) != 0) return;
        memcpy(*out + *op, tag, sizeof(tag) - 1);
        *op += sizeof(tag) - 1;
    } else {
        const char pre[] = "[IMAGE: ";
        const char post[] = "]";
        if (b64_ensure(out, cap, *op + sizeof(pre) + alen + sizeof(post)) != 0) return;
        memcpy(*out + *op, pre, sizeof(pre) - 1); *op += sizeof(pre) - 1;
        memcpy(*out + *op, alt, alen); *op += alen;
        memcpy(*out + *op, post, sizeof(post) - 1); *op += sizeof(post) - 1;
    }
}

/* ---- passes 2 & 3: fixed [IMAGE] replacement ---------------------------- */
static void b64_repl_fixed(const char *match, const regmatch_t *g1,
                            char **out, size_t *op, size_t *cap)
{
    (void)match; (void)g1;
    const char tag[] = "[IMAGE]";
    if (b64_ensure(out, cap, *op + sizeof(tag)) != 0) return;
    memcpy(*out + *op, tag, sizeof(tag) - 1);
    *op += sizeof(tag) - 1;
}

char *web_base64_img_convert(const char *text)
{
    if (!text) return strdup("");

    /* 1. Markdown image with base64 source -> keep alt text. */
    regex_t re_md;
    /* !\[ ([^]]*) \] \( [[:space:]]* data:image/ [^;]+ ;base64, [A-Za-z0-9+/=[:space:]]+ \) */
    const char *pat_md =
        "!\\[([^]]*)\\]\\([[:space:]]*data:image/[^;]+;base64,[A-Za-z0-9+/=[:space:]]+\\)";
    if (regcomp(&re_md, pat_md, REG_EXTENDED) != 0) return strdup(text);

    char *s1 = b64_run(&re_md, text, b64_repl_md);
    regfree(&re_md);
    if (!s1) return NULL;

    /* 2. Parenthesized base64 (non-markdown) -> [IMAGE]. */
    regex_t re_par;
    const char *pat_par =
        "\\([[:space:]]*data:image/[^;]+;base64,[A-Za-z0-9+/=[:space:]]+\\)";
    if (regcomp(&re_par, pat_par, REG_EXTENDED) != 0) { free(s1); return NULL; }

    char *s2 = b64_run(&re_par, s1, b64_repl_fixed);
    regfree(&re_par);
    free(s1);
    if (!s2) return NULL;

    /* 3. Bare base64 (no parentheses) -> [IMAGE]. */
    regex_t re_bare;
    const char *pat_bare = "data:image/[^;]+;base64,[A-Za-z0-9+/=]+";
    if (regcomp(&re_bare, pat_bare, REG_EXTENDED) != 0) { free(s2); return NULL; }

    char *s3 = b64_run(&re_bare, s2, b64_repl_fixed);
    regfree(&re_bare);
    free(s2);
    return s3; /* may be NULL on alloc failure */
}

#endif /* SRC_TOOLS_WEB_BASE64_IMG_C */
