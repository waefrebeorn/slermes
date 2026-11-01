/* Slermes Windows compat: <fnmatch.h> — real wildcard matcher (no MinGW impl). */
#ifndef SLERMES_COMPAT_FNMATCH_H
#define SLERMES_COMPAT_FNMATCH_H
#define FNM_NOMATCH 1
#define FNM_PATHNAME (1 << 0)
#define FNM_NOESCAPE (1 << 1)
#define FNM_PERIOD   (1 << 2)
#define FNM_CASEFOLD (1 << 4)
static inline int slermes_fnmatch_impl(const char *p, const char *s, int flags);

static inline int slermes_fnmatch_impl(const char *p, const char *s, int flags)
{
    for (; *p; p++, s++) {
        if (*p == '*') {
            while (*p == '*') p++;
            if (!*p) return 0;
            for (; *s; s++)
                if (slermes_fnmatch_impl(p, s, flags) == 0) return 0;
            return FNM_NOMATCH;
        } else if (*p == '?') {
            if (!*s) return FNM_NOMATCH;
            if ((flags & FNM_PATHNAME) && *s == '/') return FNM_NOMATCH;
        } else if (*p == '[') {
            int neg = 0, matched = 0;
            const char *q = p + 1;
            if (*q == '!' || *q == '^') { neg = 1; q++; }
            if (!*s) return FNM_NOMATCH;
            while (*q && *q != ']') {
                if (q[1] == '-' && q[2] && q[2] != ']') {
                    char lo = q[0], hi = q[2];
                    if (*s >= lo && *s <= hi) matched = 1;
                    q += 3;
                } else {
                    if (*q == *s) matched = 1;
                    q++;
                }
            }
            if (*q != ']') return FNM_NOMATCH;
            if (matched == neg) return FNM_NOMATCH;
            p = q;
        } else {
            char pc = *p, sc = *s;
            if (flags & FNM_CASEFOLD) {
                if (pc >= 'A' && pc <= 'Z') pc = (char)(pc - 'A' + 'a');
                if (sc >= 'A' && sc <= 'Z') sc = (char)(sc - 'A' + 'a');
            }
            if (pc != sc) return FNM_NOMATCH;
        }
    }
    return *s ? FNM_NOMATCH : 0;
}

static inline int fnmatch(const char *pattern, const char *string, int flags)
{
    return slermes_fnmatch_impl(pattern, string, flags);
}
#endif
