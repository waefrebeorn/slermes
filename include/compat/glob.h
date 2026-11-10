/* Slermes Windows compat: <glob.h> — directory scan + fnmatch, no shell. */
#ifndef SLERMES_COMPAT_GLOB_H
#define SLERMES_COMPAT_GLOB_H
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <fnmatch.h>

#ifndef GLOB_NOSPACE
#define GLOB_NOSPACE  1
#define GLOB_ABORTED  2
#define GLOB_NOMATCH  3
#define GLOB_ERR      (1 << 0)
#define GLOB_MARK     (1 << 1)
#define GLOB_NOSORT   (1 << 2)
#define GLOB_NOCHECK  (1 << 4)
#define GLOB_APPEND   (1 << 5)
#endif

typedef struct {
    size_t gl_pathc;
    char **gl_pathv;
    size_t gl_offs;
} glob_t;

/* Only the final path component is matched; callers here pass "<dir>/<pattern>". */
static inline int glob(const char *pattern, int flags, void *errfunc, glob_t *pglob)
{
    const char *slash;
    char dir[2048], pat[512];
    DIR *d;
    struct dirent *e;
    size_t cap = 16;
    (void)errfunc;

    if (!pglob) return GLOB_NOSPACE;
    if (!(flags & GLOB_APPEND)) { pglob->gl_pathc = 0; pglob->gl_pathv = NULL; }

    slash = strrchr(pattern, '/');
    if (slash) {
        size_t n = (size_t)(slash - pattern);
        if (n >= sizeof dir) return GLOB_NOSPACE;
        memcpy(dir, pattern, n); dir[n] = '\0';
        if (n == 0) strcpy(dir, ".");
        snprintf(pat, sizeof pat, "%s", slash + 1);
    } else {
        strcpy(dir, ".");
        snprintf(pat, sizeof pat, "%s", pattern);
    }

    d = opendir(dir);
    if (!d) {
        if (flags & GLOB_NOCHECK) {
            pglob->gl_pathv = (char **)malloc(2 * sizeof(char *));
            if (!pglob->gl_pathv) return GLOB_NOSPACE;
            pglob->gl_pathv[0] = _strdup(pattern);
            pglob->gl_pathv[1] = NULL;
            pglob->gl_pathc = 1;
            return 0;
        }
        return GLOB_NOMATCH;
    }

    pglob->gl_pathv = (char **)malloc(cap * sizeof(char *));
    if (!pglob->gl_pathv) { closedir(d); return GLOB_NOSPACE; }

    while ((e = readdir(d)) != NULL) {
        char full[2048];
        if (fnmatch(pat, e->d_name, 0) != 0) continue;
        snprintf(full, sizeof full, "%s/%s", dir, e->d_name);
        if (pglob->gl_pathc + 2 > cap) {
            char **np;
            cap *= 2;
            np = (char **)realloc(pglob->gl_pathv, cap * sizeof(char *));
            if (!np) { closedir(d); return GLOB_NOSPACE; }
            pglob->gl_pathv = np;
        }
        pglob->gl_pathv[pglob->gl_pathc++] = _strdup(full);
    }
    closedir(d);
    pglob->gl_pathv[pglob->gl_pathc] = NULL;
    if (pglob->gl_pathc == 0) {
        free(pglob->gl_pathv);
        pglob->gl_pathv = NULL;
        return GLOB_NOMATCH;
    }
    return 0;
}

static inline void globfree(glob_t *pglob)
{
    size_t i;
    if (!pglob || !pglob->gl_pathv) return;
    for (i = 0; i < pglob->gl_pathc; i++) free(pglob->gl_pathv[i]);
    free(pglob->gl_pathv);
    pglob->gl_pathv = NULL;
    pglob->gl_pathc = 0;
}
#endif
