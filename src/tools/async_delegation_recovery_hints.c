/*
 * async_delegation_recovery_hints.c — C home for upstream Hermes tools/async_delegation_recovery_hints.py.
 * STATUS: PORTED (gate w25_asyncdeleg_9f3438) — differential oracle green.
 * PoP: w25_asyncdeleg_9f3438_tail_text @ tools/async_delegation_recovery_hints.py:transcript_tail
 * PoP: w25_asyncdeleg_9f3438_tails @ tools/async_delegation_recovery_hints.py:transcript_tails
 * PoP: w25_asyncdeleg_9f3438_git_hint @ tools/async_delegation_recovery_hints.py:git_state_hint
 * PoP: w25_asyncdeleg_9f3438_constants @ tools/async_delegation_recovery_hints.py:TAIL_LINES
 *
 * Forensics attached to an abandoned-delegation recovery event, bounded so they
 * never fail recovery: a missing file, a non-git cwd or a slow git all degrade
 * to nothing.  Reading the transcript window, running git and asking the kernel
 * whether the cwd is a directory are seams; ported here is the splitlines /
 * strip / truncate arithmetic and the one-line git snapshot format.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_asyncdeleg_9f3438.h"

static int ad_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

/* Bytes of the next str.splitlines() boundary at p (0 = no boundary here). */
static size_t ad_eol_len(const char *p) {
    unsigned char c = (unsigned char)p[0];
    if (c == '\r') return (unsigned char)p[1] == '\n' ? 2 : 1;
    if (c == '\n' || c == '\v' || c == '\f' || c == 0x1c || c == 0x1d || c == 0x1e) return 1;
    if (c == 0xc2 && (unsigned char)p[1] == 0x85) return 2;                       /* U+0085 */
    if (c == 0xe2 && (unsigned char)p[1] == 0x80) {
        unsigned char t = (unsigned char)p[2];
        if (t == 0xa8 || t == 0xa9) return 3;                                     /* U+2028/9 */
    }
    return 0;
}

typedef struct {
    const char *ptr;
    size_t len;
} ad_span;

/* Python str.splitlines() spans, in order. */
static ad_span *ad_splitlines(const char *s, size_t *count) {
    size_t cap = 8, n = 0;
    const char *p = s;
    ad_span *spans = (ad_span *)malloc(cap * sizeof(ad_span));

    *count = 0;
    if (!spans) return NULL;
    while (*p) {
        const char *start = p;
        size_t eol = 0;
        while (*p && !(eol = ad_eol_len(p))) p++;
        if (n == cap) {
            ad_span *grown = (ad_span *)realloc(spans, cap * 2 * sizeof(ad_span));
            if (!grown) { free(spans); return NULL; }
            spans = grown;
            cap *= 2;
        }
        spans[n].ptr = start;
        spans[n].len = (size_t)(p - start);
        n++;
        if (eol) p += eol;
    }
    *count = n;
    return spans;
}

/* Python s[i:] start offset with the negative-index rule. */
static size_t ad_slice_start(size_t len, long long i) {
    if (i < 0) {
        long long v = (long long)len + i;
        return v < 0 ? 0 : (size_t)v;
    }
    return (size_t)i > len ? len : (size_t)i;
}

/* Codepoints in a UTF-8 run (Python len()). */
static size_t ad_cp_len(const char *s, size_t len) {
    size_t i, n = 0;
    for (i = 0; i < len; i++)
        if (((unsigned char)s[i] & 0xc0) != 0x80) n++;
    return n;
}

/* Byte offset of the cp-th codepoint from the start. */
static size_t ad_cp_offset(const char *s, size_t len, size_t cp) {
    size_t i, n = 0;
    for (i = 0; i < len; i++) {
        if (((unsigned char)s[i] & 0xc0) != 0x80) {
            if (n == cp) return i;
            n++;
        }
    }
    return len;
}

/* Python s[-n:] over codepoints (n is the signed `chars`). */
static char *ad_last_cps(const char *s, size_t len, long long n) {
    size_t cps = ad_cp_len(s, len), start_cp, off;
    long long i = -n;
    char *out;

    if (i >= 0) start_cp = (size_t)i > cps ? cps : (size_t)i;
    else {
        long long v = (long long)cps + i;
        start_cp = v < 0 ? 0 : (size_t)v;
    }
    off = ad_cp_offset(s, len, start_cp);
    out = (char *)malloc(len - off + 1);
    if (!out) return NULL;
    memcpy(out, s + off, len - off);
    out[len - off] = '\0';
    return out;
}

json_t *w25_asyncdeleg_9f3438_tail_text(const json_t *raw, long long lines, long long chars) {
    const char *s;
    size_t len, nspans = 0, i, from, join_len = 0, start, end;
    ad_span *spans;
    char *joined, *q;
    json_t *out;

    if (!raw || raw->type != JSON_STRING) return json_null();
    s = raw->str_val ? raw->str_val : "";
    len = strlen(s);
    if (len == 0) return json_null();

    spans = ad_splitlines(s, &nspans);
    if (!spans) return json_null();

    /* raw.splitlines()[-lines:] */
    from = ad_slice_start(nspans, -lines);
    for (i = from; i < nspans; i++) {
        join_len += spans[i].len;
        if (i + 1 < nspans) join_len += 1; /* the "\n" separator */
    }
    joined = (char *)malloc(join_len + 1);
    if (!joined) { free(spans); return json_null(); }
    q = joined;
    for (i = from; i < nspans; i++) {
        memcpy(q, spans[i].ptr, spans[i].len);
        q += spans[i].len;
        if (i + 1 < nspans) *q++ = '\n';
    }
    *q = '\0';
    free(spans);

    /* .strip() */
    start = 0;
    end = join_len;
    while (start < end && ad_is_space(joined[start])) start++;
    while (end > start && ad_is_space(joined[end - 1])) end--;
    memmove(joined, joined + start, end - start);
    joined[end - start] = '\0';
    len = end - start;

    if (len == 0) { free(joined); return json_null(); }

    if ((long long)ad_cp_len(joined, len) > chars) {
        char *tail = ad_last_cps(joined, len, chars);
        size_t tl;
        char *cut;
        if (!tail) { free(joined); return json_null(); }
        tl = strlen(tail);
        cut = (char *)malloc(tl + 4);
        if (!cut) { free(tail); free(joined); return json_null(); }
        snprintf(cut, tl + 4, "\xE2\x80\xA6%s", tail); /* U+2026 HORIZONTAL ELLIPSIS */
        out = json_string(cut);
        free(cut);
        free(tail);
        free(joined);
        return out;
    }
    out = json_string(joined);
    free(joined);
    return out;
}

json_t *w25_asyncdeleg_9f3438_tails(const json_t *contents) {
    json_t *out = json_object();
    size_t i, n;

    if (!contents || contents->type != JSON_OBJECT) return out;
    n = json_object_size(contents);
    for (i = 0; i < n; i++) {
        const char *k = json_object_get_key_at(contents, i);
        json_t *tail;
        if (!k) continue;
        tail = w25_asyncdeleg_9f3438_tail_text(json_object_get_at(contents, i),
                                               W25_ASYNCDELEG_9F3438_TAIL_LINES,
                                               W25_ASYNCDELEG_9F3438_TAIL_CHARS);
        if (!tail || tail->type == JSON_NULL) { json_free(tail); continue; }
        json_set(out, k, tail);
    }
    return out;
}

static char *ad_strip(const char *s) {
    size_t start = 0, end = strlen(s);
    char *out;
    while (start < end && ad_is_space(s[start])) start++;
    while (end > start && ad_is_space(s[end - 1])) end--;
    out = (char *)malloc(end - start + 1);
    if (!out) return NULL;
    memcpy(out, s + start, end - start);
    out[end - start] = '\0';
    return out;
}

static int ad_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
}

json_t *w25_asyncdeleg_9f3438_git_hint(const json_t *head, const json_t *branch,
                                       const json_t *status, const json_t *cwd,
                                       const json_t *cwd_is_dir) {
    const char *head_s, *cwd_s;
    char *head_t, *branch_t, *out;
    size_t need;
    char dirty_buf[32];
    json_t *r;

    if (!head || head->type != JSON_STRING) return json_null(); /* git log failed: no checkout */
    head_s = head->str_val ? head->str_val : "";
    cwd_s = cwd && cwd->type == JSON_STRING ? cwd->str_val : "";
    if (!cwd_s[0]) return json_null();                           /* not cwd */
    if (!ad_truthy(cwd_is_dir)) return json_null();              /* not os.path.isdir(cwd) */

    {   /* branch = (run("rev-parse","--abbrev-ref","HEAD") or "?").strip() */
        const json_t *b = branch && branch->type == JSON_STRING ? branch : NULL;
        const char *raw = (b && b->str_val && b->str_val[0]) ? b->str_val : "?";
        branch_t = ad_strip(raw);
    }
    if (!branch_t) return json_null();

    if (status && status->type == JSON_STRING) {
        size_t nspans = 0;
        ad_span *spans = ad_splitlines(status->str_val ? status->str_val : "", &nspans);
        free(spans);
        snprintf(dirty_buf, sizeof(dirty_buf), "%llu", (unsigned long long)nspans);
    } else {
        snprintf(dirty_buf, sizeof(dirty_buf), "?");
    }

    head_t = ad_strip(head_s);
    if (!head_t) { free(branch_t); return json_null(); }
    need = strlen(branch_t) + strlen(head_t) + strlen(cwd_s) + strlen(dirty_buf) + 64;
    out = (char *)malloc(need);
    if (!out) { free(head_t); free(branch_t); return json_null(); }
    snprintf(out, need, "%s @ %s (%s uncommitted file(s)) in %s", branch_t, head_t, dirty_buf, cwd_s);
    free(head_t);
    free(branch_t);
    r = json_string(out);
    free(out);
    return r;
}

json_t *w25_asyncdeleg_9f3438_constants(void) {
    json_t *o = json_object();
    json_set(o, "tail_lines", json_number((double)W25_ASYNCDELEG_9F3438_TAIL_LINES));
    json_set(o, "tail_chars", json_number((double)W25_ASYNCDELEG_9F3438_TAIL_CHARS));
    json_set(o, "git_timeout_s", json_number((double)W25_ASYNCDELEG_9F3438_GIT_TIMEOUT_S));
    return o;
}
