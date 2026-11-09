/*
 * patch_parser_util.c - patch-parser marker / emit helpers.
 *
 * Split from port_patch_parser.c (soul.md: self-contained modules).
 * File/move marker matching, JSON emission and small string helpers.
 * No file-scope state.
 */
#include "patch_parser_util.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "cron/cron_jobs_paths.h"   /* canonical xstrdup */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>



/* ---------- small helpers ---------- */ char *xstrndup(const char *s, size_t n) {
    char *p = malloc(n + 1);
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}


/* Trim leading/trailing ASCII whitespace (in place, returns start). */
static char *trim(char *s) {
    if (!s) return s;
    while (*s == ' ' || *s == '\t') s++;
    char *e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n'))
        *--e = '\0';
    return s;
}


/* PCRE-style simple regex helpers (no full regex needed for markers). */

/* Match "*** <OP> File: <rest>" -> returns malloc'd rest (trimmed) or NULL.
 * opkw is e.g. "Update", "Add", "Delete". Spaces around keywords tolerated. */ char *match_file_marker(const char *line, const char *opkw) {
    /* line must start with "***" */
    if (strncmp(line, "***", 3) != 0) return NULL;
    const char *p = line + 3;
    while (*p == ' ') p++;
    size_t kwl = strlen(opkw);
    if (strncmp(p, opkw, kwl) != 0) return NULL;
    p += kwl;
    while (*p == ' ') p++;
    if (strncmp(p, "File:", 5) != 0) return NULL;
    p += 5;
    while (*p == ' ') p++;
    char *rest = xstrdup(p);
    return xstrdup(trim(rest)); /* trimmed copy */
}


/* Match "*** Move File: <src> -> <dst>". Returns src,dst malloc'd or NULL. */ bool match_move_marker(const char *line, char **src, char **dst) {
    if (strncmp(line, "***", 3) != 0) return false;
    const char *p = line + 3;
    while (*p == ' ') p++;
    if (strncmp(p, "Move", 4) != 0) return false;
    p += 4;
    while (*p == ' ') p++;
    if (strncmp(p, "File:", 5) != 0) return false;
    p += 5;
    while (*p == ' ') p++;
    /* src = up to " -> " */
    const char *arrow = strstr(p, "->");
    if (!arrow) return false;
    size_t srclen = (size_t)(arrow - p);
    /* trim trailing ws before arrow */
    while (srclen > 0 && (p[srclen-1] == ' ' || p[srclen-1] == '\t')) srclen--;
    *src = xstrndup(p, srclen);
    const char *d = arrow + 2;
    while (*d == ' ' || *d == '\t') d++;
    char *d2 = xstrdup(d);
    *dst = xstrdup(trim(d2));
    free(d2);
    return true;
}


/* JSON string escaper for canonical output. */ void emit_json_str(const char *s)
{
    putchar('"');
    for (const char *p = s; p && *p; p++) {
        unsigned char c = (unsigned char)*p;
        switch (c) {
            case '"':  fputs("\\\"", stdout); break;
            case '\\': fputs("\\\\", stdout); break;
            case '\n': fputs("\\n", stdout); break;
            case '\r': fputs("\\r", stdout); break;
            case '\t': fputs("\\t", stdout); break;
            default:
                if (c < 0x20) fprintf(stdout, "\\u%04x", c);
                else putchar((int)c);
        }
    }
    putchar('"');
}
