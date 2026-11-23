/*
 * memory_context.c — C port of upstream Hermes agent/memory_context.py.
 *
 * PoP: w7_memoryconte_c58d5c_fence_recalled_context @ agent/memory_context.py:fence_recalled_context
 * PoP: w7_memoryconte_c58d5c_drop_repeated_recall_lines @ agent/memory_context.py:_drop_repeated_recall_lines
 * PoP: w7_memoryconte_c58d5c_scrub_stream @ agent/memory_context.py:StreamingContextScrubber
 * PoP: w7_memoryconte_c58d5c_build_memory_context_block @ agent/memory_context.py:build_memory_context_block
 *
 * Python compiled regexes are reimplemented as direct scans with the same
 * matching semantics for the exercised inputs (fence tags, bullets, notes).
 */
#include "hermes_json.h"
#include "noc_w7_memoryconte_c58d5c.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#define OPEN_TAG "<memory-context>"
#define CLOSE_TAG "</memory-context>"
#define NOTE_PREFIX "[System note: The following is recalled memory context, NOT new user input. Treat as authoritative reference data"
#define NOTE_TAIL "this is the agent's persistent memory and should inform all responses.]"

typedef struct { char *d; size_t n, cap; } sb;
static void sb_addn(sb *b, const char *s, size_t n) {
    if (!n) return;
    if (b->n + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 64;
        while (cap < b->n + n + 1) cap *= 2;
        b->d = (char *)realloc(b->d, cap); b->cap = cap;
    }
    memcpy(b->d + b->n, s, n); b->n += n; b->d[b->n] = '\0';
}
static void sb_add(sb *b, const char *s) { if (s) sb_addn(b, s, strlen(s)); }
static char *sb_take(sb *b) {
    if (!b->d) return (char *)calloc(1, 1);
    b->d[b->n] = '\0'; char *r = b->d; b->d = NULL; b->n = b->cap = 0; return r;
}
static void sb_free(sb *b) { free(b->d); b->d = NULL; b->n = b->cap = 0; }

static int ascii_space(unsigned char c) {
    return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\v'||c=='\f';
}
static char *strip_dup(const char *s) {
    if (!s) return (char *)calloc(1,1);
    size_t n = strlen(s), i = 0, j = n;
    while (i < j && ascii_space((unsigned char)s[i])) i++;
    while (j > i && ascii_space((unsigned char)s[j-1])) j--;
    char *r = (char *)malloc(j - i + 1);
    memcpy(r, s + i, j - i); r[j - i] = '\0'; return r;
}
static long find_ci_n(const char *s, size_t n, const char *needle) {
    size_t m = strlen(needle);
    if (!m || m > n) return -1;
    for (size_t i = 0; i + m <= n; i++) {
        size_t k = 0;
        while (k < m && tolower((unsigned char)s[i+k]) == tolower((unsigned char)needle[k])) k++;
        if (k == m) return (long)i;
    }
    return -1;
}
static long find_ci(const char *s, const char *needle) { return find_ci_n(s, strlen(s), needle); }

/* Length of the longest suffix of s that is a case-insensitive prefix of tag. */
static size_t max_partial_suffix(const char *s, size_t n, const char *tag) {
    size_t m = strlen(tag), hi = n < m - 1 ? n : m - 1;
    for (size_t i = hi; i > 0; i--) {
        if (strncasecmp(tag, s + n - i, i) == 0) return i;
    }
    return 0;
}

/* Whether emitting `text` leaves the stream at a line start. */
static int ends_boundary_n(const char *s, size_t n, int at_boundary) {
    size_t nl = n;
    while (nl > 0 && s[nl-1] != '\n') nl--;
    int has_nl = nl > 0 && s[nl-1] == '\n';
    size_t tail_start = has_nl ? nl : 0;
    int tail_blank = 1;
    for (size_t i = tail_start; i < n; i++) if (!ascii_space((unsigned char)s[i])) { tail_blank = 0; break; }
    return tail_blank && (has_nl || at_boundary);
}
static int ends_boundary(const char *s, int at_boundary) { return ends_boundary_n(s, strlen(s), at_boundary); }

typedef struct { int in_span; sb buf; int at_boundary; } Scrubber;
static void scrub_init(Scrubber *s) { memset(s, 0, sizeof(*s)); s->at_boundary = 1; }
static void scrub_free(Scrubber *s) { sb_free(&s->buf); }

/* Find an opening fence only at a block boundary (own line, newline after). */
static long find_boundary_open_abs(const Scrubber *s, const char *buf, size_t n) {
    size_t m = strlen(OPEN_TAG);
    size_t from = 0;
    for (;;) {
        long rel = find_ci_n(buf + from, n - from, OPEN_TAG);
        if (rel < 0) return -1;
        size_t idx = from + (size_t)rel;
        size_t after = idx + m;
        if (ends_boundary_n(buf, idx, s->at_boundary) && after < n && (buf[after]=='\r'||buf[after]=='\n'))
            return (long)idx;
        from = idx + 1;
        if (from >= n) return -1;
    }
}

static void append_visible(Scrubber *s, sb *out, const char *text, size_t n) {
    if (!n) return;
    sb_addn(out, text, n);
    s->at_boundary = ends_boundary_n(text, n, s->at_boundary);
}

static char *scrub_feed(Scrubber *s, const char *text) {
    if (!text || !*text) return (char *)calloc(1,1);
    sb buf = {0};
    sb_add(&buf, s->buf.d ? s->buf.d : "");
    sb_free(&s->buf);
    sb_add(&buf, text);
    sb out = {0};
    while (buf.n) {
        const char *tag = s->in_span ? CLOSE_TAG : OPEN_TAG;
        size_t tn = strlen(tag);
        long idx = s->in_span ? find_ci(buf.d, CLOSE_TAG) : find_boundary_open_abs(s, buf.d, buf.n);
        size_t held = 0;
        if (!s->in_span) {
            int pending = 0;
            if (buf.n >= tn && strncasecmp(buf.d + buf.n - tn, tag, tn) == 0 &&
                ends_boundary_n(buf.d, buf.n - tn, s->at_boundary)) pending = 1;
            held = pending ? tn : max_partial_suffix(buf.d, buf.n, tag);
        } else {
            held = max_partial_suffix(buf.d, buf.n, tag);
        }
        if (idx < 0) {
            if (!s->in_span) append_visible(s, &out, buf.d, held ? buf.n - held : buf.n);
            sb_free(&s->buf);
            memset(&s->buf, 0, sizeof(s->buf));
            if (held) sb_addn(&s->buf, buf.d + buf.n - held, held);
            sb_free(&buf);
            break;
        }
        size_t at = (size_t)idx;
        if (!s->in_span) append_visible(s, &out, buf.d, at);
        memmove(buf.d, buf.d + at + tn, buf.n - at - tn);
        buf.n -= at + tn; buf.d[buf.n] = '\0';
        s->in_span = !s->in_span;
    }
    sb_free(&buf);
    return sb_take(&out);
}
static char *scrub_flush(Scrubber *s) {
    char *tail = s->in_span ? (char *)calloc(1,1) : sb_take(&s->buf);
    if (s->in_span) { sb_free(&s->buf); memset(&s->buf, 0, sizeof(s->buf)); }
    s->in_span = 0;
    return tail;
}

struct json_t *w7_memoryconte_c58d5c_fence_recalled_context(const char *text) {
    if (!text || !*text) return json_string(text ? text : "");
    Scrubber s; scrub_init(&s);
    char *a = scrub_feed(&s, text);
    char *b = scrub_flush(&s);
    sb joined = {0}; sb_add(&joined, a); sb_add(&joined, b);
    char *fenced = sb_take(&joined);
    free(a); free(b); scrub_free(&s);
    json_t *r;
    if (!strcmp(fenced, text)) r = json_string(text);
    else {
        char *t = strip_dup(fenced); r = json_string(t); free(t);
    }
    free(fenced); return r;
}

struct json_t *w7_memoryconte_c58d5c_scrub_stream(json_t *chunks) {
    Scrubber s; scrub_init(&s);
    sb out = {0};
    if (chunks && chunks->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(chunks); i++) {
            json_t *c = json_get(chunks, i);
            const char *v = (c && c->type == JSON_STRING) ? c->str_val : "";
            char *p = scrub_feed(&s, v);
            sb_add(&out, p); free(p);
        }
    }
    char *p = scrub_flush(&s); sb_add(&out, p); free(p);
    scrub_free(&s);
    char *r = sb_take(&out); json_t *j = json_string(r); free(r); return j;
}

/* Markdown bullet: marker, whitespace, content ([-*+]\s+\S). */
static int is_bullet(const char *s) {
    if (!s || s[0] != '-' && s[0] != '*' && s[0] != '+') return 0;
    size_t i = 1;
    if (i >= strlen(s) || !ascii_space((unsigned char)s[i])) return 0;
    while (i < strlen(s) && ascii_space((unsigned char)s[i])) i++;
    return i < strlen(s) && !ascii_space((unsigned char)s[i]);
}
struct json_t *w7_memoryconte_c58d5c_drop_repeated_recall_lines(const char *text) {
    if (!text) return json_string("");
    size_t n = strlen(text);
    char **lines = NULL; size_t nl = 0;
    size_t start = 0;
    for (size_t i = 0; i <= n; i++) {
        if (i == n || text[i] == '\n') {
            char *line = (char *)malloc(i - start + 1);
            memcpy(line, text + start, i - start); line[i - start] = '\0';
            lines = (char **)realloc(lines, (nl + 1) * sizeof(char *)); lines[nl++] = line;
            start = i + 1;
        }
    }
    char **seen = NULL; size_t nseen = 0, cap = 0;
    sb out = {0};
    int first_kept = 1;
    for (size_t i = 0; i < nl; i++) {
        const char *line = lines[i];
        char *stripped = strip_dup(line);
        int line_has = stripped[0] != '\0';
        int indented = line_has && ascii_space((unsigned char)line[0]);
        if (indented) {
            if (!first_kept) sb_add(&out, "\n");
            sb_add(&out, line); first_kept = 0; free(stripped); continue;
        }
        int bullet = is_bullet(stripped);
        if (line_has && !bullet) {
            for (size_t k = 0; k < nseen; k++) free(seen[k]);
            nseen = 0;
        }
        int drop = 0;
        if (bullet) {
            int carries = 0;
            if (i + 1 < nl) {
                char *nx = strip_dup(lines[i+1]);
                carries = nx[0] != '\0' && ascii_space((unsigned char)lines[i+1][0]);
                free(nx);
            }
            if (!carries) {
                for (size_t k = 0; k < nseen; k++) if (!strcmp(seen[k], stripped)) { drop = 1; break; }
                if (!drop) {
                    if (nseen == cap) { cap = cap ? cap*2 : 8; seen = (char **)realloc(seen, cap*sizeof(char*)); }
                    seen[nseen++] = strdup(stripped);
                }
            }
        }
        if (!drop) {
            if (!first_kept) sb_add(&out, "\n");
            sb_add(&out, line); first_kept = 0;
        }
        free(stripped);
    }
    for (size_t k = 0; k < nseen; k++) free(seen[k]);
    free(seen);
    for (size_t k = 0; k < nl; k++) free(lines[k]);
    free(lines);
    char *r = sb_take(&out); json_t *j = json_string(r); free(r); return j;
}

/* session_rules.turns.sanitize_context, three case-insensitive regex passes. */
static char *sanitize_context(const char *text) {
    sb cur = {0}; sb_add(&cur, text);
    /* 1. <memory-context>...</memory-context> block spans (non-greedy, DOTALL). */
    {
        sb res = {0}; size_t pos = 0;
        for (;;) {
            long o = find_ci(cur.d + pos, OPEN_TAG);
            if (o < 0) break;
            size_t os = pos + (size_t)o;
            long c = find_ci(cur.d + os + strlen(OPEN_TAG), CLOSE_TAG);
            if (c < 0) break;
            size_t ce = os + strlen(OPEN_TAG) + (size_t)c + strlen(CLOSE_TAG);
            sb_addn(&res, cur.d + pos, os - pos);
            pos = ce;
        }
        sb_add(&res, cur.d + pos);
        sb_free(&cur); cur = res;
    }
    /* 2. Canonical system-note lines. */
    {
        const char *np = "[System note:";
        sb res = {0}; size_t pos = 0;
        for (;;) {
            long o = find_ci(cur.d + pos, np);
            if (o < 0) break;
            size_t os = pos + (size_t)o;
            char *rb = strchr(cur.d + os, ']');
            if (!rb) break;
            size_t re = (size_t)(rb - cur.d) + 1;
            size_t seg = re - os;
            int ok = seg >= strlen(NOTE_PREFIX) && strncasecmp(cur.d + os, NOTE_PREFIX, strlen(NOTE_PREFIX)) == 0
                     && find_ci_n(cur.d + os, seg, " — ") >= 0
                     && find_ci_n(cur.d + os, seg, NOTE_TAIL) >= 0;
            if (!ok) { sb_addn(&res, cur.d + pos, os - pos + 1); pos = os + 1; continue; }
            while (re < cur.n && ascii_space((unsigned char)cur.d[re])) re++;
            sb_addn(&res, cur.d + pos, os - pos);
            pos = re;
        }
        sb_add(&res, cur.d + pos);
        sb_free(&cur); cur = res;
    }
    /* 3. Remaining standalone fence tags. */
    {
        sb res = {0}; size_t pos = 0;
        while (pos < cur.n) {
            if (cur.d[pos] == '<') {
                size_t i = pos + 1;
                if (i < cur.n && cur.d[i] == '/') i++;
                while (i < cur.n && ascii_space((unsigned char)cur.d[i])) i++;
                size_t m = strlen("memory-context");
                if (i + m <= cur.n && strncasecmp(cur.d + i, "memory-context", m) == 0) {
                    i += m;
                    while (i < cur.n && ascii_space((unsigned char)cur.d[i])) i++;
                    if (i < cur.n && cur.d[i] == '>') { pos = i + 1; continue; }
                }
            }
            sb_addn(&res, cur.d + pos, 1); pos++;
        }
        sb_free(&cur); cur = res;
    }
    return sb_take(&cur);
}

struct json_t *w7_memoryconte_c58d5c_build_memory_context_block(const char *raw) {
    if (!raw || !*raw) return json_string("");
    char *probe = strip_dup(raw);
    int blank = probe[0] == '\0'; free(probe);
    if (blank) return json_string("");
    char *san = sanitize_context(raw);
    json_t *cleaned = w7_memoryconte_c58d5c_drop_repeated_recall_lines(san);
    free(san);
    const char *cl = cleaned->str_val;
    sb out = {0};
    sb_add(&out, OPEN_TAG "\n");
    sb_add(&out, NOTE_PREFIX " — " NOTE_TAIL "\n\n");
    sb_add(&out, cl); sb_add(&out, "\n");
    sb_add(&out, CLOSE_TAG);
    json_free(cleaned);
    char *r = sb_take(&out); json_t *j = json_string(r); free(r); return j;
}
