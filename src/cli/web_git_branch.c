/*
 * web_git_branch.c - branch naming and diff-stat helpers.
 *
 * Split from port_web_git.c (soul.md: self-contained modules). Slugify /
 * sanitize / unique-dir naming plus the numstat walker. No file-scope state.
 */
#include "web_git_branch.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include "libpath/path.h"

#include "web_git_util.h"



/* PoP: walk_entries @ hermes_cli/web_git.py:_walk_entries */ void walk_entries(const char *raw, size_t raw_len, entry_cb cb, void *ud) {
    if (!raw) return;
    const char *p = raw;
    const char *end = raw + raw_len;
    while (p < end) {
        const char *nul = memchr(p, '\0', (size_t)(end - p));
        size_t reclen = nul ? (size_t)(nul - p) : (size_t)(end - p);
        char rec[8192];
        if (reclen >= sizeof(rec)) { p = nul ? nul + 1 : end; continue; }
        memcpy(rec, p, reclen);
        rec[reclen] = '\0';

        char tag = rec[0] ? rec[0] : '\0';
        if (tag == '?') {
            cb("?", "??", rec + 2, ud);
        } else if (tag == 'u') {
            /* "u <xy> <sub> <mH> <mI> <mW> <hH> <hI> <path>" */
            char *sp = strchr(rec + 1, ' ');
            char *xy = sp ? sp + 1 : (char *)"";
            char *path = xy ? strchr(xy, ' ') : NULL;
            if (path) { path++; cb("u", xy, path, ud); }
        } else if (tag == '1' || tag == '2') {
            /* "1 <xy> ..." -> path is the last space-delimited field. */
            char *xy = strchr(rec + 1, ' ');
            char *path = xy ? strrchr(xy, ' ') : NULL;
            if (path) { path++; cb(rec, xy + 1, path, ud); }
            if (tag == '2') {
                /* rename/copy origin path is the next NUL record. */
                if (nul) p = nul + 1;
                else break;
                continue;
            }
        }
        p = nul ? nul + 1 : end;
    }
}


/* Untracked file line count (newlines + final unterminated line). */
/* PoP: untracked_insertions @ hermes_cli/web_git.py:_untracked_insertions */ int untracked_insertions(const char *cwd, const char *rel) {
    char *target = path_join(cwd, rel);
    if (!target) return 0;
    struct stat st;
    if (stat(target, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size > UNTRACKED_LINE_MAX_BYTES) {
        free(target); return 0;
    }
    FILE *f = fopen(target, "rb");
    free(target);
    if (!f) return 0;
    int lines = 0; size_t r; unsigned char b;
    bool any = false, ends_nl = true;
    while ((r = fread(&b, 1, 1, f)) == 1) {
        any = true;
        if (b == '\n') lines++;
        if (b == '\0') { fclose(f); return 0; }
        ends_nl = (b == '\n');
    }
    fclose(f);
    if (!any) return 0;
    return ends_nl ? lines : lines + 1;
}


/* PoP: sanitize_branch @ hermes_cli/web_git.py:_sanitize_branch */ char *sanitize_branch(const char *name) {
    char *v = strdup(name ? name : "");
    /* collapse whitespace -> '-' */
    for (char *p = v; *p; p++) if (isspace((unsigned char)*p)) *p = '-';
    /* strip non [word./-] */
    char *w = v;
    for (char *p = v; *p; p++) if (isalnum((unsigned char)*p) || *p == '_' || *p == '.' || *p == '/') *w++ = *p;
    *w = '\0';
    /* collapse repeats */
    char *o = v;
    for (char *p = v; *p; p++) {
        if ((*p == '-' && *(o-1) == '-') || (*p == '/' && *(o-1) == '/') || (*p == '.' && *(o-1) == '.')) continue;
        *o++ = *p;
    }
    /* trim leading/trailing - . / */
    while (*o && (*(o-1) == '-' || *(o-1) == '.' || *(o-1) == '/')) { o--; *o = '\0'; }
    return v;
}
 char *slugify(const char *name) {
    char *s = strdup(name ? name : "");
    for (char *p = s; *p; p++) *p = (char)tolower((unsigned char)*p);
    char *o = s;
    for (char *p = s; *p; p++) if (isalnum((unsigned char)*p)) *o++ = *p; else if (*o == '\0' || *(o-1) != '-') *o++ = '-';
    *o = '\0';
    /* trim + cap 40 */
    while (*o && (*(o-1) == '-')) { o--; *o = '\0'; }
    size_t len = strlen(s);
    if (len > 40) { s[40] = '\0'; while (len > 0 && s[len-1] == '-') s[--len] = '\0'; }
    if (s[0] == '\0') { free(s); return strdup("work"); }
    return s;
}


/* PoP: unique_dir @ hermes_cli/web_git.py:_unique_dir */ char *unique_dir(const char *base) {
    char *cand = strdup(base);
    int n = 1;
    struct stat st;
    while (stat(cand, &st) == 0) {
        free(cand);
        size_t need = strlen(base) + 16;
        cand = malloc(need);
        snprintf(cand, need, "%s-%d", base, ++n);
    }
    return cand;
}
