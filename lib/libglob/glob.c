/*
 * glob.c — C recursive glob matching library (J12: Python glob port).
 *
 * Implements glob matching with ** for recursive directory traversal.
 */

#define _POSIX_C_SOURCE 200809L
#include "hermes_glob.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fnmatch.h>

/* ================================================================
 *  Pattern matching (supports **, *, ?)
 * ================================================================ */

static bool glob_match_components(const char *pattern, int nparts,
                                  const char **pend, const char *path,
                                  int sparts, const char **send);

bool glob_match(const char *pattern, const char *path) {
    if (!pattern || !path) return false;

    /* Handle ** — match across directory separators */
    const char *starstar = strstr(pattern, "**");
    if (starstar) {
        /* Pattern has **: split on **, verify prefix and suffix match */
        size_t prefix_len = (size_t)(starstar - pattern);
        const char *suffix = starstar + 2; /* skip ** */

        /* Check prefix */
        if (prefix_len > 0) {
            if (strncmp(pattern, path, prefix_len) != 0)
                return false;
        }

        /* Check suffix -- must match at some point in path after prefix */
        const char *path_remain = path + prefix_len;
        if (suffix[0] == '/') {
            /* ** slash: skip the slash */
            suffix++;
            /* ** can match zero or more dirs */
            /* Find suffix in remaining path */
            while (*path_remain) {
                if (fnmatch(suffix, path_remain, 0) == 0)
                    return true;
                /* Skip to next path component */
                path_remain = strchr(path_remain, '/');
                if (!path_remain) break;
                path_remain++;
            }
            return suffix[0] == '\0';
        } else if (suffix[0] == '\0') {
            /* ** at end: matches everything */
            return true;
        } else {
            /* ** followed by non-slash: match suffix anywhere in remaining path */
            while (*path_remain) {
                if (fnmatch(suffix, path_remain, 0) == 0)
                    return true;
                path_remain++;
            }
            return false;
        }
    }

    /* No **: match component-by-component with FNM_PERIOD so wildcard-led
     * components never match a leading dot (Python glob/fnmatch rule).
     * A literal pattern component (no wildcards) is compared with strcmp
     * so ".hidden" still matches itself. */
    {
        const char *p = pattern;
        /* fast path: no wildcards anywhere -> literal compare */
        if (!strpbrk(pattern, "*?[")) return strcmp(pattern, path) == 0;

        const char *pend[64], *send[64];
        int nparts = 0;
        pend[nparts] = p;
        for (const char *q = p; *q; q++) if (*q == '/') { nparts++; pend[nparts] = q + 1; }
        nparts++;  /* count of pattern components */
        int sparts = 0;
        send[sparts] = path;
        for (const char *q = path; *q; q++) if (*q == '/') { sparts++; send[sparts] = q + 1; }
        sparts++;
        if (nparts > 64 || sparts > 64) return fnmatch(pattern, path, 0) == 0;

        return glob_match_components(pattern, nparts, pend, path, sparts, send);
    }
}

/* Compare one pattern component against one path component. */
static bool match_one_component(const char *pc, size_t plen,
                                const char *sc, size_t slen) {
    char pbuf[GLOB_MAX_PATH], sbuf[GLOB_MAX_PATH];
    if (plen >= sizeof(pbuf) || slen >= sizeof(sbuf)) return false;
    memcpy(pbuf, pc, plen); pbuf[plen] = '\0';
    memcpy(sbuf, sc, slen); sbuf[slen] = '\0';
    if (!strpbrk(pbuf, "*?[")) return strcmp(pbuf, sbuf) == 0;
    return fnmatch(pbuf, sbuf, FNM_PERIOD) == 0;
}

/* Backtracking matcher: pi indexes pattern components, si path components.
 * Returns true when the remaining suffixes match. */
static bool glob_match_from(const char *pattern, int nparts, const char **pend,
                            const char *path, int sparts, const char **send,
                            int pi, int si) {
    while (pi < nparts) {
        const char *pc = pend[pi];
        size_t plen = (pi + 1 < nparts)
                          ? (size_t)(pend[pi + 1] - pc - 1)
                          : strlen(pc);
        if (si >= sparts) {
            /* only a trailing "**" can consume nothing further */
            return plen == 2 && pc[0] == '*' && pc[1] == '*';
        }
        const char *sc = send[si];
        size_t slen = (si + 1 < sparts)
                          ? (size_t)(send[si + 1] - sc - 1)
                          : strlen(sc);
        if (plen == 2 && pc[0] == '*' && pc[1] == '*') {
            /* try consuming 0..(remaining) path components */
            for (int k = si; k <= sparts; k++) {
                if (glob_match_from(pattern, nparts, pend, path, sparts, send,
                                    pi + 1, k))
                    return true;
            }
            return false;
        }
        if (!match_one_component(pc, plen, sc, slen)) return false;
        pi++; si++;
    }
    return si == sparts;
}

static bool glob_match_components(const char *pattern, int nparts,
                                  const char **pend, const char *path,
                                  int sparts, const char **send) {
    return glob_match_from(pattern, nparts, pend, path, sparts, send, 0, 0);
}

/* Append a path to the match array, reallocating if needed */
static bool append_match(char ***matches, int *count, int *capacity,
                          const char *path) {
    if (*count >= *capacity) {
        int new_cap = *capacity == 0 ? 64 : *capacity * 2;
        if (new_cap > GLOB_MAX_MATCHES) new_cap = GLOB_MAX_MATCHES;
        char **new_m = (char **)realloc(*matches, (size_t)new_cap * sizeof(char *));
        if (!new_m) return false;
        *matches = new_m;
        *capacity = new_cap;
    }

    (*matches)[*count] = strdup(path);
    if (!(*matches)[*count]) return false;
    (*count)++;
    return true;
}

/* PoP: Python glob hides dotfiles from wildcard-led patterns — "*" and
 * "?" never match a leading '.' unless the pattern's corresponding
 * component starts with a literal '.'. */
/* Split pattern into components and check the rule against the final
 * path component's depth. Simplification faithful for single-component
 * and **-style patterns used by callers. */
static bool dotfile_allowed(const char *pattern, const char *rel) {
    if (!rel || rel[0] != '.') return true;   /* not a dotfile */
    /* find the pattern component aligned with the LAST path component */
    const char *last_slash_p = strrchr(pattern, '/');
    const char *seg = last_slash_p ? last_slash_p + 1 : pattern;
    /* if the aligned pattern segment ends in a double-star, a literal
     * leading dot is still hidden from wildcards deeper in the pattern;
     * Python hides dotfiles at every level, so we compare against the
     * final aligned segment of the pattern. */
    int rel_depth = 0;
    for (const char *q = rel; *q; q++) if (*q == '/') rel_depth++;
    int pat_depth = 0;
    for (const char *q = pattern; *q && q[0] && q[1]; q++) {
        if (q[0] == '*' && q[1] == '*') { continue; }  /* ** spans levels */
        if (*q == '/') pat_depth++;
    }
    (void)pat_depth;
    /* conservative faithful rule: hidden unless the aligned segment
     * begins with a literal dot */
    return seg[0] == '.';
}

/* Recursive directory walk */
static void walk_dir(const char *dir, const char *rel_base,
                     const char *pattern,
                     char ***matches, int *count, int *capacity) {
    DIR *dp = opendir(dir);
    if (!dp) return;

    struct dirent *entry;
    while ((entry = readdir(dp)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        /* Build full path */
        char full[GLOB_MAX_PATH];
        snprintf(full, sizeof(full), "%s/%s", dir, entry->d_name);

        /* Build relative path for matching */
        char rel[GLOB_MAX_PATH];
        if (rel_base[0]) {
            snprintf(rel, sizeof(rel), "%s/%s", rel_base, entry->d_name);
        } else {
            snprintf(rel, sizeof(rel), "%s", entry->d_name);
        }

        /* Check stat for directory check */
        struct stat st;
        bool is_dir = false;
        if (stat(full, &st) == 0 && S_ISDIR(st.st_mode))
            is_dir = true;

        /* Match this entry (Python dotfile-hiding rule applied) */
        if (dotfile_allowed(pattern, rel) && glob_match(pattern, rel)) {
            if (!append_match(matches, count, capacity, full))
                break;
        }

        /* Recurse into subdirectories */
        if (is_dir && *count < GLOB_MAX_MATCHES) {
            walk_dir(full, rel, pattern, matches, count, capacity);
        }
    }

    closedir(dp);
}

char **glob_find(const char *pattern, const char *base_dir, int *count) {
    if (!count) return NULL;
    *count = 0;

    if (!pattern || !base_dir) return NULL;

    /* Check base_dir exists */
    struct stat st;
    if (stat(base_dir, &st) != 0 || !S_ISDIR(st.st_mode))
        return NULL;

    int capacity = 0;
    char **matches = NULL;

    walk_dir(base_dir, "", pattern, &matches, count, &capacity);

    return matches;
}

void glob_free(char **matches, int count) {
    if (!matches) return;
    for (int i = 0; i < count; i++) {
        free(matches[i]);
    }
    free(matches);
}

/* ================================================================
 *  PoP: glob.escape
 * ================================================================ */
char *glob_escape(const char *path) {
    if (!path) return NULL;
    size_t len = strlen(path);
    char *out = (char *)malloc(len * 2 + 1);
    if (!out) return NULL;
    size_t o = 0;
    for (const char *p = path; *p; p++) {
        if (*p == '*' || *p == '?' || *p == '[')
            out[o++] = '\\';
        out[o++] = *p;
    }
    out[o] = '\0';
    return out;
}
