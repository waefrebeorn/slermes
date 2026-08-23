/*
 * hermes_regex.c -- C regex convenience wrapper (J16: Python re port).
 * Wraps POSIX regex with simple match/search/replace API.
 */
#define _POSIX_C_SOURCE 200809L
#include "hermes_regex.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

#define REGEX_MAX_NAMES_MAX 32

struct hregex {
    regex_t native;
    int     flags;
    int     group_count;                    /* capture groups (excluding group 0) */
    char   *names[REGEX_MAX_NAMES_MAX];     /* named groups: name -> index+1      */
    int     name_count;
};

/* Count opening capture groups in the pattern, skipping escaped parens,
 * non-capturing "(?..." constructs and bracket expressions. Also record
 * (?P<name>...) names in declaration order (group index = order of ALL
 * capturing groups). */
static void scan_pattern(const char *pattern, struct hregex *re) {
    bool in_bracket = false;
    for (const char *p = pattern; *p; p++) {
        if (*p == '\\' && p[1]) { p++; continue; }
        if (in_bracket) { if (*p == ']') in_bracket = false; continue; }
        if (*p == '[') { in_bracket = true; continue; }
        if (*p == '(' ) {
            if (p[1] == '?') {
                /* "(?P<name>...)" captures; other "(?..." forms do not.
                 * Layout here is p[0]='(', p[1]='?', p[2]='P'|'<'. */
                if (p[2] == 'P' && p[3] == '<') {
                    const char *n = p + 4;
                    if (re->group_count + 1 < REGEX_MAX_GROUPS) {
                        re->group_count++;   /* this construct DOES capture */
                        const char *close = strchr(n, '>');
                        if (close && re->name_count < REGEX_MAX_NAMES_MAX) {
                            size_t len = (size_t)(close - n);
                            re->names[re->name_count] = (char *)malloc(len + 1);
                            if (re->names[re->name_count]) {
                                memcpy(re->names[re->name_count], n, len);
                                re->names[re->name_count][len] = '\0';
                                re->name_count++;
                            }
                        }
                    }
                }
                /* skip to end of the (?...) construct head */
                while (*p && *p != ')' && *p != '(') p++;
                if (!*p) break;
                continue;
            }
            re->group_count++;
        }
    }
}

hregex_t *regex_compile(const char *pattern, int flags) {
    if (!pattern || !pattern[0]) return NULL;
    hregex_t *re = (hregex_t *)calloc(1, sizeof(hregex_t));
    if (!re) return NULL;
    int cflags = REG_EXTENDED;
    if (flags & REGEX_FLAG_ICASE)     cflags |= REG_ICASE;
    if (flags & REGEX_FLAG_MULTILINE) cflags |= REG_NEWLINE;
    scan_pattern(pattern, re);
    /* POSIX regcomp rejects Python-only "(?P<name>" syntax — strip the
     * naming decoration so named groups compile as plain capture groups
     * with the same indices. */
    size_t plen = strlen(pattern);
    char *clean = (char *)malloc(plen + 1);
    if (!clean) { free(re); return NULL; }
    size_t o = 0;
    for (size_t i = 0; i < plen; i++) {
        if (pattern[i] == '\\' && i + 1 < plen) {
            clean[o++] = pattern[i]; clean[o++] = pattern[++i];
            continue;
        }
        if (pattern[i] == '(' && pattern[i+1] == '?' &&
            (pattern[i+2] == 'P' || pattern[i+2] == '<')) {
            clean[o++] = '(';
            i += 2;                       /* consume "?P" or "?<" */
            if (pattern[i] == 'P') i++;   /* skip P before < */
            while (i < plen && pattern[i] != '>') i++;  /* skip name */
            continue;
        }
        clean[o++] = pattern[i];
    }
    clean[o] = '\0';
    int err = regcomp(&re->native, clean, cflags);
    free(clean);
    if (err != 0) {
        regex_free(re);
        return NULL;
    }
    re->flags = flags;
    return re;
}

/* Fill a match result from a successful regexec over `str`. */
static regex_match_t *fill_match(hregex_t *re, const char *str) {
    regex_match_t *m = (regex_match_t *)calloc(1, sizeof(regex_match_t));
    if (!m) return NULL;
    regmatch_t pmatch[REGEX_MAX_GROUPS];
    int err = regexec(&re->native, str, REGEX_MAX_GROUPS, pmatch, 0);
    if (err != 0) { m->matched = false; return m; }
    m->matched = true;
    m->group_count = re ? re->group_count : 0;
    for (int i = 0; i < REGEX_MAX_GROUPS; i++) {
        m->start[i] = pmatch[i].rm_so;
        m->end[i]   = pmatch[i].rm_eo;
        if (pmatch[i].rm_so >= 0) {
            size_t len = (size_t)(pmatch[i].rm_eo - pmatch[i].rm_so);
            m->groups[i] = (char *)malloc(len + 1);
            if (m->groups[i]) {
                memcpy(m->groups[i], str + pmatch[i].rm_so, len);
                m->groups[i][len] = '\0';
            }
        }
    }
    return m;
}

regex_match_t *regex_match(hregex_t *re, const char *str) {
    if (!re || !str) return NULL;
    return fill_match(re, str);
}

regex_match_t *regex_search(hregex_t *re, const char *str) {
    return regex_match(re, str);
}

/* Expand "$N" backreferences in `replacement` given match offsets.
 * Appends to a dynamically grown buffer. Returns 0 on success. */
typedef struct { char *buf; size_t len, cap; } sbuf_t;

static int sbuf_reserve(sbuf_t *s, size_t need) {
    if (s->len + need + 1 <= s->cap) return 0;
    size_t ncap = s->cap ? s->cap * 2 : 256;
    while (ncap < s->len + need + 1) ncap *= 2;
    char *nb = (char *)realloc(s->buf, ncap);
    if (!nb) return -1;
    s->buf = nb; s->cap = ncap;
    return 0;
}

static void sbuf_append(sbuf_t *s, const char *data, size_t n) {
    if (sbuf_reserve(s, n) != 0) return;
    memcpy(s->buf + s->len, data, n);
    s->len += n;
    s->buf[s->len] = '\0';
}

/* Expand replacement for match spanning [ms,me), using group offsets gm. */
static void expand_replacement(sbuf_t *out, const char *replacement,
                               const char *str,
                               const regmatch_t *pmatch) {
    size_t rl = strlen(replacement);
    for (size_t i = 0; i < rl; i++) {
        if (replacement[i] == '$' && i + 1 < rl) {
            /* accept $1..$15 (two digits max, value < REGEX_MAX_GROUPS) */
            int g = -1, width = 0;
            if (replacement[i+1] >= '0' && replacement[i+1] <= '9') {
                g = replacement[i+1] - '0'; width = 1;
                if (i + 2 < rl && replacement[i+2] >= '0' && replacement[i+2] <= '9') {
                    int g2 = g * 10 + (replacement[i+2] - '0');
                    if (g2 < REGEX_MAX_GROUPS) { g = g2; width = 2; }
                }
            }
            if (g >= 0) {
                if (pmatch[g].rm_so >= 0)
                    sbuf_append(out, str + pmatch[g].rm_so,
                                (size_t)(pmatch[g].rm_eo - pmatch[g].rm_so));
                i += (size_t)width;
                continue;
            }
        }
        sbuf_append(out, replacement + i, 1);
    }
}

char *regex_replace_n(hregex_t *re, const char *str, const char *replacement,
                      int max_count) {
    if (!re || !str) return NULL;
    if (!replacement) replacement = "";
    sbuf_t out = {0};
    size_t slen = strlen(str);
    size_t pos = 0;
    int done = 0;
    regmatch_t pmatch[REGEX_MAX_GROUPS];

    while (max_count <= 0 || done < max_count) {
        int eflags = 0;
        if (regexec(&re->native, str + pos, REGEX_MAX_GROUPS, pmatch, eflags) != 0)
            break;
        int ms = (int)(pmatch[0].rm_so);
        int me = (int)(pmatch[0].rm_eo);
        if (ms < 0) break;
        /* copy unmatched prefix */
        sbuf_append(&out, str + pos, (size_t)ms);
        /* adjust group offsets to absolute positions for expansion */
        regmatch_t abs[REGEX_MAX_GROUPS];
        for (int i = 0; i < REGEX_MAX_GROUPS; i++) {
            if (pmatch[i].rm_so >= 0) {
                abs[i].rm_so = pmatch[i].rm_so + (regoff_t)pos;
                abs[i].rm_eo = pmatch[i].rm_eo + (regoff_t)pos;
            } else { abs[i].rm_so = abs[i].rm_eo = -1; }
        }
        expand_replacement(&out, replacement, str, abs);
        done++;
        if (me == ms) {
            /* zero-width match: copy one char and advance (Python semantics) */
            if (pos + (size_t)me < slen)
                sbuf_append(&out, str + pos + me, 1);
            else
                break;
            pos += (size_t)me + 1;
        } else {
            pos += (size_t)me;
        }
        if (pos > slen) break;
    }
    if (!out.buf) {
        if (sbuf_reserve(&out, 0) != 0) return NULL;
        out.buf[0] = '\0';
    }
    sbuf_append(&out, str + pos, slen - pos);
    return out.buf ? out.buf : strdup(str);
}

char *regex_replace(hregex_t *re, const char *str, const char *replacement) {
    /* Python re.sub default count=0 replaces ALL occurrences. The legacy
     * single-shot behavior is kept only under the old name for callers
     * that relied on it; new code should use regex_replace_n. */
    return regex_replace_n(re, str, replacement, 1);
}

int regex_findall(hregex_t *re, const char *str, char **out, int out_max) {
    if (!re || !str || !out || out_max <= 0) return 0;
    size_t slen = strlen(str);
    size_t pos = 0;
    int count = 0;
    regmatch_t pmatch[REGEX_MAX_GROUPS];
    while (count < out_max) {
        if (regexec(&re->native, str + pos, 1, pmatch, 0) != 0) break;
        if (pmatch[0].rm_so < 0) break;
        size_t ms = (size_t)pmatch[0].rm_so, me = (size_t)pmatch[0].rm_eo;
        size_t len = me - ms;
        out[count] = (char *)malloc(len + 1);
        if (out[count]) {
            memcpy(out[count], str + pos + ms, len);
            out[count][len] = '\0';
        }
        count++;
        if (me == ms) {
            if (pos + me >= slen) break;
            pos += me + 1;
        } else {
            pos += me;
        }
        if (pos > slen) break;
    }
    return count;
}

int regex_split(hregex_t *re, const char *str, int maxsplit,
                char **out, int out_max) {
    if (!re || !str || !out || out_max <= 0) return 0;
    size_t slen = strlen(str);
    size_t pos = 0;
    int count = 0;
    int splits = 0;
    regmatch_t pmatch[REGEX_MAX_GROUPS];
    while (count < out_max) {
        int last_field = (maxsplit >= 0 && splits >= maxsplit);
        if (last_field ||
            regexec(&re->native, str + pos, 1, pmatch, 0) != 0 ||
            pmatch[0].rm_so < 0) {
            /* remainder is the final field */
            out[count++] = strdup(str + pos);
            break;
        }
        size_t ms = (size_t)pmatch[0].rm_so, me = (size_t)pmatch[0].rm_eo;
        out[count] = (char *)malloc(ms + 1);
        if (out[count]) {
            memcpy(out[count], str + pos, ms);
            out[count][ms] = '\0';
        }
        count++;
        splits++;
        if (me == ms) {
            if (pos + me >= slen) { out[count++] = strdup(""); break; }
            pos += me + 1;
        } else {
            pos += me;
        }
        if (pos > slen) { out[count++] = strdup(""); break; }
    }
    return count;
}

void regex_split_free(char **fields, int count) {
    if (!fields) return;
    for (int i = 0; i < count; i++) free(fields[i]);
}

int regex_group_count(hregex_t *re) {
    return re ? re->group_count : 0;
}

int regex_group_index(hregex_t *re, const char *name) {
    if (!re || !name) return -1;
    for (int i = 0; i < re->name_count; i++)
        if (strcmp(re->names[i], name) == 0) return i + 1; /* group 0 = whole */
    return -1;
}

void regex_free(hregex_t *re) {
    if (!re) return;
    regfree(&re->native);
    for (int i = 0; i < re->name_count; i++) free(re->names[i]);
    free(re);
}

void regex_match_free(regex_match_t *m) {
    if (!m) return;
    for (int i = 0; i < REGEX_MAX_GROUPS; i++) free(m->groups[i]);
    free(m);
}

char *regex_extract(const char *pattern, const char *str, int group) {
    if (!pattern || !str) return NULL;
    hregex_t *re = regex_compile(pattern, 0);
    if (!re) return NULL;
    regex_match_t *m = regex_search(re, str);
    char *result = NULL;
    if (m && m->matched && group >= 0 && group < REGEX_MAX_GROUPS && m->groups[group])
        result = strdup(m->groups[group]);
    regex_match_free(m);
    regex_free(re);
    return result;
}
