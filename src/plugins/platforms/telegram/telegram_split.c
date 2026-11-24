/*
 * telegram_split.c -- real C11 port of plugins/platforms/telegram/telegram_split.py.
 *
 * PoP: w22_telegramsp_8c7e95_strip_mdv2 @ plugins/platforms/telegram/telegram_split.py:_strip_mdv2
 * PoP: w22_telegramsp_8c7e95_quotable_text @ plugins/platforms/telegram/telegram_split.py:quotable_text
 * PoP: w22_telegramsp_8c7e95_anchor @ plugins/platforms/telegram/telegram_split.py:split_reply_anchor
 * PoP: w22_telegramsp_8c7e95_reply_file_max @ plugins/platforms/telegram/telegram_split.py:REPLY_FILE_MAX_BYTES
 *
 * `_CODE_REGION_RE`, `_strip_mdv2` and `quotable_text` are the regex chain
 * spelled out by hand (no regex library, no VLA); each pass reproduces one
 * re.sub's leftmost, non-overlapping, continue-after-the-match behaviour.
 * `split_reply_anchor`'s adapter is the seam: its attribute and its DM-topic
 * predicate are injected, so the short-circuit order is testable.
 */
#include "hermes_json.h"
#include "noc_w22_telegramsp_8c7e95.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- growable byte buffer ------------------------------------------------- */

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
    int oom;
} tsb;

static void tsb_reserve(tsb *b, size_t extra) {
    size_t need;
    char *nb;
    if (b->oom) return;
    need = b->len + extra + 1;
    if (need <= b->cap) return;
    while (b->cap < need) b->cap = b->cap ? b->cap * 2 : 128;
    nb = (char *)realloc(b->buf, b->cap);
    if (!nb) { b->oom = 1; return; }
    b->buf = nb;
}

static void tsb_putc(tsb *b, char c) {
    tsb_reserve(b, 1);
    if (b->oom) return;
    b->buf[b->len++] = c;
    b->buf[b->len] = '\0';
}

static void tsb_add(tsb *b, const char *s, size_t n) {
    if (!s || !n) return;
    tsb_reserve(b, n);
    if (b->oom) return;
    memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = '\0';
}

static char *tsb_take(tsb *b) {
    if (b->oom) { free(b->buf); return NULL; }
    if (!b->buf) {
        b->buf = (char *)malloc(1);
        if (!b->buf) return NULL;
        b->buf[0] = '\0';
    }
    return b->buf;
}

/* --- pass 1: the MarkdownV2 escape set ----------------------------------- */

/* `\\([_*\[\]()~`>#\+\-=|{}.!\\])` -- a backslash in front of any of these
 * punctuation characters is dropped. */
static int mdv2_escaped(unsigned char c) {
    switch (c) {
    case '_': case '*': case '[': case ']': case '(': case ')': case '~':
    case '`': case '>': case '#': case '+': case '-': case '=': case '|':
    case '{': case '}': case '.': case '!': case '\\':
        return 1;
    default:
        return 0;
    }
}

static char *mdv2_unescape(const char *s) {
    size_t n, i;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    {
        size_t w = 0;
        for (i = 0; i < n; i++) {
            if (s[i] == '\\' && i + 1 < n && mdv2_escaped((unsigned char)s[i + 1])) {
                out[w++] = s[i + 1];
                i++;
            } else {
                out[w++] = s[i];
            }
        }
        out[w] = '\0';
    }
    return out;
}

/* --- passes 2..6: one wrapped-markup unwrap ------------------------------- */

static int mdv2_word(unsigned char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') || c == '_';
}

/* `<open><body><close>` -> `<body>`, where open/close are `delim` repeated
 * (1 or 2) and body is a non-empty run without `delim`. With word_bounded the
 * open needs a non-\w (or start) before it and the close a non-\w (or end)
 * after it, which is the `(?<!\w)_([^_]+)_(?!\w)` italic rule. */
static char *mdv2_unwrap(const char *s, char delim, int doubled, int word_bounded) {
    tsb b;
    size_t n, p = 0;
    size_t width = doubled ? 2 : 1;

    memset(&b, 0, sizeof(b));
    if (!s) s = "";
    n = strlen(s);
    while (p < n) {
        size_t k, inner;
        int open_ok = (p + width <= n);
        size_t w;
        for (w = 0; open_ok && w < width; w++) {
            if (s[p + w] != delim) open_ok = 0;
        }
        if (open_ok && word_bounded && p > 0 && mdv2_word((unsigned char)s[p - 1])) open_ok = 0;
        inner = p + width;
        k = inner;
        if (open_ok) {
            while (k < n && s[k] != delim) k++;
            if (k == inner) open_ok = 0;               /* `+` needs one body char */
            else if (k >= n || s[k] != delim) open_ok = 0;
            else if (k + width > n) open_ok = 0;
            else {
                size_t j;
                for (j = 0; j < width; j++) {
                    if (s[k + j] != delim) open_ok = 0;
                }
                if (open_ok && word_bounded && k + width < n &&
                    mdv2_word((unsigned char)s[k + width])) open_ok = 0;
            }
        }
        if (open_ok) {
            tsb_add(&b, s + inner, k - inner);
            p = k + width;
        } else {
            tsb_putc(&b, s[p]);
            p++;
        }
    }
    return tsb_take(&b);
}

char *w22_telegramsp_8c7e95_strip_mdv2(const char *text) {
    char *s, *t;
    if (!text) text = "";
    s = mdv2_unescape(text);
    if (!s) return NULL;
    t = mdv2_unwrap(s, '*', 1, 0);   /* **bold** BEFORE MarkdownV2 *bold* */
    free(s); if (!t) return NULL;
    s = mdv2_unwrap(t, '*', 0, 0);   /* *bold* */
    free(t); if (!s) return NULL;
    t = mdv2_unwrap(s, '_', 0, 1);   /* _italic_, word-bounded */
    free(s); if (!t) return NULL;
    s = mdv2_unwrap(t, '~', 0, 0);   /* ~strike~ */
    free(t); if (!s) return NULL;
    t = mdv2_unwrap(s, '|', 1, 0);   /* ||spoiler|| */
    free(s);
    return t;
}

/* --- quotable_text ------------------------------------------------------- */

/* `re.sub(r"\\([`\\])", r"\1", code)` -- only a backslash and a backtick
 * unescape inside a MarkdownV2 code region. */
static int mdv2_code_escaped(unsigned char c) {
    return c == '`' || c == '\\';
}

static void code_unescape(const char *s, size_t n, tsb *b) {
    size_t i;
    for (i = 0; i < n; i++) {
        if (s[i] == '\\' && i + 1 < n && mdv2_code_escaped((unsigned char)s[i + 1])) {
            tsb_putc(b, s[i + 1]);
            i++;
        } else {
            tsb_putc(b, s[i]);
        }
    }
}

/* One `_CODE_REGION_RE` match at or after `from`: fenced ```...```(or end),
 * else an inline `...` span with no backtick or newline inside. */
static int code_region_at(const char *s, size_t n, size_t from, size_t *start, size_t *end) {
    size_t p;
    for (p = from; p < n; p++) {
        if (s[p] != '`') continue;
        if (p + 2 < n && s[p + 1] == '`' && s[p + 2] == '`') {
            size_t k;
            for (k = p + 3; k + 2 < n; k++) {
                if (s[k] == '`' && s[k + 1] == '`' && s[k + 2] == '`') break;
            }
            if (k + 2 < n && s[k] == '`' && s[k + 1] == '`' && s[k + 2] == '`') {
                *start = p;
                *end = k + 3;
            } else {
                *start = p;
                *end = n;                     /* `\Z`: an unclosed fence runs to the end */
            }
            return 1;
        }
        {
            size_t j = p + 1;
            while (j < n && s[j] != '`' && s[j] != '\n') j++;
            if (j < n && s[j] == '`') {
                *start = p;
                *end = j + 1;
                return 1;
            }
        }
    }
    return 0;
}

char *w22_telegramsp_8c7e95_quotable_text(const char *text, int mdv2) {
    tsb b;
    size_t n, pos = 0, scan = 0, start, end;

    memset(&b, 0, sizeof(b));
    if (!text) text = "";
    n = strlen(text);
    while (code_region_at(text, n, scan, &start, &end)) {
        char *head, *plain;
        size_t gap = start - pos;
        head = (char *)malloc(gap + 1);
        if (!head) { free(b.buf); return NULL; }
        memcpy(head, text + pos, gap);
        head[gap] = '\0';
        plain = w22_telegramsp_8c7e95_strip_mdv2(head);
        free(head);
        if (!plain) { free(b.buf); return NULL; }
        tsb_add(&b, plain, strlen(plain));
        free(plain);
        if (mdv2) code_unescape(text + start, end - start, &b);
        else tsb_add(&b, text + start, end - start);
        pos = end;
        scan = end;
    }
    {
        char *plain = w22_telegramsp_8c7e95_strip_mdv2(text + pos);
        if (!plain) { free(b.buf); return NULL; }
        tsb_add(&b, plain, strlen(plain));
        free(plain);
    }
    return tsb_take(&b);
}

/* --- split_reply_anchor -------------------------------------------------- */

json_t *w22_telegramsp_8c7e95_anchor(int anchor_multipart, int parts, int dm_topic,
                                     const json_t *reply_to) {
    json_t *out = json_object();
    int keep;
    int dm_checked = 0;

    /* getattr(adapter, "_anchor_multipart_replies", True) */
    keep = (anchor_multipart < 0) ? 1 : (anchor_multipart != 0);
    if (!keep) {
        if (parts <= 1) {
            keep = 1;              /* a single part is never a split reply */
        } else {
            dm_checked = 1;        /* the or-chain reaches the DM predicate */
            keep = dm_topic != 0;
        }
    }
    json_set(out, "reply_to", reply_to ? json_copy(reply_to) : json_null());
    json_set(out, "dm_checked", json_bool(dm_checked));
    if (!keep) json_set(out, "reply_to", json_null());
    return out;
}
