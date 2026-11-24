/*
 * stream_consumer_directives.c -- real C11 port of the hold-back predicates of
 * upstream Hermes gateway/stream_consumer_directives.py.
 *
 * The live module compiles four end-anchored patterns and asks whether the tail
 * of a draft could still become a hidden attachment. Each pattern is a suffix
 * grammar (``\Z``/``$`` plus a lookbehind), so this port scans the tail directly
 * instead of carrying a regex engine -- same anchors, same boundaries, same
 * IGNORECASE handling.
 *
 * Two behaviours of Python's ``re`` are reproduced deliberately:
 *   * ``$`` also matches just before ONE trailing newline (the media/flag
 *     patterns use ``$``; the image pattern uses ``\Z``, which does not);
 *   * the media prefix branch backtracks its 0-3 quote run, so a lookup starts
 *     at every possible position before ``MEDIA``.
 * Non-ASCII lookbehind: a preceding non-ASCII character is treated as an
 * identifier character (Python's ``\w`` covers Unicode letters) except for the
 * two curly quotes the pattern names, which block the match as punctuation.
 *
 * What stays upstream: strip_image_tags_for_display() -- it calls
 * BasePlatformAdapter.extract_images() and rewrites the text between matches.
 *
 * PoP: w20_streamcons_6b51f8_pending_media @ gateway/stream_consumer_directives.py:_PENDING_MEDIA_RE
 * PoP: w20_streamcons_6b51f8_pending_flag @ gateway/stream_consumer_directives.py:_PENDING_FLAG_RE
 * PoP: w20_streamcons_6b51f8_pending_image @ gateway/stream_consumer_directives.py:_PENDING_IMAGE_RE
 * PoP: w20_streamcons_6b51f8_has_pending @ gateway/stream_consumer_directives.py:has_pending_media_directive
 */
#include "hermes_json.h"
#include "noc_w20_streamcons_6b51f8.h"

#include <stdlib.h>
#include <string.h>

static const char *const STREAM_FLAGS[2] = {"[[audio_as_voice]]", "[[as_document]]"};
static const char *const MEDIA_WORD = "media:";

static char *sc6b_dup(const char *s, size_t n) {
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n);
    out[n] = '\0';
    return out;
}

static int sc6b_is_alpha(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static int sc6b_is_digit(unsigned char c) { return c >= '0' && c <= '9'; }

/* [A-Za-z0-9] -- the media pattern's lookbehind class (ASCII only). */
static int sc6b_is_alnum(unsigned char c) { return sc6b_is_alpha(c) || sc6b_is_digit(c); }

/* [A-Za-z_] -- the flag pattern's word class. */
static int sc6b_is_wordch(unsigned char c) { return sc6b_is_alpha(c) || c == '_'; }

/* ASCII whitespace, Python \s for the characters a stream can carry. */
static int sc6b_is_space(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static unsigned char sc6b_lower(unsigned char c) {
    return (c >= 'A' && c <= 'Z') ? (unsigned char)(c - 'A' + 'a') : c;
}

/* `$` matches at the end of the string or just before a final newline. */
static size_t sc6b_dollar_len(const char *s, size_t n) {
    return (n > 0 && s[n - 1] == '\n') ? n - 1 : n;
}

int w20_streamcons_6b51f8_pending_media(const char *line) {
    const char *s = line ? line : "";
    size_t n = strlen(s), m = sc6b_dollar_len(s, n), i;
    size_t l;

    /* MEDIA:\s*(?:$|[`"'~/\\]|[A-Za-z](?::|$)) */
    for (i = 0; i + 6 <= m; i++) {
        size_t k;
        int ok = 1;
        int j;
        for (j = 0; j < 6; j++) {
            if (sc6b_lower((unsigned char)s[i + (size_t)j]) !=
                (unsigned char)MEDIA_WORD[j]) { ok = 0; break; }
        }
        if (!ok) continue;
        k = i + 6;
        while (k < m && sc6b_is_space((unsigned char)s[k])) k++;
        if (k >= m) return 1;
        if (strchr("`\"'~/\\", s[k]) != NULL) return 1;
        if (sc6b_is_alpha((unsigned char)s[k]) && (k + 1 == m || s[k + 1] == ':')) return 1;
    }

    /* (?<![A-Za-z0-9])[`"'*_]{0,3}M(?:E(?:D(?:I(?:A)?)?)?)?$ */
    for (l = 1; l <= 5 && l <= m; l++) {
        size_t p = m - l, q = 0, qq;
        int letters_ok = 1;
        size_t j;
        for (j = 0; j < l; j++) {
            if (sc6b_lower((unsigned char)s[p + j]) != (unsigned char)MEDIA_WORD[j]) {
                letters_ok = 0;
                break;
            }
        }
        if (!letters_ok) continue;
        while (q < 3 && p > q && strchr("`\"'*_", s[p - 1 - q]) != NULL) q++;
        for (qq = 0; qq <= q; qq++) {
            if (p < qq + 1) return 1;                       /* lookbehind at string start */
            if (!sc6b_is_alnum((unsigned char)s[p - 1 - qq])) return 1;
        }
    }
    return 0;
}

char *w20_streamcons_6b51f8_pending_flag(const char *line) {
    const char *s = line ? line : "";
    size_t n = strlen(s), m = sc6b_dollar_len(s, n);
    size_t i = m, j, start;

    /* \[\[?[A-Za-z_]*\]?$ */
    if (i > 0 && s[i - 1] == ']') i--;
    j = i;
    while (j > 0 && sc6b_is_wordch((unsigned char)s[j - 1])) j--;
    if (j == 0 || s[j - 1] != '[') return NULL;
    start = (j > 1 && s[j - 2] == '[') ? j - 2 : j - 1;
    return sc6b_dup(s + start, m - start);
}

int w20_streamcons_6b51f8_flag_opens_stream(const char *line) {
    char *flag = w20_streamcons_6b51f8_pending_flag(line);
    size_t flen;
    int i, hit = 0;
    if (!flag) return 0;
    flen = strlen(flag);
    for (i = 0; i < 2; i++) {
        if (!strncmp(STREAM_FLAGS[i], flag, flen)) { hit = 1; break; }
    }
    free(flag);
    return hit;
}

/* <(?:i(?:m(?:g(?:\s+(?:s(?:r(?:c(?:=["']?[^\s"'<>]*["']?\s*\/?)?)?)?)?)?)?)?)?$ */
static int sc6b_img_prefix(const char *s) {
    size_t n = strlen(s), i = 1;          /* s[0] is '<' */

    if (i < n && s[i] == 'i') {
        i++;
        if (i < n && s[i] == 'm') {
            i++;
            if (i < n && s[i] == 'g') {
                i++;
                if (i < n && sc6b_is_space((unsigned char)s[i])) {
                    while (i < n && sc6b_is_space((unsigned char)s[i])) i++;
                    if (i < n && s[i] == 's') {
                        i++;
                        if (i < n && s[i] == 'r') {
                            i++;
                            if (i < n && s[i] == 'c') {
                                i++;
                                if (i < n && s[i] == '=') {
                                    i++;
                                    if (i < n && (s[i] == '"' || s[i] == '\'')) i++;
                                    while (i < n && !sc6b_is_space((unsigned char)s[i]) &&
                                           s[i] != '"' && s[i] != '\'' && s[i] != '<' &&
                                           s[i] != '>') i++;
                                    if (i < n && (s[i] == '"' || s[i] == '\'')) i++;
                                    while (i < n && sc6b_is_space((unsigned char)s[i])) i++;
                                    if (i < n && s[i] == '/') i++;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return i == n;
}

int w20_streamcons_6b51f8_pending_image(const char *text) {
    const char *s = text ? text : "";
    size_t n = strlen(s), p;

    /* !\[[^\]]*(?:\](?:\([^)\s]*)?)?\Z */
    for (p = 0; p + 1 < n; p++) {
        const char *rest, *close, *after;
        if (s[p] != '!' || s[p + 1] != '[') continue;
        rest = s + p + 2;
        close = strchr(rest, ']');
        if (!close) return 1;
        after = close + 1;
        if (!*after) return 1;
        if (*after == '(') {
            const char *r = after + 1;
            int ok = 1;
            while (*r) {
                if (*r == ')' || sc6b_is_space((unsigned char)*r)) { ok = 0; break; }
                r++;
            }
            if (ok) return 1;
        }
    }

    /* (?<![\w)\]"'<U+201D><U+2019>!?.*_~`])!\Z */
    if (n > 0 && s[n - 1] == '!') {
        if (n == 1) return 1;
        /* the two curly quotes are the only non-ASCII members of the class */
        if (n >= 4 && (unsigned char)s[n - 4] == 0xE2 && (unsigned char)s[n - 3] == 0x80 &&
            ((unsigned char)s[n - 2] == 0x9D || (unsigned char)s[n - 2] == 0x99)) {
            return 0;
        }
        {
            unsigned char c = (unsigned char)s[n - 2];
            if (c >= 0x80) return 0;              /* \w over Unicode: a letter blocks */
            if (sc6b_is_alnum(c) || c == '_') return 0;
            if (strchr(")]\"'!?.*_~`", (char)c) != NULL) return 0;
            return 1;
        }
    }

    /* <(?:i(?:m(?:g...)?)?)?\Z */
    for (p = 0; p < n; p++) {
        if (s[p] == '<' && sc6b_img_prefix(s + p)) return 1;
    }

    /* </(?:i(?:m(?:g)?)?)?\Z */
    if (n >= 2 && s[n - 2] == '<' && s[n - 1] == '/') return 1;
    if (n >= 3 && !strcmp(s + n - 3, "</i")) return 1;
    if (n >= 4 && !strcmp(s + n - 4, "</im")) return 1;
    if (n >= 5 && !strcmp(s + n - 5, "</img")) return 1;
    return 0;
}

char *w20_streamcons_6b51f8_last_line(const char *cleaned) {
    const char *s = cleaned ? cleaned : "";
    const char *nl = strrchr(s, '\n');
    const char *start = nl ? nl + 1 : s;
    return sc6b_dup(start, strlen(start));
}

int w20_streamcons_6b51f8_has_pending(const char *cleaned) {
    char *line = w20_streamcons_6b51f8_last_line(cleaned);
    int hit;
    if (!line) return 0;
    hit = w20_streamcons_6b51f8_pending_media(line) ||
          w20_streamcons_6b51f8_pending_image(cleaned);
    if (!hit) hit = w20_streamcons_6b51f8_flag_opens_stream(line);
    free(line);
    return hit;
}

