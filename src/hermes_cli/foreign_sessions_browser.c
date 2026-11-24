/*
 * foreign_sessions_browser.c - C11 port of upstream
 * hermes_cli/foreign_sessions_browser.py (gate w24_foreignses_93c0f0).
 *
 * PoP: w24_foreignses_93c0f0_handle_for @ hermes_cli/foreign_sessions_browser.py:_candidates
 * PoP: w24_foreignses_93c0f0_validate_handle @ hermes_cli/foreign_sessions_browser.py:resolve_foreign_session
 * PoP: w24_foreignses_93c0f0_resolve_error @ hermes_cli/foreign_sessions_browser.py:resolve_foreign_session
 * PoP: w24_foreignses_93c0f0_validate_page @ hermes_cli/foreign_sessions_browser.py:list_foreign_sessions
 * PoP: w24_foreignses_93c0f0_display_title @ hermes_cli/foreign_sessions_browser.py:_display_title
 * PoP: w24_foreignses_93c0f0_preview @ hermes_cli/foreign_sessions_browser.py:preview_foreign_session
 * PoP: w24_foreignses_93c0f0_parse_error @ hermes_cli/foreign_sessions_browser.py:_parse
 * PoP: w24_foreignses_93c0f0_next_offset @ hermes_cli/foreign_sessions_browser.py:list_foreign_sessions
 * PoP: w24_foreignses_93c0f0_constants @ hermes_cli/foreign_sessions_browser.py:<module constants>
 *
 * The seams are the filesystem, the per-source parser and the persistence
 * layer: _walk/_parser/import_foreign_history are never entered here. What is
 * ported is every decision made from values alone -- the opaque sha256 handle
 * (the only identifier a request may name), the page and handle validation, the
 * display title that skips a Codex attachment envelope, and the bounded preview
 * that keeps a huge log from mounting thousands of messages.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "noc_w24_foreignses_93c0f0.h"

#define FSB_MAX_LOG_BYTES  (32.0 * 1024.0 * 1024.0)   /* MAX_LOG_BYTES */
#define FSB_PREVIEW_TURNS  40
#define FSB_PREVIEW_CHARS  8000
#define FSB_TITLE_CHARS    180

static const char *const FSB_ERR_HANDLE = "Unknown session. Refresh the list and try again";
static const char *const FSB_ERR_GONE = "Session no longer available. Refresh the list and try again";
static const char *const FSB_ERR_PAGE = "Invalid session page";
static const char *const FSB_ERR_SIZE = "This log exceeds the 32 MB preview and import limit";
static const char *const FSB_ERR_EMPTY = "This session has no readable conversation messages";

/* _SOURCE_LABELS */
static const char *const FSB_SOURCE_KEYS[2] = {"claude", "codex"};
static const char *const FSB_SOURCE_LABELS[2] = {"Claude Code", "Codex CLI"};

/* ---------------------------------------------------------------- SHA-256 */

typedef struct {
    uint32_t h[8];
    uint64_t bitlen;
    unsigned char buf[64];
    size_t used;
} fsb_sha256;

static const uint32_t FSB_K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
    0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
    0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
    0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
    0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
};

static uint32_t fsb_rotr(uint32_t x, int n) {
    return (x >> n) | (x << (32 - n));
}

static void fsb_sha256_init(fsb_sha256 *s) {
    s->h[0] = 0x6a09e667u; s->h[1] = 0xbb67ae85u; s->h[2] = 0x3c6ef372u;
    s->h[3] = 0xa54ff53au; s->h[4] = 0x510e527fu; s->h[5] = 0x9b05688cu;
    s->h[6] = 0x1f83d9abu; s->h[7] = 0x5be0cd19u;
    s->bitlen = 0;
    s->used = 0;
}

static void fsb_sha256_block(fsb_sha256 *s, const unsigned char *p) {
    uint32_t w[64], a, b, c, d, e, f, g, h, t1, t2;
    int i;
    for (i = 0; i < 16; i++)
        w[i] = ((uint32_t)p[4 * i] << 24) | ((uint32_t)p[4 * i + 1] << 16) |
               ((uint32_t)p[4 * i + 2] << 8) | (uint32_t)p[4 * i + 3];
    for (i = 16; i < 64; i++) {
        uint32_t s0 = fsb_rotr(w[i - 15], 7) ^ fsb_rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = fsb_rotr(w[i - 2], 17) ^ fsb_rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = s->h[0]; b = s->h[1]; c = s->h[2]; d = s->h[3];
    e = s->h[4]; f = s->h[5]; g = s->h[6]; h = s->h[7];
    for (i = 0; i < 64; i++) {
        uint32_t s1 = fsb_rotr(e, 6) ^ fsb_rotr(e, 11) ^ fsb_rotr(e, 25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t s0 = fsb_rotr(a, 2) ^ fsb_rotr(a, 13) ^ fsb_rotr(a, 22);
        uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
        t1 = h + s1 + ch + FSB_K[i] + w[i];
        t2 = s0 + mj;
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d;
    s->h[4] += e; s->h[5] += f; s->h[6] += g; s->h[7] += h;
}

static void fsb_sha256_update(fsb_sha256 *s, const void *data, size_t len) {
    const unsigned char *p = (const unsigned char *)data;
    s->bitlen += (uint64_t)len * 8u;
    while (len > 0) {
        size_t take = 64 - s->used;
        if (take > len) take = len;
        memcpy(s->buf + s->used, p, take);
        s->used += take;
        p += take;
        len -= take;
        if (s->used == 64) {
            fsb_sha256_block(s, s->buf);
            s->used = 0;
        }
    }
}

static void fsb_sha256_hex(const void *data, size_t len, char out[65]) {
    static const char hexdig[] = "0123456789abcdef";
    fsb_sha256 s;
    unsigned char tail[72];
    size_t pad, i;
    uint64_t bits;

    fsb_sha256_init(&s);
    fsb_sha256_update(&s, data, len);
    bits = s.bitlen;

    pad = (s.used < 56) ? (56 - s.used) : (120 - s.used);
    memset(tail, 0, sizeof(tail));
    tail[0] = 0x80;
    for (i = 0; i < 8; i++) tail[pad + i] = (unsigned char)((bits >> (56 - 8 * i)) & 0xFF);
    fsb_sha256_update(&s, tail, pad + 8);

    for (i = 0; i < 8; i++) {
        uint32_t v = s.h[i];
        out[8 * i + 0] = hexdig[(v >> 28) & 0xF];
        out[8 * i + 1] = hexdig[(v >> 24) & 0xF];
        out[8 * i + 2] = hexdig[(v >> 20) & 0xF];
        out[8 * i + 3] = hexdig[(v >> 16) & 0xF];
        out[8 * i + 4] = hexdig[(v >> 12) & 0xF];
        out[8 * i + 5] = hexdig[(v >> 8) & 0xF];
        out[8 * i + 6] = hexdig[(v >> 4) & 0xF];
        out[8 * i + 7] = hexdig[v & 0xF];
    }
    out[64] = '\0';
}

/* ------------------------------------------------------------ text helpers */

static const json_t *fsb_arg(const json_t *req, const char *key) {
    return req && req->type == JSON_OBJECT ? json_obj_get(req, key) : NULL;
}

static const char *fsb_str(const json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : NULL;
}

static int fsb_isspace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static int fsb_inline_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v';
}

/* Number of UTF-8 code points (Python len() of a str). */
static size_t fsb_len_chars(const char *s) {
    size_t i = 0, n = 0;
    if (!s) return 0;
    while (s[i]) {
        i++;
        while (s[i] && ((unsigned char)s[i] & 0xC0) == 0x80) i++;
        n++;
    }
    return n;
}

/* Prefix of at most max_chars code points, ending on a code-point boundary. */
static char *fsb_prefix(const char *s, size_t max_chars) {
    size_t i = 0, n = 0;
    char *out;
    if (!s) s = "";
    while (s[i] && n < max_chars) {
        i++;
        while (s[i] && ((unsigned char)s[i] & 0xC0) == 0x80) i++;
        n++;
    }
    out = (char *)malloc(i + 1);
    if (!out) return NULL;
    memcpy(out, s, i);
    out[i] = '\0';
    return out;
}

/* Python str.strip() over the ASCII whitespace set. */
static char *fsb_strip(const char *s) {
    const char *a, *b;
    char *out;
    size_t n;
    if (!s) s = "";
    a = s;
    while (*a && fsb_isspace(*a)) a++;
    b = a + strlen(a);
    while (b > a && fsb_isspace(b[-1])) b--;
    n = (size_t)(b - a);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, a, n);
    out[n] = '\0';
    return out;
}

/* splitlines()[0]: up to the first line-boundary code point. */
static char *fsb_first_line(const char *s) {
    size_t i = 0;
    char *out;
    if (!s) s = "";
    while (s[i]) {
        unsigned char c = (unsigned char)s[i];
        if (c == '\n' || c == '\r' || c == '\v' || c == '\f' || c == 0x1c ||
            c == 0x1d || c == 0x1e || c == 0x85)
            break;
        i++;
    }
    out = (char *)malloc(i + 1);
    if (!out) return NULL;
    memcpy(out, s, i);
    out[i] = '\0';
    return out;
}

/* lstrip("# "). */
static const char *fsb_lstrip_hash(const char *s) {
    if (!s) s = "";
    while (*s == '#' || *s == ' ') s++;
    return s;
}

/* (?im)^#{1,6}\s*My request:\s*$ -- leftmost match, with everything after the
 * colon to end-of-line whitespace. */
static int fsb_find_my_request(const char *text, size_t *start, size_t *end) {
    static const char needle[12] = "my request:";
    size_t line = 0;

    if (!text) return 0;
    while (1) {
        size_t p, q, hashes = 0, i;
        int ok = 1;
        p = line;
        q = line;
        while (text[q] && text[q] != '\n') q++;
        while (text[p] == '#' && hashes < 6) { p++; hashes++; }
        if (hashes >= 1) {
            while (p < q && fsb_inline_space(text[p])) p++;
            if (p + 11 <= q) {
                for (i = 0; i < 11; i++) {
                    unsigned char c = (unsigned char)text[p + i];
                    if (c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 'a');
                    if (c != (unsigned char)needle[i]) { ok = 0; break; }
                }
                if (ok) {
                    p += 11;
                    while (p < q && fsb_inline_space(text[p])) p++;
                    if (p == q) {
                        *start = line;
                        *end = p;
                        return 1;
                    }
                }
            }
        }
        if (!text[q]) return 0;
        line = q + 1;
    }
}

/* --------------------------------------------------------------- functions */

json_t *w24_foreignses_93c0f0_handle_for(const json_t *req) {
    const char *source = fsb_str(fsb_arg(req, "source"));
    const char *path = fsb_str(fsb_arg(req, "path"));
    char *joined, hex[65];
    size_t need;

    if (!source) source = "";
    if (!path) path = "";
    need = strlen(source) + 1 + strlen(path) + 1;
    joined = (char *)malloc(need);
    if (!joined) return json_null();
    snprintf(joined, need, "%s:%s", source, path);
    fsb_sha256_hex(joined, strlen(joined), hex);
    free(joined);
    return json_string(hex);
}

json_t *w24_foreignses_93c0f0_resolve_error(const json_t *req) {
    const json_t *known = fsb_arg(req, "known");
    const char *handle = fsb_str(fsb_arg(req, "handle"));
    size_t i, n;

    if (!handle || fsb_len_chars(handle) != 64) return json_string(FSB_ERR_HANDLE);
    if (known && known->type == JSON_ARRAY) {
        n = json_len(known);
        for (i = 0; i < n; i++) {
            const char *k = fsb_str(json_get(known, i));
            if (k && strcmp(k, handle) == 0) return json_null();
        }
    }
    return json_string(FSB_ERR_GONE);
}

/* Python int view: a bool or an integral number (libjson stores doubles). */
static int fsb_as_int(const json_t *v, long long *out) {
    if (!v) return 0;
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1 : 0; return 1; }
    if (v->type == JSON_NUMBER) {
        double d = v->num_val;
        if (d != (double)(long long)d) return 0;
        *out = (long long)d;
        return 1;
    }
    return 0;
}

json_t *w24_foreignses_93c0f0_validate_page(const json_t *req) {
    long long offset = 0, limit = 0;
    if (!fsb_as_int(fsb_arg(req, "offset"), &offset) || offset < 0)
        return json_string(FSB_ERR_PAGE);
    if (!fsb_as_int(fsb_arg(req, "limit"), &limit) || limit < 1 || limit > 50)
        return json_string(FSB_ERR_PAGE);
    return json_null();
}

static const char *fsb_source_label(const char *source) {
    size_t i;
    if (!source) return NULL;
    for (i = 0; i < 2; i++)
        if (strcmp(source, FSB_SOURCE_KEYS[i]) == 0) return FSB_SOURCE_LABELS[i];
    return NULL;
}

json_t *w24_foreignses_93c0f0_display_title(const json_t *req) {
    const json_t *parsed = fsb_arg(req, "parsed");
    const json_t *turns = parsed && parsed->type == JSON_OBJECT
                              ? json_obj_get(parsed, "turns") : NULL;
    const char *source = fsb_str(fsb_arg(req, "source"));
    const char *first_user = "";
    char *tail = NULL, *stripped = NULL, *title = NULL, *out_text;
    size_t i, n, mstart = 0, mend = 0;
    json_t *out;

    if (turns && turns->type == JSON_ARRAY) {
        n = json_len(turns);
        for (i = 0; i < n; i++) {
            const json_t *turn = json_get(turns, i);
            const char *role = turn && turn->type == JSON_OBJECT
                                   ? fsb_str(json_obj_get(turn, "role")) : NULL;
            if (role && strcmp(role, "user") == 0) {
                const char *content = fsb_str(json_obj_get(turn, "content"));
                first_user = content ? content : "";
                break;
            }
        }
    }

    if (fsb_find_my_request(first_user, &mstart, &mend)) {
        const char *rest = first_user + mend;
        tail = (char *)malloc(strlen(rest) + 1);
        if (tail) {
            memcpy(tail, rest, strlen(rest) + 1);
            stripped = fsb_strip(tail);
            if (stripped && stripped[0] != '\0') title = fsb_first_line(stripped);
        }
        (void)mstart;
    }
    if (!title || title[0] == '\0') {
        const json_t *guess = parsed && parsed->type == JSON_OBJECT
                                  ? json_obj_get(parsed, "title_guess") : NULL;
        const char *g = fsb_str(guess);
        free(title);
        title = NULL;
        if (g) {
            title = (char *)malloc(strlen(g) + 1);
            if (title) memcpy(title, g, strlen(g) + 1);
        }
    }
    if (!title || title[0] == '\0') {
        const char *label = fsb_source_label(source);
        free(title);
        title = NULL;
        if (!label) { free(stripped); free(tail); return json_null(); }
        title = (char *)malloc(strlen(label) + 1);
        if (!title) { free(stripped); free(tail); return json_null(); }
        memcpy(title, label, strlen(label) + 1);
    }

    out_text = fsb_prefix(fsb_lstrip_hash(title), FSB_TITLE_CHARS);
    free(title);
    free(stripped);
    free(tail);
    if (!out_text) return json_null();
    out = json_string(out_text);
    free(out_text);
    return out;
}

json_t *w24_foreignses_93c0f0_preview(const json_t *req) {
    const json_t *parsed = fsb_arg(req, "parsed");
    const json_t *turns = parsed && parsed->type == JSON_OBJECT
                              ? json_obj_get(parsed, "turns") : NULL;
    json_t *out = json_object();
    json_t *messages = json_array();
    size_t total = 0, from = 0, i;
    int truncated = 0;

    if (turns && turns->type == JSON_ARRAY) {
        total = json_len(turns);
        if (total > FSB_PREVIEW_TURNS) { truncated = 1; from = total - FSB_PREVIEW_TURNS; }
        for (i = from; i < total; i++) {
            const json_t *turn = json_get(turns, i);
            json_t *row = turn ? json_copy(turn) : json_object();
            const char *content = turn && turn->type == JSON_OBJECT
                                      ? fsb_str(json_obj_get(turn, "content")) : NULL;
            if (content && fsb_len_chars(content) > FSB_PREVIEW_CHARS) truncated = 1;
            if (row && row->type == JSON_OBJECT) {
                char *clip = fsb_prefix(content, FSB_PREVIEW_CHARS);
                if (clip) {
                    json_set(row, "content", json_string(clip));
                    free(clip);
                }
            }
            json_append(messages, row);
        }
    }
    json_set(out, "messages", messages);
    json_set(out, "total", json_number((double)total));
    json_set(out, "truncated", json_bool(truncated ? true : false));
    return out;
}

json_t *w24_foreignses_93c0f0_parse_error(const json_t *req) {
    const json_t *size = fsb_arg(req, "size");
    long long turn_count = 0;
    if (size && size->type == JSON_NUMBER && size->num_val > FSB_MAX_LOG_BYTES)
        return json_string(FSB_ERR_SIZE);
    if (!fsb_as_int(fsb_arg(req, "turn_count"), &turn_count) || turn_count <= 0)
        return json_string(FSB_ERR_EMPTY);
    return json_null();
}

json_t *w24_foreignses_93c0f0_next_offset(const json_t *req) {
    long long offset = 0, limit = 0, total = 0, next;
    if (!fsb_as_int(fsb_arg(req, "offset"), &offset)) offset = 0;
    if (!fsb_as_int(fsb_arg(req, "limit"), &limit)) limit = 0;
    if (!fsb_as_int(fsb_arg(req, "total"), &total)) total = 0;
    next = offset + limit;
    if (next < total) return json_number((double)next);
    return json_null();
}

json_t *w24_foreignses_93c0f0_constants(void) {
    json_t *o = json_object();
    json_t *labels = json_object();
    size_t i;
    json_set(o, "max_log_bytes", json_number(FSB_MAX_LOG_BYTES));
    json_set(o, "preview_turns", json_number(FSB_PREVIEW_TURNS));
    json_set(o, "preview_chars", json_number(FSB_PREVIEW_CHARS));
    json_set(o, "title_chars", json_number(FSB_TITLE_CHARS));
    for (i = 0; i < 2; i++)
        json_set(labels, FSB_SOURCE_KEYS[i], json_string(FSB_SOURCE_LABELS[i]));
    json_set(o, "source_labels", labels);
    return o;
}
