/*
 * slv_string.c -- the string library, including a Lua-pattern matcher.
 * See slv_internal.h.
 */
#include "slv_internal.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------- classes */

static int match_class(int c, int cl) {
    int res;
    switch (cl) {
    case 'a': res = isalpha(c); break;
    case 'c': res = iscntrl(c); break;
    case 'd': res = isdigit(c); break;
    case 'g': res = isgraph(c); break;
    case 'l': res = islower(c); break;
    case 'p': res = ispunct(c); break;
    case 's': res = isspace(c); break;
    case 'u': res = isupper(c); break;
    case 'w': res = isalnum(c); break;
    case 'x': res = isxdigit(c); break;
    default: return (cl == c);
    }
    if (isupper(cl)) res = !res;
    return res;
}

/* -------------------------------------------------------------- matcher */

#define MAXCAPTURES 32
#define CAP_UNFINISHED (-1)
#define CAP_POSITION (-2)
#define L_ESC '%'

typedef struct {
    const char *src_init;
    const char *src_end;
    const char *p_end;
    int level;
    struct { const char *init; ptrdiff_t len; } capture[MAXCAPTURES];
} matchstate;

static const char *match(matchstate *ms, const char *s, const char *p);

static int check_capture(matchstate *ms, int l) {
    l -= '1';
    if (l < 0 || l >= ms->level || ms->capture[l].len == CAP_UNFINISHED) return -1;
    return l;
}

static int capture_to_close(matchstate *ms) {
    int level = ms->level;
    for (level--; level >= 0; level--)
        if (ms->capture[level].len == CAP_UNFINISHED) return level;
    return -1;
}

static const char *classend(matchstate *ms, const char *p) {
    switch (*p++) {
    case L_ESC: {
        if (p >= ms->p_end) { /* error */ }
        return p + 1;
    }
    case '[':
        if (*p == '^') p++;
        do {
            if (p >= ms->p_end) break;
            if (*p++ == L_ESC) { if (p < ms->p_end) p++; }
        } while (*p != ']' && p < ms->p_end);
        return p + 1;
    default: return p;
    }
}

static int matchbracketclass(int c, const char *p, const char *ec) {
    int sig = 1;
    if (*(p + 1) == '^') { sig = 0; p++; }
    while (++p < ec) {
        if (*p == L_ESC) {
            p++;
            if (match_class(c, (unsigned char)*p)) return sig;
        } else if (*(p + 1) == '-' && (p + 2 < ec)) {
            p += 2;
            if ((unsigned char)*(p - 2) <= c && c <= (unsigned char)*p) return sig;
        } else if ((unsigned char)*p == c) return sig;
    }
    return !sig;
}

static int singlematch(matchstate *ms, const char *s, const char *p, const char *ep) {
    if (s >= ms->src_end) return 0;
    int c = (unsigned char)*s;
    switch (*p) {
    case '.': return 1;
    case L_ESC: return match_class(c, (unsigned char)*(p + 1));
    case '[': return matchbracketclass(c, p, ep - 1);
    default: return ((unsigned char)*p == c);
    }
}

static const char *matchbalance(matchstate *ms, const char *s, const char *p) {
    if (p >= ms->p_end - 1) return NULL;
    if (*s != *p) return NULL;
    int b = *p, e = *(p + 1), cont = 1;
    while (++s < ms->src_end) {
        if (*s == e) { if (--cont == 0) return s + 1; }
        else if (*s == b) cont++;
    }
    return NULL;
}

static const char *max_expand(matchstate *ms, const char *s, const char *p, const char *ep) {
    ptrdiff_t i = 0;
    while (singlematch(ms, s + i, p, ep)) i++;
    while (i >= 0) {
        const char *res = match(ms, s + i, ep + 1);
        if (res) return res;
        i--;
    }
    return NULL;
}

static const char *min_expand(matchstate *ms, const char *s, const char *p, const char *ep) {
    for (;;) {
        const char *res = match(ms, s, ep + 1);
        if (res) return res;
        if (singlematch(ms, s, p, ep)) s++;
        else return NULL;
    }
}

static const char *start_capture(matchstate *ms, const char *s, const char *p, int what) {
    const char *res;
    int level = ms->level;
    if (level >= MAXCAPTURES) return NULL;
    ms->capture[level].len = what;
    ms->capture[level].init = s;
    ms->level = level + 1;
    if ((res = match(ms, s, p)) == NULL) ms->level--;
    return res;
}

static const char *end_capture(matchstate *ms, const char *s, const char *p) {
    int l = capture_to_close(ms);
    const char *res;
    if (l < 0) return NULL;
    ms->capture[l].len = s - ms->capture[l].init;
    if ((res = match(ms, s, p)) == NULL) ms->capture[l].len = CAP_UNFINISHED;
    return res;
}

static const char *match_capture(matchstate *ms, const char *s, int l) {
    size_t len;
    l = check_capture(ms, l);
    if (l < 0) return NULL;
    len = (size_t)ms->capture[l].len;
    if ((size_t)(ms->src_end - s) >= len && memcmp(ms->capture[l].init, s, len) == 0)
        return s + len;
    return NULL;
}

static const char *match(matchstate *ms, const char *s, const char *p) {
    if (ms->level == 0 && s == NULL) return NULL;
    while (p < ms->p_end) {
        switch (*p) {
        case '(':
            return (*(p + 1) == ')') ? start_capture(ms, s, p + 2, CAP_POSITION)
                                     : start_capture(ms, s, p + 1, CAP_UNFINISHED);
        case ')': return end_capture(ms, s, p + 1);
        case '$':
            if ((p + 1) == ms->p_end) return (s == ms->src_end) ? s : NULL;
            goto dflt;
        case L_ESC:
            switch (*(p + 1)) {
            case 'b': {
                s = matchbalance(ms, s, p + 2);
                if (s == NULL) return NULL;
                p += 4;
                continue;
            }
            case 'f': {
                const char *ep;
                p += 2;
                if (*p != '[') return NULL;
                ep = classend(ms, p);
                int previous = (s == ms->src_init) ? '\0' : (unsigned char)*(s - 1);
                int current = (s < ms->src_end) ? (unsigned char)*s : '\0';
                if (!matchbracketclass(previous, p, ep - 1) &&
                     matchbracketclass(current, p, ep - 1)) {
                    p = ep;
                    continue;
                }
                return NULL;
            }
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9': {
                s = match_capture(ms, s, (unsigned char)*(p + 1));
                if (s == NULL) return NULL;
                p += 2;
                continue;
            }
            default: goto dflt;
            }
            break;
        default: dflt: {
            const char *ep = classend(ms, p);
            if (!singlematch(ms, s, p, ep)) {
                if (*ep == '*' || *ep == '?' || *ep == '-') { p = ep + 1; continue; }
                return NULL;
            }
            switch (*ep) {
            case '?': {
                const char *res = match(ms, s + 1, ep + 1);
                if (res) return res;
                p = ep + 1;
                continue;
            }
            case '+': return max_expand(ms, s + 1, p, ep);
            case '*': return max_expand(ms, s, p, ep);
            case '-': return min_expand(ms, s, p, ep);
            default: s++; p = ep; continue;
            }
        }
        }
    }
    return s;
}

/* push the captures of a finished match */
static int push_captures(slv *L, matchstate *ms, const char *s, const char *e, slv_value *out) {
    int nlevels = (ms->level == 0 && s) ? 1 : ms->level;
    if (nlevels > MAXCAPTURES) nlevels = MAXCAPTURES;
    for (int i = 0; i < nlevels; i++) {
        if (i >= ms->level) {
            out[i].t = SLV_INT;
            out[i].as.i = (int64_t)(ms->capture[i].init - ms->src_init) + 1;
        } else if (ms->capture[i].len == CAP_POSITION) {
            out[i].t = SLV_INT;
            out[i].as.i = (int64_t)(ms->capture[i].init - ms->src_init) + 1;
        } else {
            size_t len = (size_t)ms->capture[i].len;
            out[i] = slv_str(slv_intern(L, ms->capture[i].init, len));
        }
    }
    return nlevels;
}

/* --------------------------------------------------------- primitives */

static int posrelat(int64_t pos, size_t len) {
    if (pos >= 0) return (int)pos;
    if ((size_t)(-pos) > len) return 0;
    return (int)(len + (size_t)pos + 1);
}

static slv_value s_len(slv *L, slv_value *a, int n) {
    (void)L;
    if (n < 1 || a[0].t != SLV_STR) return slv_nil();
    return slv_integer((int64_t)strlen(a[0].as.s));
}

static slv_value s_sub(slv *L, slv_value *a, int n) {
    if (n < 1 || a[0].t != SLV_STR) return slv_nil();
    size_t l = strlen(a[0].as.s);
    int i = posrelat(n >= 2 ? slv_tointeger(a[1], NULL) : 1, l);
    int j = posrelat(n >= 3 ? slv_tointeger(a[2], NULL) : (int64_t)l, l);
    if (i < 1) i = 1;
    if (j > (int)l) j = (int)l;
    if (i > j) return slv_str(slv_intern(L, "", 0));
    return slv_str(slv_intern(L, a[0].as.s + i - 1, (size_t)(j - i + 1)));
}

static slv_value s_rep(slv *L, slv_value *a, int n) {
    if (n < 1 || a[0].t != SLV_STR) return slv_nil();
    int64_t cnt = (n >= 2) ? slv_tointeger(a[1], NULL) : 0;
    const char *sep = (n >= 3 && a[2].t == SLV_STR) ? a[2].as.s : "";
    size_t sl = strlen(a[0].as.s), sepl = strlen(sep);
    if (cnt <= 0) return slv_str(slv_intern(L, "", 0));
    size_t total = (size_t)cnt * sl + (size_t)(cnt - 1) * sepl;
    char *buf = (char *)malloc(total + 1);
    if (!buf) return slv_nil();
    for (int64_t i = 0; i < cnt; i++) {
        memcpy(buf, a[0].as.s, sl); buf += sl;
        if (i + 1 < cnt) { memcpy(buf, sep, sepl); buf += sepl; }
    }
    *buf = 0;
    slv_value r = slv_str(slv_intern(L, buf - total, total));
    free(buf - total);
    return r;
}

static slv_value s_upper(slv *L, slv_value *a, int n) {
    if (n < 1 || a[0].t != SLV_STR) return slv_nil();
    size_t l = strlen(a[0].as.s);
    char *buf = (char *)malloc(l + 1);
    if (!buf) return slv_nil();
    for (size_t i = 0; i < l; i++) buf[i] = (char)toupper((unsigned char)a[0].as.s[i]);
    buf[l] = 0;
    slv_value r = slv_str(slv_intern(L, buf, l));
    free(buf);
    return r;
}

static slv_value s_lower(slv *L, slv_value *a, int n) {
    if (n < 1 || a[0].t != SLV_STR) return slv_nil();
    size_t l = strlen(a[0].as.s);
    char *buf = (char *)malloc(l + 1);
    if (!buf) return slv_nil();
    for (size_t i = 0; i < l; i++) buf[i] = (char)tolower((unsigned char)a[0].as.s[i]);
    buf[l] = 0;
    slv_value r = slv_str(slv_intern(L, buf, l));
    free(buf);
    return r;
}

static slv_value s_reverse(slv *L, slv_value *a, int n) {
    if (n < 1 || a[0].t != SLV_STR) return slv_nil();
    size_t l = strlen(a[0].as.s);
    char *buf = (char *)malloc(l + 1);
    if (!buf) return slv_nil();
    for (size_t i = 0; i < l; i++) buf[i] = a[0].as.s[l - 1 - i];
    buf[l] = 0;
    slv_value r = slv_str(slv_intern(L, buf, l));
    free(buf);
    return r;
}

static slv_value s_byte(slv *L, slv_value *a, int n) {
    if (n < 1 || a[0].t != SLV_STR) return slv_nil();
    size_t l = strlen(a[0].as.s);
    int i = posrelat(n >= 2 ? slv_tointeger(a[1], NULL) : 1, l);
    int j = posrelat(n >= 3 ? slv_tointeger(a[2], NULL) : (int64_t)i, l);
    if (i < 1) i = 1;
    if (j > (int)l) j = (int)l;
    if (i > j) return slv_nil();
    int cnt = j - i + 1;
    slv_value *out = SLV_ALLOCN(slv_value, cnt, L);
    for (int k = 0; k < cnt; k++) out[k] = slv_integer((unsigned char)a[0].as.s[i - 1 + k]);
    return slv_multi_value(L, out, cnt);
}

static slv_value s_char(slv *L, slv_value *a, int n) {
    char buf[256]; int k = 0;
    for (int i = 0; i < n && k < 255; i++) {
        int64_t v = slv_tointeger(a[i], NULL);
        buf[k++] = (char)(v & 0xFF);
    }
    return slv_str(slv_intern(L, buf, (size_t)k));
}

/* --------------------------------------------------------------- format */

static void addquoted(slv *L, char **buf, size_t *len, size_t *cap, const char *s) {
    size_t l = strlen(s);
    while (*len + l + 3 > *cap) { *cap = *cap * 2 + 32; *buf = (char *)realloc(*buf, *cap); }
    (*buf)[(*len)++] = '"';
    for (size_t i = 0; i < l; i++) {
        char c = s[i];
        if (c == '"' || c == '\\' || c == '\n') { (*buf)[(*len)++] = '\\'; (*buf)[(*len)++] = c; }
        else if ((unsigned char)c < 32 || (unsigned char)c == 127) {
            char tmp[8]; snprintf(tmp, sizeof tmp, "\\%d", (int)(unsigned char)c);
            size_t tl = strlen(tmp);
            while (*len + tl > *cap) { *cap = *cap * 2 + 32; *buf = (char *)realloc(*buf, *cap); }
            memcpy(*buf + *len, tmp, tl); *len += tl;
        } else (*buf)[(*len)++] = c;
    }
    (*buf)[(*len)++] = '"';
    (void)L;
}

static slv_value s_format(slv *L, slv_value *a, int n) {
    if (n < 1 || a[0].t != SLV_STR) return slv_nil();
    const char *fmt = a[0].as.s;
    size_t fl = strlen(fmt);
    char *buf = NULL; size_t len = 0, cap = 0;
    int arg = 1;
    for (size_t i = 0; i < fl; i++) {
        if (fmt[i] != '%') {
            if (len + 2 > cap) { cap = cap * 2 + 32; buf = (char *)realloc(buf, cap); }
            buf[len++] = fmt[i];
            continue;
        }
        i++;
        if (i < fl && fmt[i] == '%') { if (len + 2 > cap) { cap = cap * 2 + 32; buf = (char *)realloc(buf, cap); } buf[len++] = '%'; continue; }
        /* collect the spec */
        char spec[64]; int sp = 0; spec[sp++] = '%';
        while (i < fl && sp < 60 && strchr("-+ #0", fmt[i])) spec[sp++] = fmt[i++];
        while (i < fl && sp < 60 && isdigit((unsigned char)fmt[i])) spec[sp++] = fmt[i++];
        if (i < fl && fmt[i] == '.') {
            spec[sp++] = fmt[i++];
            while (i < fl && sp < 60 && isdigit((unsigned char)fmt[i])) spec[sp++] = fmt[i++];
        }
        char conv = (i < fl) ? fmt[i] : 0;
        spec[sp] = 0;
        char tmp[512];
        switch (conv) {
        case 'd': case 'i': {
            if (arg >= n) { free(buf); return slv_nil(); }
            int64_t v = slv_tointeger(a[arg++], NULL);
            snprintf(tmp, sizeof tmp, spec, (long long)v);
            break;
        }
        case 'u': case 'o': case 'x': case 'X': {
            if (arg >= n) { free(buf); return slv_nil(); }
            int64_t v = slv_tointeger(a[arg++], NULL);
            snprintf(tmp, sizeof tmp, spec, (long long)v);
            break;
        }
        case 'c': {
            if (arg >= n) { free(buf); return slv_nil(); }
            int v = (int)slv_tointeger(a[arg++], NULL);
            snprintf(tmp, sizeof tmp, spec, v);
            break;
        }
        case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': case 'a': case 'A': {
            if (arg >= n) { free(buf); return slv_nil(); }
            double v = slv_tonumber(a[arg++], NULL);
            snprintf(tmp, sizeof tmp, spec, v);
            break;
        }
        case 's': {
            if (arg >= n) { free(buf); return slv_nil(); }
            const char *s = slv_tostring_intern(L, a[arg++]);
            if (strchr(spec, '.') || strlen(s) > 400) {
                /* respect a precision if given, else print in full */
                snprintf(tmp, sizeof tmp, spec, s);
            } else snprintf(tmp, sizeof tmp, spec, s);
            break;
        }
        case 'q': {
            if (arg >= n) { free(buf); return slv_nil(); }
            addquoted(L, &buf, &len, &cap, slv_tostring_intern(L, a[arg++]));
            continue;
        }
        default:
            free(buf);
            return slv_nil();
        }
        size_t tl = strlen(tmp);
        while (len + tl + 1 > cap) { cap = cap * 2 + 32; buf = (char *)realloc(buf, cap); }
        memcpy(buf + len, tmp, tl); len += tl;
    }
    if (buf) buf[len] = 0;
    slv_value r = slv_str(slv_intern(L, buf ? buf : "", len));
    free(buf);
    return r;
}

/* ------------------------------------------------------ pattern matching */

static slv_value s_find(slv *L, slv_value *a, int n) {
    if (n < 2 || a[0].t != SLV_STR || a[1].t != SLV_STR) return slv_nil();
    size_t ls = strlen(a[0].as.s), lp = strlen(a[1].as.s);
    int64_t init = (n >= 3) ? slv_tointeger(a[2], NULL) : 1;
    int i = posrelat(init, ls);
    if (i < 1) i = 1;
    if (i > (int)ls + 1) return slv_nil();
    int plain = (n >= 4) && slv_truthy(a[3]);
    if (plain || lp == 0) {
        const char *found = NULL;
        if (lp == 0) found = a[0].as.s + (i - 1);
        else {
            for (int k = i - 1; k + (int)lp <= (int)ls; k++)
                if (memcmp(a[0].as.s + k, a[1].as.s, lp) == 0) { found = a[0].as.s + k; break; }
        }
        if (!found) return slv_nil();
        slv_value out[2];
        out[0] = slv_integer(found - a[0].as.s + 1);
        out[1] = slv_integer(found - a[0].as.s + (int64_t)lp);
        return slv_multi_value(L, out, 2);
    }
    matchstate ms;
    memset(&ms, 0, sizeof ms);
    ms.src_init = a[0].as.s;
    ms.src_end = a[0].as.s + ls;
    ms.p_end = a[1].as.s + lp;
    int anchor = (a[1].as.s[0] == '^');
    const char *p = a[1].as.s + (anchor ? 1 : 0);
    const char *s = a[0].as.s + (i - 1);
    do {
        ms.level = 0;
        const char *e = match(&ms, s, p);
        if (e) {
            slv_value out[MAXCAPTURES + 2];
            out[0] = slv_integer(s - a[0].as.s + 1);
            out[1] = slv_integer(e - a[0].as.s);
            int nc = push_captures(L, &ms, NULL, NULL, out + 2);
            return slv_multi_value(L, out, nc + 2);
        }
        s++;
    } while (s <= ms.src_end && !anchor);
    return slv_nil();
}

static slv_value s_match(slv *L, slv_value *a, int n) {
    if (n < 2 || a[0].t != SLV_STR || a[1].t != SLV_STR) return slv_nil();
    size_t ls = strlen(a[0].as.s), lp = strlen(a[1].as.s);
    int64_t init = (n >= 3) ? slv_tointeger(a[2], NULL) : 1;
    int i = posrelat(init, ls);
    if (i < 1) i = 1;
    if (i > (int)ls + 1) return slv_nil();
    matchstate ms;
    memset(&ms, 0, sizeof ms);
    ms.src_init = a[0].as.s;
    ms.src_end = a[0].as.s + ls;
    ms.p_end = a[1].as.s + lp;
    int anchor = (a[1].as.s[0] == '^');
    const char *p = a[1].as.s + (anchor ? 1 : 0);
    const char *s = a[0].as.s + (i - 1);
    do {
        ms.level = 0;
        const char *e = match(&ms, s, p);
        if (e) {
            slv_value out[MAXCAPTURES];
            int nc = push_captures(L, &ms, s, e, out);
            if (nc == 0) {
                out[0] = slv_str(slv_intern(L, s, (size_t)(e - s)));
                nc = 1;
            }
            return slv_multi_value(L, out, nc);
        }
        s++;
    } while (s <= ms.src_end && !anchor);
    return slv_nil();
}

typedef struct { const char *src; size_t ls; const char *pat; size_t lp; int64_t pos; } gmatch_state;

static slv_value iter_gmatch(slv *L, slv_value *a, int n, void *ud) {
    (void)n;
    gmatch_state *st = (gmatch_state *)ud;
    matchstate ms;
    const char *s = st->src + st->pos;
    while (s <= st->src + st->ls) {
        memset(&ms, 0, sizeof ms);
        ms.src_init = st->src;
        ms.src_end = st->src + st->ls;
        ms.p_end = st->pat + st->lp;
        ms.level = 0;
        const char *e = match(&ms, s, st->pat);
        if (e) {
            st->pos = (e == s) ? (size_t)(e - st->src) + 1 : (size_t)(e - st->src);
            slv_value out[MAXCAPTURES];
            int nc = push_captures(L, &ms, s, e, out);
            if (nc == 0) {
                out[0] = slv_str(slv_intern(L, s, (size_t)(e - s)));
                nc = 1;
            }
            return slv_multi_value(L, out, nc);
        }
        s++;
    }
    return slv_nil();
}

static slv_value s_gmatch(slv *L, slv_value *a, int n) {
    if (n < 2 || a[0].t != SLV_STR || a[1].t != SLV_STR) return slv_nil();
    gmatch_state *st = (gmatch_state *)calloc(1, sizeof *st);
    st->src = a[0].as.s; st->ls = strlen(a[0].as.s);
    st->pat = a[1].as.s; st->lp = strlen(a[1].as.s);
    slv_value it; it.t = SLV_CFUNCTION; it.as.cf.f = iter_gmatch; it.as.cf.ud = st;
    return it;
}

static slv_value s_gsub(slv *L, slv_value *a, int n) {
    if (n < 3 || a[0].t != SLV_STR || a[1].t != SLV_STR) return slv_nil();
    const char *src = a[0].as.s;
    size_t ls = strlen(src);
    const char *pat = a[1].as.s;
    size_t lp = strlen(pat);
    int64_t maxn = (n >= 4) ? slv_tointeger(a[3], NULL) : (int64_t)ls + 1;
    int anchor = (pat[0] == '^');
    const char *p = pat + (anchor ? 1 : 0);
    char *buf = NULL; size_t len = 0, cap = 0;
    const char *s = src;
    int64_t count = 0;
    matchstate ms;
    do {
        ms.level = 0;
        ms.src_init = src; ms.src_end = src + ls; ms.p_end = pat + lp;
        const char *e = match(&ms, s, p);
        if (e) {
            count++;
            /* apply the replacement */
            if (a[2].t == SLV_STR) {
                for (const char *r = a[2].as.s; *r; r++) {
                    if (*r == '%' && *(r + 1)) {
                        r++;
                        if (*r == '%') { if (len + 2 > cap) { cap = cap*2+32; buf = (char*)realloc(buf, cap); } buf[len++] = '%'; }
                        else if (isdigit((unsigned char)*r)) {
                            int ci = *r - '0';
                            if (ci == 0) {
                                size_t l = (size_t)(e - s);
                                while (len + l + 1 > cap) { cap = cap*2+32; buf = (char*)realloc(buf, cap); }
                                memcpy(buf + len, s, l); len += l;
                            } else {
                                int li = check_capture(&ms, *r);
                                if (li >= 0) {
                                    size_t l = (size_t)ms.capture[li].len;
                                    const char *init = ms.capture[li].init;
                                    while (len + l + 1 > cap) { cap = cap*2+32; buf = (char*)realloc(buf, cap); }
                                    memcpy(buf + len, init, l); len += l;
                                }
                            }
                        }
                    } else {
                        if (len + 2 > cap) { cap = cap*2+32; buf = (char*)realloc(buf, cap); }
                        buf[len++] = *r;
                    }
                }
            } else if (a[2].t == SLV_TABLE) {
                int li = check_capture(&ms, '1');
                slv_value key = (li >= 0 && ms.capture[li].len == CAP_POSITION)
                                      ? slv_integer(ms.capture[li].init - src + 1)
                                      : slv_str(slv_intern(L, ms.capture[li >= 0 ? li : 0].init,
                                                               (size_t)(li >= 0 ? ms.capture[li].len : 0)));
                slv_value rep = slv_table_get(L, a[2].as.tab, key);
                if (rep.t == SLV_STR) {
                    size_t l = strlen(rep.as.s);
                    while (len + l + 1 > cap) { cap = cap*2+32; buf = (char*)realloc(buf, cap); }
                    memcpy(buf + len, rep.as.s, l); len += l;
                } else if (rep.t == SLV_NUM || rep.t == SLV_INT) {
                    const char *rs = slv_tostring_intern(L, rep);
                    size_t l = strlen(rs);
                    while (len + l + 1 > cap) { cap = cap*2+32; buf = (char*)realloc(buf, cap); }
                    memcpy(buf + len, rs, l); len += l;
                }
            } else if (a[2].t == SLV_FUNCTION || a[2].t == SLV_CFUNCTION) {
                slv_value caps[MAXCAPTURES];
                int nc = push_captures(L, &ms, s, e, caps);
                if (nc == 0) { caps[0] = slv_str(slv_intern(L, s, (size_t)(e - s))); nc = 1; }
                slv_value rep = slv_call(L, a[2], caps, nc);
                if (rep.t == SLV_STR) {
                    size_t l = strlen(rep.as.s);
                    while (len + l + 1 > cap) { cap = cap*2+32; buf = (char*)realloc(buf, cap); }
                    memcpy(buf + len, rep.as.s, l); len += l;
                } else if (rep.t == SLV_NUM || rep.t == SLV_INT) {
                    const char *rs = slv_tostring_intern(L, rep);
                    size_t l = strlen(rs);
                    while (len + l + 1 > cap) { cap = cap*2+32; buf = (char*)realloc(buf, cap); }
                    memcpy(buf + len, rs, l); len += l;
                }
            }
            if (e) s = e;
            if (e == s && *s) {
                if (len + 2 > cap) { cap = cap*2+32; buf = (char*)realloc(buf, cap); }
                buf[len++] = *s++;
            }
        } else {
            if (s < src + ls) {
                if (len + 2 > cap) { cap = cap*2+32; buf = (char*)realloc(buf, cap); }
                buf[len++] = *s;
            }
            s++;
        }
        if (count >= maxn) break;
    } while (s <= src + ls && !anchor);
    if (s <= src + ls) {
        size_t rem = (size_t)(src + ls - s);
        while (len + rem + 1 > cap) { cap = cap*2+32; buf = (char*)realloc(buf, cap); }
        memcpy(buf + len, s, rem); len += rem;
    }
    if (buf) buf[len] = 0;
    slv_value out[2];
    out[0] = slv_str(slv_intern(L, buf ? buf : "", len));
    out[1] = slv_integer(count);
    free(buf);
    return slv_multi_value(L, out, 2);
}

/* --------------------------------------------------------- registration */

static slv_value cfn_len(slv *L, slv_value *a, int n, void *ud) { (void)ud; return s_len(L, a, n); }
static slv_value cfn_sub(slv *L, slv_value *a, int n, void *ud) { (void)ud; return s_sub(L, a, n); }
static slv_value cfn_rep(slv *L, slv_value *a, int n, void *ud) { (void)ud; return s_rep(L, a, n); }
static slv_value cfn_upper(slv *L, slv_value *a, int n, void *ud) { (void)ud; return s_upper(L, a, n); }
static slv_value cfn_lower(slv *L, slv_value *a, int n, void *ud) { (void)ud; return s_lower(L, a, n); }
static slv_value cfn_reverse(slv *L, slv_value *a, int n, void *ud) { (void)ud; return s_reverse(L, a, n); }
static slv_value cfn_byte(slv *L, slv_value *a, int n, void *ud) { (void)ud; return s_byte(L, a, n); }
static slv_value cfn_char(slv *L, slv_value *a, int n, void *ud) { (void)ud; return s_char(L, a, n); }
static slv_value cfn_format(slv *L, slv_value *a, int n, void *ud) { (void)ud; return s_format(L, a, n); }
static slv_value cfn_find(slv *L, slv_value *a, int n, void *ud) { (void)ud; return s_find(L, a, n); }
static slv_value cfn_match(slv *L, slv_value *a, int n, void *ud) { (void)ud; return s_match(L, a, n); }
static slv_value cfn_gmatch(slv *L, slv_value *a, int n, void *ud) { (void)ud; return s_gmatch(L, a, n); }
static slv_value cfn_gsub(slv *L, slv_value *a, int n, void *ud) { (void)ud; return s_gsub(L, a, n); }

void slv_register_string(slv *L) {
    slv_table *t = slv_table_new(L, 0, 16);
    slv_set_global(L, "string", (slv_value){.t = SLV_TABLE, .as.tab = t});

#define S(name, fn) { slv_value v; v.t = SLV_CFUNCTION; v.as.cf.f = fn; v.as.cf.ud = NULL; \
    slv_table_set(L, t, slv_str(name), v); }
    S("len", cfn_len);
    S("sub", cfn_sub);
    S("rep", cfn_rep);
    S("upper", cfn_upper);
    S("lower", cfn_lower);
    S("reverse", cfn_reverse);
    S("byte", cfn_byte);
    S("char", cfn_char);
    S("format", cfn_format);
    S("find", cfn_find);
    S("match", cfn_match);
    S("gmatch", cfn_gmatch);
    S("gsub", cfn_gsub);
#undef S

    /* the string metatable: string lib functions are methods on strings */
    slv_table *mt = slv_table_new(L, 0, 2);
    L->string_meta = mt;
    slv_value idx; idx.t = SLV_TABLE; idx.as.tab = t;
    slv_table_set(L, mt, slv_str("__index"), idx);
}
