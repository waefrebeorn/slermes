/*
 * adapter.c — C11 port of upstream plugins/platforms/email/adapter.py
 * (gates w28_adapter_ca7894_p1 .. _p3).
 *
 * PoP: w28_adapter_ca7894_normalize_security     @ plugins/platforms/email/adapter.py:_normalize_security
 * PoP: w28_adapter_ca7894_is_automated_sender    @ plugins/platforms/email/adapter.py:_is_automated_sender
 * PoP: w28_adapter_ca7894_safe_decode            @ plugins/platforms/email/adapter.py:_safe_decode
 * PoP: w28_adapter_ca7894_decode_header_value    @ plugins/platforms/email/adapter.py:_decode_header_value
 * PoP: w28_adapter_ca7894_strip_html             @ plugins/platforms/email/adapter.py:_strip_html
 * PoP: w28_adapter_ca7894_strip_comments         @ plugins/platforms/email/adapter.py:_strip_comments
 * PoP: w28_adapter_ca7894_extract_email_address  @ plugins/platforms/email/adapter.py:_extract_email_address
 * PoP: w28_adapter_ca7894_domain_of              @ plugins/platforms/email/adapter.py:_domain_of
 * PoP: w28_adapter_ca7894_domains_aligned        @ plugins/platforms/email/adapter.py:_domains_aligned
 * PoP: w28_adapter_ca7894_ar_clauses             @ plugins/platforms/email/adapter.py:_ar_clauses
 * PoP: w28_adapter_ca7894_auth_props             @ plugins/platforms/email/adapter.py:_auth_props
 * PoP: w28_adapter_ca7894_verify_sender_auth     @ plugins/platforms/email/adapter.py:_verify_sender_authentication
 * PoP: w28_adapter_ca7894_sender_accepted        @ plugins/platforms/email/adapter.py:EmailAdapter._sender_accepted
 * PoP: w28_adapter_ca7894_trim_seen_uids         @ plugins/platforms/email/adapter.py:EmailAdapter._trim_seen_uids
 * PoP: w28_adapter_ca7894_message_id_domain      @ plugins/platforms/email/adapter.py:EmailAdapter._message_id_domain
 * PoP: w28_adapter_ca7894_reply_threading        @ plugins/platforms/email/adapter.py:EmailAdapter._new_reply
 * PoP: w28_adapter_ca7894_attachment_meta        @ plugins/platforms/email/adapter.py:_extract_attachments
 *
 * Ported here is the decision core: charset/encoded-word decoding, HTML-to-text
 * shaping, From: identity parsing (parseaddr's strict single-mailbox path plus
 * its documented display-<addr> fallback and the reply-To: round-trip gate),
 * the RFC 8601 Authentication-Results grammar and its DMARC/SPF/DKIM verdict,
 * the pre-dispatch sender gate, and the dispatch/reply derivations.
 *
 * Every I/O seam stays in the caller: the IMAP socket (search/fetch/store),
 * SMTP, the credentials/env reads (_get_secret), the gateway allowlist verdict
 * (_is_sender_authorized), the attachment cache writers and the thread-context
 * dict all arrive as fixture values.  Nothing here opens a socket or a file.
 *
 * Domain notes (deliberate, exercised by the gate fixtures):
 *   - A-Z folding only on the identity/allowlist paths, matching upstream's
 *     _ASCII_LOWER (its identity path never folds U+212A).
 *   - _safe_decode carries utf-8, us-ascii and latin-1.  A label naming any
 *     other legacy codec (cp850, gb18030, cp949, ...) takes the same rung as an
 *     unknown label: the utf-8 fallback.  ASCII payloads decode identically
 *     under either reading.
 *   - _extract_email_address rejects route-addr / obsolete syntax exactly like
 *     an unparseable value instead of modelling it.
 *   - Bytes and text travel to this port as JSON strings (fixture payloads are
 *     base64), so an embedded NUL is outside the domain: every C string here is
 *     NUL-terminated, where the upstream str can carry U+0000.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

#include "noc_w28_adapter_ca7894_p1.h"
#include "noc_w28_adapter_ca7894_p2.h"
#include "noc_w28_adapter_ca7894_p3.h"

/* ── small helpers ────────────────────────────────────────────────────── */

static char *ea_ndup(const char *s, size_t n) {
    char *p = (char *)malloc(n + 1);
    if (!p) return NULL;
    if (n) memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

static char *ea_dup(const char *s) { return ea_ndup(s ? s : "", s ? strlen(s) : 0); }

static int ea_py_space(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static char ea_lower(unsigned char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : (char)c; }

static char *ea_lower_dup(const char *s) {
    char *p = ea_dup(s);
    size_t i;
    for (i = 0; p && p[i]; i++) p[i] = ea_lower((unsigned char)p[i]);
    return p;
}

static int ea_cmp_nocase(const char *a, const char *b, size_t n) {
    size_t i;
    for (i = 0; i < n; i++) {
        int ca = ea_lower((unsigned char)a[i]), cb = ea_lower((unsigned char)b[i]);
        if (ca != cb) return ca - cb;
        if (!a[i]) return 0;
    }
    return 0;
}

typedef struct { char *p; size_t n, cap; int oom; } sb_t;

static void sb_init(sb_t *b) { b->p = NULL; b->n = 0; b->cap = 0; b->oom = 0; }

static void sb_reserve(sb_t *b, size_t extra) {
    size_t cap;
    char *np;
    if (b->oom) return;
    if (b->n + extra + 1 <= b->cap) return;
    cap = b->cap ? b->cap : 64;
    while (cap < b->n + extra + 1) cap *= 2;
    np = (char *)realloc(b->p, cap);
    if (!np) { b->oom = 1; return; }
    b->p = np;
    b->cap = cap;
}

static void sb_putn(sb_t *b, const char *s, size_t n) {
    sb_reserve(b, n);
    if (b->oom) return;
    if (n) memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
}

static void sb_put(sb_t *b, const char *s) { if (s) sb_putn(b, s, strlen(s)); }
static void sb_putc(sb_t *b, char c) { sb_putn(b, &c, 1); }

static void sb_putcp(sb_t *b, unsigned cp) {
    char t[4];
    size_t k = 0;
    if (cp < 0x80) { t[k++] = (char)cp; }
    else if (cp < 0x800) {
        t[k++] = (char)(0xC0 | (cp >> 6));
        t[k++] = (char)(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        t[k++] = (char)(0xE0 | (cp >> 12));
        t[k++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        t[k++] = (char)(0x80 | (cp & 0x3F));
    } else {
        t[k++] = (char)(0xF0 | (cp >> 18));
        t[k++] = (char)(0x80 | ((cp >> 12) & 0x3F));
        t[k++] = (char)(0x80 | ((cp >> 6) & 0x3F));
        t[k++] = (char)(0x80 | (cp & 0x3F));
    }
    sb_putn(b, t, k);
}

static char *sb_take(sb_t *b) {
    char *r;
    if (!b->p) { b->n = b->cap = 0; return ea_dup(""); }
    r = b->p;
    b->p = NULL;
    b->n = b->cap = 0;
    return r;
}

static void sb_free(sb_t *b) { free(b->p); b->p = NULL; b->n = b->cap = 0; }

/* utf-8 decode, errors="replace" with the Unicode maximal-subpart rule that
 * CPython's utf-8 decoder implements (one U+FFFD per maximal invalid part). */
static char *ea_utf8_decode(const unsigned char *b, size_t n) {
    sb_t out;
    size_t i = 0;
    sb_init(&out);
    while (i < n) {
        unsigned char c = b[i];
        unsigned need, cp, lo = 0x80, hi = 0xBF, j;
        if (c < 0x80) { sb_putc(&out, (char)c); i++; continue; }
        if (c >= 0xC2 && c <= 0xDF) { need = 1; cp = c & 0x1F; }
        else if (c >= 0xE0 && c <= 0xEF) {
            need = 2; cp = c & 0x0F;
            if (c == 0xE0) lo = 0xA0; else if (c == 0xED) hi = 0x9F;
        } else if (c >= 0xF0 && c <= 0xF4) {
            need = 3; cp = c & 0x07;
            if (c == 0xF0) lo = 0x90; else if (c == 0xF4) hi = 0x8F;
        } else {
            sb_putcp(&out, 0xFFFD); i++; continue;
        }
        for (j = 1; j <= need; j++) {
            unsigned char cc;
            if (i + j >= n) break;
            cc = b[i + j];
            if (j == 1) { if (cc < lo || cc > hi) break; }
            else if (cc < 0x80 || cc > 0xBF) break;
            cp = (cp << 6) | (cc & 0x3F);
        }
        if (j > need) { sb_putcp(&out, cp); i += need + 1; }
        else { sb_putcp(&out, 0xFFFD); i += j; }
    }
    return sb_take(&out);
}

static char *ea_latin1_decode(const unsigned char *b, size_t n) {
    sb_t out;
    size_t i;
    sb_init(&out);
    for (i = 0; i < n; i++) sb_putcp(&out, b[i]);
    return sb_take(&out);
}

static char *ea_ascii_decode(const unsigned char *b, size_t n) {
    sb_t out;
    size_t i;
    sb_init(&out);
    for (i = 0; i < n; i++) {
        if (b[i] < 0x80) sb_putc(&out, (char)b[i]);
        else sb_putcp(&out, 0xFFFD);
    }
    return sb_take(&out);
}

/* ── charset labels: _CHARSET_ALIASES + the Python codec registry ─────── */

static char *ea_label_norm(const char *charset) {
    /* (charset or "utf-8").strip().strip("\"'").lower() or "utf-8" */
    const char *s = charset ? charset : "utf-8";
    size_t a = 0, b = strlen(s), i;
    char *out;
    while (a < b && ea_py_space((unsigned char)s[a])) a++;
    while (b > a && ea_py_space((unsigned char)s[b - 1])) b--;
    while (a < b && (s[a] == '"' || s[a] == '\'')) a++;
    while (b > a && (s[b - 1] == '"' || s[b - 1] == '\'')) b--;
    if (a == b) return ea_dup("utf-8");
    out = ea_ndup(s + a, b - a);
    for (i = 0; out[i]; i++) out[i] = ea_lower((unsigned char)out[i]);
    return out;
}

static const char *ea_codec_of(const char *label) {
    static const struct { const char *alias; const char *codec; } MAP[] = {
        { "unknown-8bit", "utf-8" }, { "unknown", "utf-8" }, { "x-unknown", "utf-8" },
        { "default", "utf-8" }, { "ansi_x3.110-1983", "latin-1" },
        { "utf-8", "utf-8" }, { "utf8", "utf-8" }, { "utf", "utf-8" }, { "u8", "utf-8" },
        { "utf-8-sig", "utf-8" },
        { "latin-1", "latin-1" }, { "latin1", "latin-1" }, { "latin", "latin-1" },
        { "l1", "latin-1" }, { "iso-8859-1", "latin-1" }, { "iso8859-1", "latin-1" },
        { "iso88591", "latin-1" }, { "8859", "latin-1" }, { "cp819", "latin-1" },
        { "ascii", "ascii" }, { "us-ascii", "ascii" }, { "usascii", "ascii" },
        { "646", "ascii" }, { "ansi_x3.4-1968", "ascii" }, { "cp367", "ascii" },
    };
    /* Python resolves a label through codecs.normalize_encoding(), which throws
     * away everything that is not alphanumeric: "latin-" and "latin - 1" and
     * "iso-8859" all reach the SAME codec as "latin-1". Matching the raw label
     * alone answered NULL for those and fell through to UTF-8. */
    static const struct { const char *norm; const char *codec; } NORMED[] = {
        { "latin1", "latin-1" }, { "latin", "latin-1" }, { "l1", "latin-1" },
        { "iso88591", "latin-1" }, { "iso8859", "latin-1" }, { "cp819", "latin-1" },
        { "utf8", "utf-8" }, { "utf", "utf-8" }, { "u8", "utf-8" }, { "utf8sig", "utf-8" },
        { "ascii", "ascii" }, { "usascii", "ascii" }, { "646", "ascii" }, { "cp367", "ascii" },
        { "unknown8bit", "utf-8" }, { "unknown", "utf-8" }, { "xunknown", "utf-8" },
        { "default", "utf-8" },
    };
    size_t i;
    char norm[64];
    size_t j = 0;
    for (i = 0; i < sizeof(MAP) / sizeof(MAP[0]); i++)
        if (!strcmp(MAP[i].alias, label)) return MAP[i].codec;
    for (i = 0; label[i] && j + 1 < sizeof norm; i++) {
        unsigned char c = (unsigned char)label[i];
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) norm[j++] = (char)c;
    }
    norm[j] = '\0';
    for (i = 0; i < sizeof(NORMED) / sizeof(NORMED[0]); i++)
        if (!strcmp(NORMED[i].norm, norm)) return NORMED[i].codec;
    return NULL;
}

static char *ea_safe_decode_bytes(const unsigned char *b, size_t n, const char *charset) {
    char *label = ea_label_norm(charset);
    const char *codec = ea_codec_of(label);
    char *out;
    if (codec && !strcmp(codec, "latin-1")) out = ea_latin1_decode(b, n);
    else if (codec && !strcmp(codec, "ascii")) out = ea_ascii_decode(b, n);
    else out = ea_utf8_decode(b, n);   /* utf-8 explicitly, or the LookupError rung */
    free(label);
    return out;
}

/* ── base64 / RFC 2047 ───────────────────────────────────────────────── */

static int ea_b64_val(int c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

/* binascii.a2b_base64(strict_mode=False): non-alphabet bytes are dropped, '='
 * ends the data, and a lone dangling sextet is an error. */
static int ea_b64_decode(const char *s, sb_t *out) {
    int quad[4];
    int qn = 0;
    size_t i;
    for (i = 0; s[i]; i++) {
        int v;
        if (s[i] == '=') break;
        v = ea_b64_val((unsigned char)s[i]);
        if (v < 0) continue;
        quad[qn++] = v;
        if (qn == 4) {
            sb_putc(out, (char)((quad[0] << 2) | (quad[1] >> 4)));
            sb_putc(out, (char)(((quad[1] & 0x0F) << 4) | (quad[2] >> 2)));
            sb_putc(out, (char)(((quad[2] & 0x03) << 6) | quad[3]));
            qn = 0;
        }
    }
    if (qn == 1) return -1;
    if (qn == 2) sb_putc(out, (char)((quad[0] << 2) | (quad[1] >> 4)));
    else if (qn == 3) {
        sb_putc(out, (char)((quad[0] << 2) | (quad[1] >> 4)));
        sb_putc(out, (char)(((quad[1] & 0x0F) << 4) | (quad[2] >> 2)));
    }
    return 0;
}

/* email.quoprimime.header_decode: '_' -> ' ', then =XX unquoting. */
static char *ea_q_decode(const char *s, size_t n) {
    sb_t out;
    size_t i;
    sb_init(&out);
    for (i = 0; i < n; i++) {
        char c = s[i];
        if (c == '_') { sb_putc(&out, ' '); continue; }
        if (c == '=' && i + 2 < n) {
            int h1 = -1, h2 = -1;
            char a = s[i + 1], b = s[i + 2];
            if (a >= '0' && a <= '9') h1 = a - '0';
            else if (a >= 'a' && a <= 'f') h1 = a - 'a' + 10;
            else if (a >= 'A' && a <= 'F') h1 = a - 'A' + 10;
            if (b >= '0' && b <= '9') h2 = b - '0';
            else if (b >= 'a' && b <= 'f') h2 = b - 'a' + 10;
            else if (b >= 'A' && b <= 'F') h2 = b - 'A' + 10;
            if (h1 >= 0 && h2 >= 0) {
                sb_putc(&out, (char)((h1 << 4) | h2));
                i += 2;
                continue;
            }
        }
        sb_putc(&out, c);
    }
    return sb_take(&out);
}

typedef struct { size_t start, end; char *charset; char enc; char *encoded; } ea_ew;

/* Next RFC 2047 encoded word in *line* at or after *from*. */
static int ea_find_ew(const char *line, size_t from, ea_ew *out) {
    size_t i;
    for (i = from; line[i]; i++) {
        size_t j, k;
        char enc;
        if (line[i] != '=' || line[i + 1] != '?') continue;
        j = i + 2;
        while (line[j] && line[j] != '?') j++;
        if (!line[j]) continue;
        enc = line[j + 1];
        if (!enc) continue;
        if (!(enc == 'q' || enc == 'Q' || enc == 'b' || enc == 'B')) continue;
        if (line[j + 2] != '?') continue;
        k = j + 3;
        while (line[k] && !(line[k] == '?' && line[k + 1] == '=')) k++;
        if (!line[k]) continue;
        out->start = i;
        out->end = k + 2;
        out->enc = ea_lower((unsigned char)enc);
        out->charset = ea_ndup(line + i + 2, j - i - 2);
        out->encoded = ea_ndup(line + j + 3, k - (j + 3));
        return 1;
    }
    return 0;
}

typedef struct { char *data; size_t n; int enc; char *charset; } ea_word;
typedef struct { ea_word *v; size_t n, cap; } ea_words;

static void ea_words_push(ea_words *w, char *data, size_t n, int enc, char *charset) {
    if (w->n == w->cap) {
        size_t cap = w->cap ? w->cap * 2 : 8;
        ea_word *nv = (ea_word *)realloc(w->v, cap * sizeof(*nv));
        if (!nv) { free(data); free(charset); return; }
        w->v = nv;
        w->cap = cap;
    }
    w->v[w->n].data = data;
    w->v[w->n].n = n;
    w->v[w->n].enc = enc;
    w->v[w->n].charset = charset;
    w->n++;
}

static void ea_words_free(ea_words *w) {
    size_t i;
    for (i = 0; i < w->n; i++) { free(w->v[i].data); free(w->v[i].charset); }
    free(w->v);
    w->v = NULL;
    w->n = w->cap = 0;
}

static void ea_words_del(ea_words *w, size_t at) {
    free(w->v[at].data);
    free(w->v[at].charset);
    memmove(&w->v[at], &w->v[at + 1], (w->n - at - 1) * sizeof(ea_word));
    w->n--;
}

/* Python str.splitlines() over the separators that occur in a header value. */
static int ea_next_line(const char *s, size_t start, size_t *line_end, size_t *next_start) {
    size_t i = start;
    if (!s[start]) return 0;
    while (s[i]) {
        char c = s[i];
        if (c == '\n') { *line_end = i; *next_start = i + 1; return 1; }
        if (c == '\r') {
            *line_end = i;
            *next_start = (s[i + 1] == '\n') ? i + 2 : i + 1;
            return 1;
        }
        if (c == '\v' || c == '\f' || c == '\x1c' || c == '\x1d' || c == '\x1e') {
            *line_end = i; *next_start = i + 1; return 1;
        }
        i++;
    }
    *line_end = i;
    *next_start = i;
    return 1;
}

static char *ea_strip(const char *s) {
    size_t a = 0, b = strlen(s);
    while (a < b && ea_py_space((unsigned char)s[a])) a++;
    while (b > a && ea_py_space((unsigned char)s[b - 1])) b--;
    return ea_ndup(s + a, b - a);
}

/* ── _decode_header_value (string form) ──────────────────────────────── */

static char *ea_decode_header_str(const char *raw) {
    ea_words words;
    size_t start = 0, line_end, next_start;
    int found = 0;

    memset(&words, 0, sizeof(words));

    /* ecre.search: any encoded word anywhere in the value. */
    for (start = 0; ea_next_line(raw, start, &line_end, &next_start); start = next_start) {
        ea_ew probe;
        size_t off = 0;
        char *line = ea_ndup(raw + start, line_end - start);
        while (ea_find_ew(line, off, &probe)) {
            found = 1;
            free(probe.charset);
            free(probe.encoded);
            off = probe.end;
        }
        free(line);
        if (found) break;
        if (next_start == start) break;
    }
    if (!found) return ea_dup(raw);

    for (start = 0; ea_next_line(raw, start, &line_end, &next_start); start = next_start) {
        char *line = ea_ndup(raw + start, line_end - start);
        size_t off = 0, prev = 0, len = strlen(line);
        int first = 1, any = 0;
        ea_ew ew;
        while (ea_find_ew(line, off, &ew)) {
            size_t un_start = prev, un_len = ew.start - prev;
            any = 1;
            if (first) {
                while (un_len && ea_py_space((unsigned char)line[un_start])) { un_start++; un_len--; }
                first = 0;
            }
            if (un_len) ea_words_push(&words, ea_ndup(line + un_start, un_len), un_len, 0, NULL);
            ea_words_push(&words, ew.encoded, strlen(ew.encoded), ew.enc, ew.charset);
            prev = ew.end;
            off = ew.end;
        }
        if (!any) {
            size_t un_start = 0, un_len = len;
            while (un_len && ea_py_space((unsigned char)line[un_start])) { un_start++; un_len--; }
            if (un_len) ea_words_push(&words, ea_ndup(line + un_start, un_len), un_len, 0, NULL);
        } else if (prev < len) {
            ea_words_push(&words, ea_ndup(line + prev, len - prev), len - prev, 0, NULL);
        }
        free(line);
        if (next_start == start) break;
    }

    /* Drop whitespace-only unencoded words sitting between two encoded ones. */
    {
        size_t n;
        for (n = words.n; n >= 3; n--) {
            size_t idx = n - 2;
            int only_ws = 1;
            size_t k;
            if (!words.v[n - 1].enc || !words.v[idx - 1].enc || words.v[idx].enc) continue;
            if (!words.v[idx].n) continue;
            for (k = 0; k < words.v[idx].n; k++)
                if (!ea_py_space((unsigned char)words.v[idx].data[k])) { only_ws = 0; break; }
            if (only_ws) ea_words_del(&words, idx);
        }
    }

    /* Decode each word; a bad base64 word makes upstream re-raise through
     * _decode_header_value, which hands the raw value back untouched. */
    {
        size_t i;
        for (i = 0; i < words.n; i++) {
            sb_t dec;
            if (!words.v[i].enc) continue;
            sb_init(&dec);
            if (words.v[i].enc == 'q') {
                char *out = ea_q_decode(words.v[i].data, words.v[i].n);
                sb_put(&dec, out);
                free(out);
            } else {
                char *padded = NULL;
                size_t len = words.v[i].n, paderr = len % 4;
                const char *src = words.v[i].data;
                if (paderr) {
                    size_t pad = 4 - paderr, k;
                    padded = (char *)malloc(len + pad + 1);
                    if (!padded) { sb_free(&dec); ea_words_free(&words); return ea_dup(raw); }
                    memcpy(padded, words.v[i].data, len);
                    for (k = 0; k < pad; k++) padded[len + k] = '=';
                    padded[len + pad] = '\0';
                    src = padded;
                }
                if (ea_b64_decode(src, &dec) < 0) {
                    free(padded);
                    sb_free(&dec);
                    ea_words_free(&words);
                    return ea_dup(raw);
                }
                free(padded);
            }
            free(words.v[i].data);
            words.v[i].data = sb_take(&dec);
            words.v[i].n = strlen(words.v[i].data);
        }
    }

    /* Collapse consecutive runs of the same charset, then join with " ". */
    {
        ea_words col;
        char *last = NULL;
        size_t last_n = 0;
        const char *last_cs = NULL;
        int last_set = 0;
        size_t i;
        char *result;
        memset(&col, 0, sizeof(col));
        for (i = 0; i < words.n; i++) {
            const char *cs = words.v[i].charset;
            if (!last_set) {
                last_n = words.v[i].n;
                last = ea_ndup(words.v[i].data, words.v[i].n);
                last_cs = cs;
                last_set = 1;
            } else if ((cs == NULL) != (last_cs == NULL) || (cs && last_cs && strcmp(cs, last_cs))) {
                ea_words_push(&col, last, last_n, 1, ea_dup(last_cs ? last_cs : NULL));
                last = ea_ndup(words.v[i].data, words.v[i].n);
                last_n = words.v[i].n;
                last_cs = cs;
            } else if (!last_cs) {
                char *merged = (char *)malloc(last_n + 1 + words.v[i].n + 1);
                if (merged) {
                    memcpy(merged, last, last_n);
                    merged[last_n] = ' ';
                    memcpy(merged + last_n + 1, words.v[i].data, words.v[i].n);
                    merged[last_n + 1 + words.v[i].n] = '\0';
                    free(last);
                    last = merged;
                    last_n = last_n + 1 + words.v[i].n;
                }
            } else {
                char *merged = (char *)malloc(last_n + words.v[i].n + 1);
                if (merged) {
                    memcpy(merged, last, last_n);
                    memcpy(merged + last_n, words.v[i].data, words.v[i].n);
                    merged[last_n + words.v[i].n] = '\0';
                    free(last);
                    last = merged;
                    last_n = last_n + words.v[i].n;
                }
            }
        }
        if (last_set) ea_words_push(&col, last, last_n, 1, ea_dup(last_cs ? last_cs : NULL));
        {
            sb_t joined;
            sb_init(&joined);
            for (i = 0; i < col.n; i++) {
                const char *cs = (col.v[i].charset && col.v[i].charset[0]) ? col.v[i].charset : NULL;
                char *piece = ea_safe_decode_bytes((const unsigned char *)col.v[i].data, col.v[i].n, cs);
                if (i) sb_putc(&joined, ' ');
                sb_put(&joined, piece);
                free(piece);
            }
            result = sb_take(&joined);
        }
        ea_words_free(&col);
        ea_words_free(&words);
        return result;
    }
}

/* ── _strip_comments (string form) ───────────────────────────────────── */

static char *ea_strip_comments_str(const char *text) {
    char *cur = ea_dup(text);
    size_t guard = strlen(cur) + 2;
    int changed = 1;
    while (changed && guard--) {
        sb_t b;
        size_t i = 0, n = strlen(cur);
        changed = 0;
        sb_init(&b);
        while (i < n) {
            if (cur[i] == '(') {
                size_t j = i + 1;
                while (j < n && cur[j] != '(' && cur[j] != ')') j++;
                if (j < n && cur[j] == ')') { sb_putc(&b, ' '); i = j + 1; changed = 1; continue; }
            }
            sb_putc(&b, cur[i]);
            i++;
        }
        free(cur);
        cur = sb_take(&b);
    }
    return cur;
}

/* ── gate p1 ─────────────────────────────────────────────────────────── */

/* PoP: _normalize_security @ plugins/platforms/email/adapter.py:_normalize_security */
json_t *w28_adapter_ca7894_normalize_security(const json_t *in) {
    static const struct { const char *raw; const char *out; } ALIASES[] = {
        { "tls", "tls" }, { "ssl", "tls" }, { "implicit", "tls" },
        { "starttls", "starttls" }, { "plain", "plain" }, { "none", "plain" },
    };
    const json_t *vj = in ? json_obj_get(in, "value") : NULL;
    const json_t *dj = in ? json_obj_get(in, "default") : NULL;
    const char *dflt = (dj && dj->type == JSON_STRING) ? dj->str_val : "tls";
    const char *value = "";
    sb_t raw;
    size_t i;
    const char *out = NULL;
    if (vj && vj->type == JSON_STRING) value = vj->str_val;
    else if (vj && vj->type == JSON_BOOL) value = vj->bool_val ? "True" : "False";
    sb_init(&raw);
    for (i = 0; value[i]; i++) {
        char c = ea_lower((unsigned char)value[i]);
        if (c == '-' || c == '_') continue;
        sb_putc(&raw, c);
    }
    {
        char *r = sb_take(&raw);
        char *t = ea_strip(r);
        free(r);
        for (i = 0; i < sizeof(ALIASES) / sizeof(ALIASES[0]); i++)
            if (!strcmp(ALIASES[i].raw, t)) { out = ALIASES[i].out; break; }
        free(t);
    }
    return json_string(out ? out : dflt);
}

/* PoP: _is_automated_sender @ plugins/platforms/email/adapter.py:_is_automated_sender */
json_t *w28_adapter_ca7894_is_automated_sender(const json_t *in) {
    static const char *NOREPLY[] = {
        "noreply", "no-reply", "no_reply", "donotreply", "do-not-reply", "mailer-daemon",
        "postmaster", "bounce", "notifications@", "automated@", "auto-confirm",
        "auto-reply", "automailer",
    };
    const json_t *headers = in ? json_obj_get(in, "headers") : NULL;
    const json_t *aj = in ? json_obj_get(in, "address") : NULL;
    const char *address = (aj && aj->type == JSON_STRING) ? aj->str_val : "";
    char *addr = ea_lower_dup(address);
    size_t i;
    int hit = 0;
    for (i = 0; !hit && i < sizeof(NOREPLY) / sizeof(NOREPLY[0]); i++)
        if (strstr(addr, NOREPLY[i])) hit = 1;
    free(addr);
    if (!hit && headers && headers->type == JSON_OBJECT) {
        static const char *HDRS[] = { "Auto-Submitted", "Precedence", "X-Auto-Response-Suppress", "List-Unsubscribe" };
        size_t h;
        for (h = 0; !hit && h < sizeof(HDRS) / sizeof(HDRS[0]); h++) {
            const json_t *v = json_obj_get(headers, HDRS[h]);
            const char *val;
            if (!v || v->type != JSON_STRING || !v->str_val[0]) continue;
            val = v->str_val;
            if (h == 0) {
                char *low = ea_lower_dup(val);
                hit = strcmp(low, "no") != 0;
                free(low);
            } else if (h == 1) {
                char *low = ea_lower_dup(val);
                hit = !strcmp(low, "bulk") || !strcmp(low, "list") || !strcmp(low, "junk");
                free(low);
            } else {
                hit = 1;
            }
        }
    }
    return json_bool(hit != 0);
}

/* PoP: _safe_decode @ plugins/platforms/email/adapter.py:_safe_decode */
json_t *w28_adapter_ca7894_safe_decode(const json_t *in) {
    const json_t *bj = in ? json_obj_get(in, "payload_b64") : NULL;
    const json_t *cj = in ? json_obj_get(in, "charset") : NULL;
    const char *b64 = (bj && bj->type == JSON_STRING) ? bj->str_val : "";
    const char *charset = (cj && cj->type == JSON_STRING) ? cj->str_val : NULL;
    sb_t raw;
    char *out;
    json_t *j;
    sb_init(&raw);
    if (b64[0]) ea_b64_decode(b64, &raw);
    out = ea_safe_decode_bytes((const unsigned char *)(raw.p ? raw.p : ""), raw.n, charset);
    sb_free(&raw);
    j = json_string(out);
    free(out);
    return j;
}

/* PoP: _decode_header_value @ plugins/platforms/email/adapter.py:_decode_header_value */
json_t *w28_adapter_ca7894_decode_header_value(const json_t *in) {
    const json_t *rj = in ? json_obj_get(in, "raw") : NULL;
    const char *raw = (rj && rj->type == JSON_STRING) ? rj->str_val : "";
    char *out = ea_decode_header_str(raw);
    json_t *j = json_string(out);
    free(out);
    return j;
}

/* ordered _HTML_SUBS passes */
static char *ea_sub_br(const char *s) {   /* the <br> / <br/> pattern */
    sb_t b;
    size_t i = 0, n = strlen(s);
    sb_init(&b);
    while (i < n) {
        if (s[i] == '<' && i + 2 < n && ea_lower((unsigned char)s[i + 1]) == 'b' &&
            ea_lower((unsigned char)s[i + 2]) == 'r') {
            size_t j = i + 3;
            while (j < n && ea_py_space((unsigned char)s[j])) j++;
            if (j < n && s[j] == '/') j++;
            if (j < n && s[j] == '>') { sb_putc(&b, '\n'); i = j + 1; continue; }
        }
        sb_putc(&b, s[i]);
        i++;
    }
    return sb_take(&b);
}

static char *ea_sub_open_p(const char *s) {   /* <p[^>]*> */
    sb_t b;
    size_t i = 0, n = strlen(s);
    sb_init(&b);
    while (i < n) {
        if (s[i] == '<' && i + 1 < n && ea_lower((unsigned char)s[i + 1]) == 'p') {
            size_t j = i + 2;
            while (j < n && s[j] != '>') j++;
            if (j < n) { sb_putc(&b, '\n'); i = j + 1; continue; }
        }
        sb_putc(&b, s[i]);
        i++;
    }
    return sb_take(&b);
}

static char *ea_sub_close_p(const char *s) {   /* </p> */
    sb_t b;
    size_t i = 0, n = strlen(s);
    sb_init(&b);
    while (i < n) {
        if (s[i] == '<' && i + 3 < n && s[i + 1] == '/' &&
            ea_lower((unsigned char)s[i + 2]) == 'p' && s[i + 3] == '>') {
            sb_putc(&b, '\n');
            i += 4;
            continue;
        }
        sb_putc(&b, s[i]);
        i++;
    }
    return sb_take(&b);
}

static char *ea_sub_all_tags(const char *s) {   /* <[^>]+> */
    sb_t b;
    size_t i = 0, n = strlen(s);
    sb_init(&b);
    while (i < n) {
        if (s[i] == '<') {
            size_t j = i + 1;
            while (j < n && s[j] != '>') j++;
            if (j < n && j > i + 1) { i = j + 1; continue; }
        }
        sb_putc(&b, s[i]);
        i++;
    }
    return sb_take(&b);
}

static char *ea_sub_literal(const char *s, const char *pat, const char *repl) {
    sb_t b;
    size_t i = 0, n = strlen(s), pn = strlen(pat);
    sb_init(&b);
    while (i < n) {
        if (i + pn <= n && !memcmp(s + i, pat, pn)) { sb_put(&b, repl); i += pn; continue; }
        sb_putc(&b, s[i]);
        i++;
    }
    return sb_take(&b);
}

static char *ea_collapse_newlines(const char *s) {   /* \n{3,} -> \n\n */
    sb_t b;
    size_t i = 0, n = strlen(s);
    sb_init(&b);
    while (i < n) {
        if (s[i] == '\n') {
            size_t j = i;
            while (j < n && s[j] == '\n') j++;
            if (j - i >= 3) sb_put(&b, "\n\n");
            else sb_putn(&b, s + i, j - i);
            i = j;
            continue;
        }
        sb_putc(&b, s[i]);
        i++;
    }
    return sb_take(&b);
}

/* PoP: _strip_html @ plugins/platforms/email/adapter.py:_strip_html */
json_t *w28_adapter_ca7894_strip_html(const json_t *in) {
    const json_t *hj = in ? json_obj_get(in, "html") : NULL;
    const char *html = (hj && hj->type == JSON_STRING) ? hj->str_val : "";
    char *a, *b, *c, *d, *e, *f, *g, *h;
    json_t *j;
    a = ea_sub_br(html);
    b = ea_sub_open_p(a);                     free(a);
    c = ea_sub_close_p(b);                    free(b);
    d = ea_sub_all_tags(c);                   free(c);
    e = ea_sub_literal(d, "&nbsp;", " ");     free(d);
    f = ea_sub_literal(e, "&amp;", "&");      free(e);
    g = ea_sub_literal(f, "&lt;", "<");       free(f);
    h = ea_sub_literal(g, "&gt;", ">");       free(g);
    a = ea_collapse_newlines(h);              free(h);
    b = ea_strip(a);                          free(a);
    j = json_string(b);
    free(b);
    return j;
}

/* PoP: _strip_comments @ plugins/platforms/email/adapter.py:_strip_comments */
json_t *w28_adapter_ca7894_strip_comments(const json_t *in) {
    const json_t *tj = in ? json_obj_get(in, "text") : NULL;
    const char *text = (tj && tj->type == JSON_STRING) ? tj->str_val : "";
    char *out = ea_strip_comments_str(text);
    json_t *j = json_string(out);
    free(out);
    return j;
}

/* ── gate p2 ─────────────────────────────────────────────────────────── */

/* The display phrase with its quoted-string spans removed. */
static char *ea_display_unquoted(const char *s) {
    sb_t b;
    size_t i, n = strlen(s);
    sb_init(&b);
    for (i = 0; i < n; i++) {
        if (s[i] == '"') {
            size_t j = i + 1;
            while (j < n && s[j] != '"') { if (s[j] == '\\' && j + 1 < n) j++; j++; }
            if (j < n) { i = j; continue; }
            sb_putc(&b, '"');
            continue;
        }
        sb_putc(&b, s[i]);
    }
    return sb_take(&b);
}

/* parseaddr(strict)'s single-mailbox path, reduced to the addr-spec.  "" means
 * the value is not one mailbox (multi-address, group, phrase only, garbage). */
static char *ea_parseaddr_addr(const char *value) {
    size_t n = strlen(value), i;
    int depth = 0, quoted = 0;
    long lt = -1, gt = -1;
    int bad = 0;

    /* _check_parenthesis: parens must balance outside quoted real names. */
    for (i = 0; i < n; i++) {
        char c = value[i];
        if (c == '\\' && i + 1 < n) { i++; continue; }
        if (quoted) { if (c == '"') quoted = 0; continue; }
        if (c == '"') { quoted = 1; continue; }
        if (c == '(') depth++;
        else if (c == ')') { depth--; if (depth < 0) return ea_dup(""); }
    }
    if (depth != 0) return ea_dup("");

    depth = 0;
    quoted = 0;
    for (i = 0; i < n && !bad; i++) {
        char c = value[i];
        if (quoted) {
            if (c == '\\' && i + 1 < n) i++;
            else if (c == '"') quoted = 0;
            continue;
        }
        if (c == '"') { quoted = 1; continue; }
        if (depth > 0) { if (c == '(') depth++; else if (c == ')') depth--; continue; }
        if (c == '(') { depth = 1; continue; }
        if (lt >= 0) {
            if (c == '<') { bad = 1; break; }
            if (c == '>') { gt = (long)i; break; }
            continue;
        }
        if (c == ',' || c == ':' || c == ';') { bad = 1; break; }
        if (c == '<') { lt = (long)i; continue; }
        if (c == ')' || c == '>') { bad = 1; break; }
    }
    if (quoted) bad = 1;
    if (bad) return ea_dup("");

    if (lt >= 0) {
        char *display, *unq, *addr, *prefix, *tmp;
        size_t k;
        /* parseaddr answers the address up to the END of a value whose angle-addr
         * was never closed: parseaddr("John <j@x") is ("John", "j@x"). */
        if (gt < 0) gt = (long)n;
        for (k = (size_t)gt + 1; k < n; k++)
            if (!ea_py_space((unsigned char)value[k])) return ea_dup("");  /* text after > */
        prefix = ea_ndup(value, (size_t)lt);
        unq = ea_display_unquoted(prefix);
        free(prefix);
        tmp = ea_strip_comments_str(unq);
        free(unq);
        display = ea_strip(tmp);
        free(tmp);
        addr = ea_strip(ea_ndup(value + lt + 1, (size_t)gt - (size_t)lt - 1));
        /* A phrase holding "@" is not a phrase: parseaddr("a@x <a@x>") is ("", ""),
         * even when the two sides are the same text. */
        if (strchr(display, '@')) { free(display); free(addr); return ea_dup(""); }
        free(display);
        return addr;
    }

    /* No angle-addr: a bare addr-spec.  CPython's parser drops CFWS, so what is
     * left of the value after comments and whitespace is the address. */
    {
        char *nocomment = ea_strip_comments_str(value);
        sb_t b;
        sb_init(&b);
        for (i = 0; nocomment[i]; i++)
            if (!ea_py_space((unsigned char)nocomment[i])) sb_putc(&b, nocomment[i]);
        free(nocomment);
        return sb_take(&b);
    }
}

/* PoP: _extract_email_address @ plugins/platforms/email/adapter.py:_extract_email_address */
json_t *w28_adapter_ca7894_extract_email_address(const json_t *in) {
    const json_t *rj = in ? json_obj_get(in, "raw") : NULL;
    const char *raw = (rj && rj->type == JSON_STRING) ? rj->str_val : "";
    sb_t unfolded;
    char *value, *addr, *low;
    size_t i, nparen = 0;
    json_t *j;

    /* re.sub(r"\r?\n[ \t]+", " ", raw) */
    sb_init(&unfolded);
    for (i = 0; raw[i]; i++) {
        if (raw[i] == '\r' && raw[i + 1] == '\n' &&
            (raw[i + 2] == ' ' || raw[i + 2] == '\t')) {
            size_t k = i + 2;
            while (raw[k] == ' ' || raw[k] == '\t') k++;
            sb_putc(&unfolded, ' ');
            i = k - 1;
            continue;
        }
        if (raw[i] == '\n' && (raw[i + 1] == ' ' || raw[i + 1] == '\t')) {
            size_t k = i + 1;
            while (raw[k] == ' ' || raw[k] == '\t') k++;
            sb_putc(&unfolded, ' ');
            i = k - 1;
            continue;
        }
        sb_putc(&unfolded, raw[i]);
    }
    value = sb_take(&unfolded);
    if (strlen(value) > 2048) { free(value); return json_string(""); }
    for (i = 0; value[i]; i++) if (value[i] == '(') nparen++;
    if (nparen > 64) { free(value); return json_string(""); }

    addr = ea_parseaddr_addr(value);
    if (!addr[0]) {
        /* _SINGLE_BRACKET_FROM_RE.fullmatch(value): ([^"<>]*)<([^<>\s]+)>\s* */
        const char *lt = NULL, *gt = NULL, *p;
        free(addr);
        addr = NULL;
        for (p = value; *p; p++) {
            if (*p == '<') { lt = p; break; }
            if (*p == '"' || *p == '>') break;
        }
        if (*p && lt) {
            const char *q = lt + 1;
            int ok = 1;
            while (*q && *q != '>') {
                if (*q == '<' || ea_py_space((unsigned char)*q)) { ok = 0; break; }
                q++;
            }
            if (ok && *q == '>') {
                gt = q;
                for (p = gt + 1; *p; p++)
                    if (!ea_py_space((unsigned char)*p)) { ok = 0; break; }
                if (ok) {
                    char *bracketed = ea_ndup(lt + 1, (size_t)(gt - lt) - 1);
                    char *prefix = ea_ndup(value, (size_t)(lt - value));
                    char *tmp = ea_strip_comments_str(prefix);
                    char *display = ea_strip(tmp);
                    int spec_ok = 1, ch_ok = 1, at_ok = 1;
                    size_t k;
                    const char *at = strchr(bracketed, '@');
                    free(tmp);
                    free(prefix);
                    if (!at || at == bracketed || !at[1]) spec_ok = 0;
                    if (strchr(bracketed, ',') || strchr(bracketed, ';') ||
                        strchr(bracketed, ':')) spec_ok = 0;
                    if (at && strchr(at + 1, '@')) spec_ok = 0;
                    for (k = 0; display[k]; k++)
                        if (display[k] == ';' || display[k] == ':' || display[k] == '(' ||
                            display[k] == ')') { ch_ok = 0; break; }
                    if (strchr(display, '@')) {
                        char *dl = ea_lower_dup(display), *bl = ea_lower_dup(bracketed);
                        at_ok = !strcmp(dl, bl);
                        free(dl);
                        free(bl);
                    }
                    if (spec_ok && ch_ok && at_ok) addr = ea_dup(bracketed);
                    free(bracketed);
                    free(display);
                }
            }
        }
        if (!addr) addr = ea_dup("");
    }

    low = ea_strip(addr);
    free(addr);
    free(value);
    for (i = 0; low[i]; i++) low[i] = ea_lower((unsigned char)low[i]);
    {
        /* getaddresses([addr]) == [("", addr)] for an addr-spec: exactly one
         * '@', a bare domain, no quoted/comment/list/group punctuation. */
        const char *at = strchr(low, '@');
        int ok = 1;
        if (!at || at == low || !at[1]) ok = 0;
        if (strchr(low, '"') || strchr(low, '(') || strchr(low, ')') ||
            strchr(low, ',') || strchr(low, ';') || strchr(low, ':') ||
            strchr(low, '[') || strchr(low, '<') || strchr(low, '>')) ok = 0;
        if (ok) {
            const char *q;
            if (strchr(at + 1, '@')) ok = 0;
            else if (ea_py_space((unsigned char)low[0]) ||
                     ea_py_space((unsigned char)at[-1])) ok = 0;
            else {
                for (q = at + 1; *q && ok; q++)
                    if (ea_py_space((unsigned char)*q)) ok = 0;
            }
        }
        if (!ok) { free(low); return json_string(""); }
    }
    j = json_string(low);
    free(low);
    return j;
}

/* PoP: _domain_of @ plugins/platforms/email/adapter.py:_domain_of */
json_t *w28_adapter_ca7894_domain_of(const json_t *in) {
    const json_t *aj = in ? json_obj_get(in, "address") : NULL;
    const char *address = (aj && aj->type == JSON_STRING) ? aj->str_val : "";
    const char *at = strrchr(address, '@');
    char *part = ea_strip(at ? at + 1 : address);   /* rpartition("@")[2] */
    size_t i;
    json_t *j;
    for (i = 0; part[i]; i++) part[i] = ea_lower((unsigned char)part[i]);
    j = json_string(part);
    free(part);
    return j;
}

/* PoP: _domains_aligned @ plugins/platforms/email/adapter.py:_domains_aligned */
json_t *w28_adapter_ca7894_domains_aligned(const json_t *in) {
    const json_t *aj = in ? json_obj_get(in, "a") : NULL;
    const json_t *bj = in ? json_obj_get(in, "b") : NULL;
    char *a = ea_strip((aj && aj->type == JSON_STRING) ? aj->str_val : "");
    char *b = ea_strip((bj && bj->type == JSON_STRING) ? bj->str_val : "");
    size_t la, lb, i;
    int ok = 0;
    for (i = 0; a[i]; i++) a[i] = ea_lower((unsigned char)a[i]);
    for (i = 0; b[i]; i++) b[i] = ea_lower((unsigned char)b[i]);
    la = strlen(a);
    while (la && a[la - 1] == '.') a[--la] = '\0';
    lb = strlen(b);
    while (lb && b[lb - 1] == '.') b[--lb] = '\0';
    if (la && lb) {
        if (!strcmp(a, b)) ok = 1;
        else if (la > lb && a[la - lb - 1] == '.' && !strcmp(a + la - lb, b)) ok = 1;
        else if (lb > la && b[lb - la - 1] == '.' && !strcmp(b + lb - la, a)) ok = 1;
    }
    free(a);
    free(b);
    return json_bool(ok != 0);
}

/* _ar_clauses: split on ';' outside quoted-strings/comments; NULL=unbalanced. */
static char **ea_split_clauses(const char *text, size_t *count) {
    size_t n = strlen(text), i = 0, cap = 4, cn = 0;
    char **out = (char **)malloc(cap * sizeof(char *));
    int quoted = 0;
    size_t depth = 0;
    sb_t cur;
    if (!out) return NULL;
    sb_init(&cur);
    while (i < n) {
        char c = text[i];
        if (c == '\\' && (quoted || depth)) {
            if (quoted && i + 1 < n) sb_putn(&cur, text + i, 2);
            i += 2;
            continue;
        }
        if (quoted) {
            if (c == '"') quoted = 0;
            sb_putc(&cur, c);
        } else if (depth) {
            if (c == '(') depth++;
            else if (c == ')') {
                depth--;
                if (!depth) sb_putc(&cur, ' ');
            }
        } else if (c == '(') {
            depth = 1;
        } else if (c == '"') {
            quoted = 1;
            sb_putc(&cur, c);
        } else if (c == ')') {
            goto unbalanced;
        } else if (c == ';') {
            if (cn == cap) {
                char **nv = (char **)realloc(out, cap * 2 * sizeof(char *));
                if (!nv) goto unbalanced;
                out = nv;
                cap *= 2;
            }
            out[cn++] = sb_take(&cur);
            sb_init(&cur);
        } else {
            sb_putc(&cur, c);
        }
        i++;
    }
    if (quoted || depth) goto unbalanced;
    if (cn == cap) {
        char **nv = (char **)realloc(out, (cap + 1) * sizeof(char *));
        if (!nv) goto unbalanced;
        out = nv;
    }
    out[cn++] = sb_take(&cur);
    *count = cn;
    return out;

unbalanced:
    sb_free(&cur);
    for (i = 0; i < cn; i++) free(out[i]);
    free(out);
    return NULL;
}

static void ea_free_clauses(char **clauses, size_t count) {
    size_t i;
    if (!clauses) return;
    for (i = 0; i < count; i++) free(clauses[i]);
    free(clauses);
}

/* PoP: _ar_clauses @ plugins/platforms/email/adapter.py:_ar_clauses */
json_t *w28_adapter_ca7894_ar_clauses(const json_t *in) {
    const json_t *tj = in ? json_obj_get(in, "text") : NULL;
    const char *text = (tj && tj->type == JSON_STRING) ? tj->str_val : "";
    size_t count = 0, i;
    char **clauses = ea_split_clauses(text, &count);
    json_t *arr;
    if (!clauses) return json_null();
    arr = json_array();
    for (i = 0; i < count; i++) json_append(arr, json_string(clauses[i]));
    ea_free_clauses(clauses, count);
    return arr;
}

typedef struct { char *name; char *val; } ea_prop;
typedef struct { ea_prop *v; size_t n, cap; } ea_props;

static void ea_props_push(ea_props *p, char *name, char *val) {
    if (p->n == p->cap) {
        size_t cap = p->cap ? p->cap * 2 : 8;
        ea_prop *nv = (ea_prop *)realloc(p->v, cap * sizeof(*nv));
        if (!nv) { free(name); free(val); return; }
        p->v = nv;
        p->cap = cap;
    }
    p->v[p->n].name = name;
    p->v[p->n].val = val;
    p->n++;
}

static void ea_props_free(ea_props *p) {
    size_t i;
    for (i = 0; i < p->n; i++) { free(p->v[i].name); free(p->v[i].val); }
    free(p->v);
    p->v = NULL;
    p->n = p->cap = 0;
}

/* length of a quoted-string starting at s (0 when not one / unterminated) */
static size_t ea_quoted_len(const char *s) {
    size_t i;
    if (s[0] != '"') return 0;
    for (i = 1; s[i]; i++) {
        if (s[i] == '\\') { if (!s[i + 1]) return 0; i++; continue; }
        if (s[i] == '"') return i + 1;
    }
    return 0;
}

/* _AUTH_PROP_RE.findall, reduced to the (property, value) pairs it yields. */
static void ea_scan_props(const char *s, ea_props *out) {
    static const char *NAMES[] = { "header.from", "header.d", "smtp.mailfrom", "smtp.from", "envelope-from" };
    size_t i = 0;
    while (s[i]) {
        size_t k, hit = 0;
        for (k = 0; k < sizeof(NAMES) / sizeof(NAMES[0]); k++) {
            size_t ln = strlen(NAMES[k]), q;
            if (ea_cmp_nocase(s + i, NAMES[k], ln)) continue;
            q = i + ln;
            while (s[q] && ea_py_space((unsigned char)s[q])) q++;
            if (s[q] != '=') continue;
            q++;
            while (s[q] && ea_py_space((unsigned char)s[q])) q++;
            {
                size_t vstart = q, vend = q;
                while (s[vend]) {
                    size_t ql;
                    if ((ql = ea_quoted_len(s + vend))) { vend += ql; continue; }
                    if (ea_py_space((unsigned char)s[vend]) || s[vend] == '"' || s[vend] == ';') break;
                    vend++;
                }
                if (vend == vstart) continue;
                {
                    char *val = ea_ndup(s + vstart, vend - vstart);
                    char *name = ea_ndup(NAMES[k], ln);
                    size_t a = 0, b = strlen(val), z;
                    for (z = 0; name[z]; z++) name[z] = ea_lower((unsigned char)name[z]);
                    while (a < b && val[a] == '"') a++;
                    while (b > a && val[b - 1] == '"') b--;
                    memmove(val, val + a, b - a);
                    val[b - a] = '\0';
                    ea_props_push(out, name, val);
                }
                i = vend;
                hit = 1;
                break;
            }
        }
        if (hit) continue;
        {
            /* the pattern's other branch: a token consumed whole, never reported */
            size_t vend = i, start = i;
            while (s[vend]) {
                size_t ql;
                if ((ql = ea_quoted_len(s + vend))) { vend += ql; continue; }
                if (ea_py_space((unsigned char)s[vend]) || s[vend] == '"') break;
                vend++;
            }
            i = (vend == start) ? start + 1 : vend;
        }
    }
}

/* PoP: _auth_props @ plugins/platforms/email/adapter.py:_auth_props */
json_t *w28_adapter_ca7894_auth_props(const json_t *in) {
    const json_t *tj = in ? json_obj_get(in, "text") : NULL;
    const char *text = (tj && tj->type == JSON_STRING) ? tj->str_val : "";
    ea_props props;
    json_t *arr = json_array();
    size_t i;
    memset(&props, 0, sizeof(props));
    ea_scan_props(text, &props);
    for (i = 0; i < props.n; i++) {
        json_t *pair = json_array();
        json_append(pair, json_string(props.v[i].name));
        json_append(pair, json_string(props.v[i].val));
        json_append(arr, pair);
    }
    ea_props_free(&props);
    return arr;
}

/* ── gate p3 ─────────────────────────────────────────────────────────── */

/* Relaxed DMARC alignment of two domain strings. */
static int ea_domains_aligned_raw(const char *a_in, const char *b_in) {
    char *a = ea_strip(a_in), *b = ea_strip(b_in);
    size_t la, lb, i;
    int ok = 0;
    for (i = 0; a[i]; i++) a[i] = ea_lower((unsigned char)a[i]);
    for (i = 0; b[i]; i++) b[i] = ea_lower((unsigned char)b[i]);
    la = strlen(a);
    while (la && a[la - 1] == '.') a[--la] = '\0';
    lb = strlen(b);
    while (lb && b[lb - 1] == '.') b[--lb] = '\0';
    if (la && lb) {
        if (!strcmp(a, b)) ok = 1;
        else if (la > lb && a[la - lb - 1] == '.' && !strcmp(a + la - lb, b)) ok = 1;
        else if (lb > la && b[lb - la - 1] == '.' && !strcmp(b + lb - la, a)) ok = 1;
    }
    free(a);
    free(b);
    return ok;
}

/* upstream's aligned(): every named property's domain must align. */
static int ea_props_aligned(const ea_props *props, const char **names, size_t nnames,
                            const char *from_domain, int required) {
    size_t i, hits = 0;
    int all = 1;
    for (i = 0; i < props->n; i++) {
        size_t k;
        int named = 0;
        const char *at, *part;
        char *dom;
        for (k = 0; k < nnames; k++) if (!strcmp(props->v[i].name, names[k])) named = 1;
        if (!named) continue;
        hits++;
        at = strrchr(props->v[i].val, '@');
        part = at ? at + 1 : props->v[i].val;
        dom = ea_strip(part);
        if (!ea_domains_aligned_raw(dom, from_domain)) all = 0;
        free(dom);
    }
    return ((hits > 0) || !required) && all;
}

typedef struct { char *result; ea_props props; } ea_verdict;

static json_t *ea_verdict_json(int ok, const char *reason) {
    json_t *out = json_array();
    json_append(out, json_bool(ok));
    json_append(out, json_string(reason));
    return out;
}

/* PoP: _verify_sender_authentication @ plugins/platforms/email/adapter.py:_verify_sender_authentication */
json_t *w28_adapter_ca7894_verify_sender_auth(const json_t *in) {
    const json_t *fj = in ? json_obj_get(in, "from_addr") : NULL;
    const json_t *aj = in ? json_obj_get(in, "authserv_id") : NULL;
    const json_t *headers = in ? json_obj_get(in, "headers") : NULL;
    const char *from_addr = (fj && fj->type == JSON_STRING) ? fj->str_val : "";
    const char *authserv = (aj && aj->type == JSON_STRING) ? aj->str_val : "";
    const char *first = NULL;
    char *trusted = NULL, *from_domain = NULL;
    char **clauses = NULL;
    size_t nclauses = 0, i;
    ea_verdict dmarc[16], spf[16], dkim[32];
    size_t ndmarc = 0, nspf = 0, ndkim = 0;
    json_t *out = NULL;

    memset(dmarc, 0, sizeof(dmarc));
    memset(spf, 0, sizeof(spf));
    memset(dkim, 0, sizeof(dkim));

    {
        const char *at = strrchr(from_addr, '@');
        const char *part = at ? at + 1 : from_addr;
        char *low = ea_lower_dup(part);
        from_domain = ea_strip(low);
        free(low);
    }
    if (!from_domain[0]) {
        out = ea_verdict_json(0, "missing From domain");
        goto done;
    }
    if (headers && headers->type == JSON_ARRAY && json_len(headers) > 0) {
        const json_t *h0 = json_get(headers, 0);
        if (h0 && h0->type == JSON_STRING) first = h0->str_val;
    }
    if (!first) {
        out = ea_verdict_json(0, "no Authentication-Results header");
        goto done;
    }
    {
        /* trusted = " ".join(str(headers[0]).split()) */
        sb_t t;
        size_t k;
        int pending = 0;
        sb_init(&t);
        for (k = 0; first[k]; k++) {
            if (ea_py_space((unsigned char)first[k])) { pending = 1; continue; }
            if (pending && t.n) sb_putc(&t, ' ');
            pending = 0;
            sb_putc(&t, first[k]);
        }
        trusted = sb_take(&t);
    }
    clauses = ea_split_clauses(trusted, &nclauses);
    if (!clauses) {
        out = ea_verdict_json(0, "unbalanced quote or comment in Authentication-Results");
        goto done;
    }
    if (!authserv[0]) {
        out = ea_verdict_json(0, "authserv-id is not configured; refusing to trust Authentication-Results");
        goto done;
    }
    {
        char *c0 = ea_strip(clauses[0]);
        size_t k;
        int eq;
        /* Only the HEADER's authserv token is lower-cased; the configured id is
         * compared as given. So "mx.example.com;..." against "MX.EXAMPLE.COM" is
         * UNTRUSTED, while "MX.EXAMPLE.COM;..." against "mx.example.com" is trusted. */
        for (k = 0; c0[k]; k++) c0[k] = ea_lower((unsigned char)c0[k]);
        eq = !strcmp(c0, authserv ? authserv : "");
        free(c0);
        if (!eq) {
            out = ea_verdict_json(0, "no Authentication-Results from trusted authserv-id");
            goto done;
        }
    }
    for (i = 0; i < nclauses; i++) {
        const char *c = clauses[i];
        size_t k = 0;
        const char *method = NULL;
        size_t mlen = 0;
        while (c[k] && ea_py_space((unsigned char)c[k])) k++;
        {
            static const char *METHODS[] = { "dmarc", "dkim", "spf" };
            size_t m;
            for (m = 0; m < 3; m++) {
                size_t ln = strlen(METHODS[m]);
                if (!ea_cmp_nocase(c + k, METHODS[m], ln) && !isalnum((unsigned char)c[k + ln])) {
                    method = METHODS[m];
                    mlen = ln;
                    break;
                }
            }
        }
        if (!method) continue;
        {
            size_t q = k + mlen;
            char *result;
            while (c[q] && ea_py_space((unsigned char)c[q])) q++;
            if (c[q] != '=') continue;
            q++;
            while (c[q] && ea_py_space((unsigned char)c[q])) q++;
            if (!isalpha((unsigned char)c[q])) continue;
            {
                size_t rs = q;
                while (isalpha((unsigned char)c[q])) q++;
                {
                    char *raw_result = ea_ndup(c + rs, q - rs);
                    result = ea_lower_dup(raw_result);
                    free(raw_result);
                }
            }
            {
                ea_props props;
                memset(&props, 0, sizeof(props));
                ea_scan_props(c, &props);
                if (!strcmp(method, "dmarc") && ndmarc < 16) {
                    dmarc[ndmarc].result = result;
                    dmarc[ndmarc].props = props;
                    ndmarc++;
                } else if (!strcmp(method, "spf") && nspf < 16) {
                    spf[nspf].result = result;
                    spf[nspf].props = props;
                    nspf++;
                } else if (!strcmp(method, "dkim") && ndkim < 32) {
                    dkim[ndkim].result = result;
                    dkim[ndkim].props = props;
                    ndkim++;
                } else {
                    free(result);
                    ea_props_free(&props);
                }
            }
        }
    }
    if (ndmarc > 1) {
        out = ea_verdict_json(0, "ambiguous dmarc result");
        goto done;
    }
    {
        static const char *FROM[] = { "header.from" };
        for (i = 0; i < ndmarc; i++)
            if (!strcmp(dmarc[i].result, "pass") &&
                ea_props_aligned(&dmarc[i].props, FROM, 1, from_domain, 0)) {
                out = ea_verdict_json(1, "dmarc=pass");
                goto done;
            }
    }
    if (nspf == 1 && !strcmp(spf[0].result, "pass")) {
        static const char *SPFF[] = { "smtp.mailfrom", "smtp.from", "envelope-from" };
        if (ea_props_aligned(&spf[0].props, SPFF, 3, from_domain, 1)) {
            out = ea_verdict_json(1, "spf=pass aligned");
            goto done;
        }
    }
    for (i = 0; i < ndkim; i++) {
        static const char *HD[] = { "header.d" };
        static const char *HF[] = { "header.from" };
        const char **names = HD;
        size_t z;
        int has_hd = 0;
        for (z = 0; z < dkim[i].props.n; z++)
            if (!strcmp(dkim[i].props.v[z].name, "header.d")) has_hd = 1;
        if (!has_hd) names = HF;
        if (!strcmp(dkim[i].result, "pass") &&
            ea_props_aligned(&dkim[i].props, names, 1, from_domain, 1)) {
            out = ea_verdict_json(1, "dkim=pass aligned");
            goto done;
        }
    }
    {
        /* f"authentication failed ({trusted[:120]})" -- sliced by characters */
        sb_t reason;
        size_t k = 0, chars = 0;
        char *r;
        sb_init(&reason);
        sb_put(&reason, "authentication failed (");
        while (trusted[k] && chars < 120) {
            unsigned char c = (unsigned char)trusted[k];
            if (c < 0x80) { sb_putc(&reason, (char)c); k++; chars++; }
            else {
                size_t len = (c >= 0xF0) ? 4 : (c >= 0xE0) ? 3 : (c >= 0xC0) ? 2 : 1;
                size_t jj;
                for (jj = 0; jj < len && trusted[k + jj]; jj++) sb_putc(&reason, trusted[k + jj]);
                k += len;
                chars++;
            }
        }
        sb_put(&reason, ")");
        r = sb_take(&reason);
        out = ea_verdict_json(0, r);
        free(r);
    }

done:
    for (i = 0; i < ndmarc; i++) { free(dmarc[i].result); ea_props_free(&dmarc[i].props); }
    for (i = 0; i < nspf; i++) { free(spf[i].result); ea_props_free(&spf[i].props); }
    for (i = 0; i < ndkim; i++) { free(dkim[i].result); ea_props_free(&dkim[i].props); }
    ea_free_clauses(clauses, nclauses);
    free(trusted);
    free(from_domain);
    return out;
}

/* PoP: _trim_seen_uids @ plugins/platforms/email/adapter.py:EmailAdapter._trim_seen_uids */
json_t *w28_adapter_ca7894_trim_seen_uids(const json_t *in) {
    const json_t *uids = in ? json_obj_get(in, "uids") : NULL;
    double dmax = in ? json_get_num(in, "max", 2000.0) : 2000.0;
    size_t max = (dmax < 0) ? 0 : (size_t)dmax;
    json_t *arr = json_array();
    const char **vals;
    long *keys;
    size_t n = 0, i, j, m = 0;

    if (uids && uids->type == JSON_ARRAY) n = json_len(uids);
    vals = (const char **)calloc(n ? n : 1, sizeof(*vals));
    keys = (long *)calloc(n ? n : 1, sizeof(*keys));
    if (!vals || !keys) { free(vals); free(keys); return arr; }
    for (i = 0; i < n; i++) {
        const json_t *v = json_get(uids, i);
        const char *s = (v && v->type == JSON_STRING) ? v->str_val : "";
        size_t k;
        int dup = 0;
        for (k = 0; k < m; k++) if (!strcmp(vals[k], s)) { dup = 1; break; }
        if (dup) continue;
        vals[m] = s;
        keys[m] = strtol(s, NULL, 10);
        m++;
    }
    n = m;
    for (i = 1; i < n; i++) {
        const char *sv = vals[i];
        long kv = keys[i];
        j = i;
        while (j > 0 && keys[j - 1] > kv) {
            vals[j] = vals[j - 1];
            keys[j] = keys[j - 1];
            j--;
        }
        vals[j] = sv;
        keys[j] = kv;
    }
    {
        size_t start = 0;
        if (n > max) start = (max / 2) ? n - (max / 2) : 0;  /* x[-0:] keeps all */
        for (i = start; i < n; i++) json_append(arr, json_string(vals[i]));
    }
    free(vals);
    free(keys);
    return arr;
}

/* PoP: _message_id_domain @ plugins/platforms/email/adapter.py:EmailAdapter._message_id_domain */
json_t *w28_adapter_ca7894_message_id_domain(const json_t *in) {
    const json_t *aj = in ? json_obj_get(in, "address") : NULL;
    const char *address = (aj && aj->type == JSON_STRING) ? aj->str_val : "";
    const char *part = "";
    if (strchr(address, '@')) part = strrchr(address, '@') + 1;
    if (!part[0]) return json_string("localhost");
    return json_string(part);
}

/* PoP: _new_reply @ plugins/platforms/email/adapter.py:EmailAdapter._new_reply */
json_t *w28_adapter_ca7894_reply_threading(const json_t *in) {
    const json_t *ctx = in ? json_obj_get(in, "ctx") : NULL;
    const json_t *sj = ctx ? json_obj_get(ctx, "subject") : NULL;
    const json_t *mj = ctx ? json_obj_get(ctx, "message_id") : NULL;
    const json_t *rj = in ? json_obj_get(in, "reply_to") : NULL;
    const char *ctx_subject = (sj && sj->type == JSON_STRING) ? sj->str_val : NULL;
    const char *ctx_msgid = (mj && mj->type == JSON_STRING) ? mj->str_val : NULL;
    const char *reply_to = (rj && rj->type == JSON_STRING) ? rj->str_val : NULL;
    const char *subject_src = ctx_subject ? ctx_subject : "Hermes Agent";
    /* An empty message id is falsy in Python, so the threading headers are None
     * rather than "" -- the port was emitting empty strings. */
    const char *original = (reply_to && reply_to[0]) ? reply_to
                           : ((ctx_msgid && ctx_msgid[0]) ? ctx_msgid : NULL);
    json_t *out = json_object();
    sb_t subj;
    sb_init(&subj);
    if (!strncmp(subject_src, "Re:", 3)) sb_put(&subj, subject_src);
    else { sb_put(&subj, "Re: "); sb_put(&subj, subject_src); }
    {
        char *s = sb_take(&subj);
        json_set(out, "subject", json_string(s));
        free(s);
    }
    json_set(out, "in_reply_to", original ? json_string(original) : json_null());
    json_set(out, "references", original ? json_string(original) : json_null());
    return out;
}

/* PoP: _extract_attachments @ plugins/platforms/email/adapter.py:_extract_attachments */
json_t *w28_adapter_ca7894_attachment_meta(const json_t *in) {
    const json_t *dj = in ? json_obj_get(in, "disposition") : NULL;
    const json_t *tj = in ? json_obj_get(in, "content_type") : NULL;
    const json_t *sj = in ? json_obj_get(in, "subtype") : NULL;
    const json_t *fj = in ? json_obj_get(in, "filename") : NULL;
    const char *disposition = (dj && dj->type == JSON_STRING) ? dj->str_val : "";
    const char *content_type = (tj && tj->type == JSON_STRING) ? tj->str_val : "text/plain";
    const char *subtype = (sj && sj->type == JSON_STRING) ? sj->str_val : "";
    const char *filename = (fj && fj->type == JSON_STRING) ? fj->str_val : NULL;
    int has_payload = in ? json_get_bool(in, "has_payload", 1) : 1;
    int skip = in ? json_get_bool(in, "skip", 0) : 0;
    char *name;
    const char *kind = "document";
    const char *base, *dot;
    json_t *out;

    if (skip) return json_null();
    if (!strstr(disposition, "attachment")) {
        if (!strstr(disposition, "inline")) return json_null();
        if (!strcmp(content_type, "text/plain") || !strcmp(content_type, "text/html")) return json_null();
    }
    if (!has_payload) return json_null();
    if (filename) name = ea_decode_header_str(filename);
    else {
        sb_t b;
        sb_init(&b);
        sb_put(&b, "attachment.");
        sb_put(&b, subtype[0] ? subtype : "bin");
        name = sb_take(&b);
    }
    base = strrchr(name, '/');
    base = base ? base + 1 : name;
    dot = strrchr(base, '.');
    if (dot && dot != base) {
        static const char *EXTS[] = { ".jpg", ".jpeg", ".png", ".gif", ".webp" };
        size_t i;
        for (i = 0; i < sizeof(EXTS) / sizeof(EXTS[0]); i++) {
            size_t ln = strlen(EXTS[i]), have = strlen(dot), k;
            int eq = 1;
            if (have != ln) continue;
            for (k = 0; k < ln; k++) if (ea_lower((unsigned char)dot[k]) != EXTS[i][k]) eq = 0;
            if (eq) { kind = "image"; break; }
        }
    }
    out = json_object();
    json_set(out, "filename", json_string(name));
    json_set(out, "type", json_string(kind));
    json_set(out, "media_type", json_string(content_type));
    free(name);
    return out;
}

/* PoP: EmailAdapter._sender_accepted @ plugins/platforms/email/adapter.py:EmailAdapter._sender_accepted */
json_t *w28_adapter_ca7894_sender_accepted(const json_t *in) {
    const json_t *sj = in ? json_obj_get(in, "sender") : NULL;
    const json_t *aj = in ? json_obj_get(in, "address") : NULL;
    const json_t *msg = in ? json_obj_get(in, "msg") : NULL;
    const json_t *env = in ? json_obj_get(in, "env") : NULL;
    const json_t *extra = in ? json_obj_get(in, "extra") : NULL;
    const json_t *authz = in ? json_obj_get(in, "authorized") : NULL;
    const char *sender = (sj && sj->type == JSON_STRING) ? sj->str_val : "";
    const char *address = (aj && aj->type == JSON_STRING) ? aj->str_val : "";
    int require_auth = in ? json_get_bool(in, "require_auth", 1) : 1;
    int authenticated = msg ? json_get_bool(msg, "sender_authenticated", 0) : 0;
    const json_t *rj = msg ? json_obj_get(msg, "auth_reason") : NULL;
    const char *auth_reason = (rj && rj->type == JSON_STRING) ? rj->str_val : NULL;
    json_t *out = json_object();
    /* upstream compares the RAW sender_addr: only the allowlist entries and
     * self._address are folded, so a cased From: matches nothing. */
    char *sender_key = ea_dup(sender);
    char *addr_low = ea_lower_dup(address);
    char **listed = NULL;
    size_t nlisted = 0, cap = 0;
    size_t i;
    int is_listed = 0, granted = 0, answered;
    char *allowed_raw;

    if (!strcmp(sender_key, addr_low)) {
        free(sender_key); free(addr_low);
        json_set(out, "accepted", json_bool(0));
        json_set(out, "log", json_string(""));
        return out;
    }
    free(addr_low);
    {
        /* _is_automated_sender(sender_addr, {}) */
        json_t *probe = json_object();
        json_t *auto_j;
        json_set(probe, "address", json_string(sender_key));
        auto_j = w28_adapter_ca7894_is_automated_sender(probe);
        json_free(probe);
        if (json_bool_value(auto_j)) {
            json_free(auto_j);
            free(sender_key);
            json_set(out, "accepted", json_bool(0));
            json_set(out, "log", json_string("debug"));
            return out;
        }
        json_free(auto_j);
    }

    {
        static const char *NAMES[2] = { "EMAIL_ALLOWED_USERS", "GATEWAY_ALLOWED_USERS" };
        const json_t *arj = env ? json_obj_get(env, NAMES[0]) : NULL;
        allowed_raw = ea_strip((arj && arj->type == JSON_STRING) ? arj->str_val : "");
        {
            size_t which;
            for (which = 0; which < 2; which++) {
                const json_t *rawj = env ? json_obj_get(env, NAMES[which]) : NULL;
                const char *raw_j = (rawj && rawj->type == JSON_STRING) ? rawj->str_val : "";
                const char *p = raw_j;
                while (ea_py_space((unsigned char)*p)) p++;
                if (*p == '[') {
                    /* decode_json_list_literal: a leading '[' means a JSON list */
                    json_t *j = json_parse(raw_j, NULL);
                    if (j && j->type == JSON_ARRAY) {
                        size_t k;
                        for (k = 0; k < json_len(j); k++) {
                            const json_t *e = json_get(j, k);
                            char *item = NULL;
                            if (e && e->type == JSON_STRING) item = ea_strip(e->str_val);
                            else if (e && e->type == JSON_NUMBER) {
                                char buf[64];
                                snprintf(buf, sizeof(buf), "%g", e->num_val);
                                item = ea_strip(buf);
                            } else if (e && e->type == JSON_BOOL) item = ea_dup(e->bool_val ? "True" : "False");
                            if (item && item[0]) {
                                size_t z;
                                for (z = 0; item[z]; z++) item[z] = ea_lower((unsigned char)item[z]);
                                if (nlisted == cap) {
                                    size_t ncap = cap ? cap * 2 : 8;
                                    char **nv = (char **)realloc(listed, ncap * sizeof(char *));
                                    if (!nv) { free(item); continue; }
                                    listed = nv;
                                    cap = ncap;
                                }
                                listed[nlisted++] = item;
                            } else free(item);
                        }
                        json_free(j);
                        continue;
                    }
                    if (j) json_free(j);
                }
                {
                    /* str(raw).split(",") */
                    const char *q = raw_j, *start = q;
                    for (;;) {
                        if (*q == ',' || !*q) {
                            char *item = ea_strip(ea_ndup(start, (size_t)(q - start)));
                            if (item[0]) {
                                size_t z;
                                for (z = 0; item[z]; z++) item[z] = ea_lower((unsigned char)item[z]);
                                if (nlisted == cap) {
                                    size_t ncap = cap ? cap * 2 : 8;
                                    char **nv = (char **)realloc(listed, ncap * sizeof(char *));
                                    if (!nv) { free(item); item = NULL; }
                                    else { listed = nv; cap = ncap; }
                                }
                                if (item) listed[nlisted++] = item;
                            } else free(item);
                            if (!*q) break;
                            q++;
                            start = q;
                            continue;
                        }
                        q++;
                    }
                }
            }
        }
        for (i = 0; i < nlisted; i++) if (!strcmp(listed[i], sender_key)) is_listed = 1;
        if (is_listed) {
            granted = 1;
        } else {
            const char *atp = strchr(sender_key, '@');
            char *local = atp ? ea_ndup(sender_key, (size_t)(atp - sender_key)) : ea_dup(sender_key);
            int local_listed = 0;
            for (i = 0; i < nlisted; i++) if (!strcmp(listed[i], local)) local_listed = 1;
            free(local);
            if (local_listed) {
                for (i = 0; i < nlisted; i++) free(listed[i]);
                free(listed); free(allowed_raw); free(sender_key);
                json_set(out, "accepted", json_bool(0));
                json_set(out, "log", json_string("debug"));
                return out;
            }
            if (authz && authz->type == JSON_BOOL) {
                granted = authz->bool_val ? 1 : 0;
            } else {
                int allow_all = 0;
                if (env) {
                    static const char *AN[2] = { "EMAIL_ALLOW_ALL_USERS", "GATEWAY_ALLOW_ALL_USERS" };
                    size_t z;
                    for (z = 0; z < 2; z++) {
                        const json_t *v = json_obj_get(env, AN[z]);
                        if (v && v->type == JSON_STRING) {
                            char *s = ea_strip(v->str_val);
                            size_t y;
                            for (y = 0; s[y]; y++) s[y] = ea_lower((unsigned char)s[y]);
                            if (!strcmp(s, "true") || !strcmp(s, "1") || !strcmp(s, "yes")) allow_all = 1;
                            free(s);
                        }
                    }
                }
                granted = allowed_raw[0] ? 0 : (allow_all ? 1 : 0);
            }
        }
        for (i = 0; i < nlisted; i++) free(listed[i]);
        free(listed);
        free(allowed_raw);
    }

    answered = 0;
    if (!granted) {
        const json_t *bj = extra ? json_obj_get(extra, "unauthorized_dm_behavior") : NULL;
        if (bj && bj->type == JSON_STRING) {
            char *s = ea_strip(bj->str_val);
            size_t z;
            for (z = 0; s[z]; z++) s[z] = ea_lower((unsigned char)s[z]);
            answered = !strcmp(s, "pair") || !strcmp(s, "decline");
            free(s);
        }
        if (!answered) {
            free(sender_key);
            json_set(out, "accepted", json_bool(0));
            json_set(out, "log", json_string("debug"));
            return out;
        }
    }
    if (require_auth && !authenticated) {
        int operator_fixable = 0, warn;
        if (auth_reason && (!strcmp(auth_reason, "no Authentication-Results header") ||
                            !strcmp(auth_reason, "no Authentication-Results from trusted authserv-id")))
            operator_fixable = 1;
        warn = is_listed || (granted && operator_fixable);
        free(sender_key);
        json_set(out, "accepted", json_bool(0));
        json_set(out, "log", json_string(warn ? "warning" : "debug"));
        return out;
    }
    free(sender_key);
    json_set(out, "accepted", json_bool(1));
    json_set(out, "log", json_string(""));
    return out;
}
