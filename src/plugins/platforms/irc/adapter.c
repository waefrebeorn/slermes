/*
 * adapter.c - C11 port of upstream plugins/platforms/irc/adapter.py
 * (gates w33_adapter_eaa5eb_p1 and w33_adapter_eaa5eb_p2).
 *
 * PoP: w33_adapter_eaa5eb_p1_parse_message @ plugins/platforms/irc/adapter.py:_parse_irc_message
 * PoP: w33_adapter_eaa5eb_p1_extract_nick @ plugins/platforms/irc/adapter.py:_extract_nick
 * PoP: w33_adapter_eaa5eb_p1_strip_markdown @ plugins/platforms/irc/adapter.py:IRCAdapter._strip_markdown
 * PoP: w33_adapter_eaa5eb_p1_chunk_paragraph @ plugins/platforms/irc/adapter.py:_chunk_paragraph
 * PoP: w33_adapter_eaa5eb_p1_split_lines @ plugins/platforms/irc/adapter.py:_split_lines
 * PoP: w33_adapter_eaa5eb_p1_privmsg_budget @ plugins/platforms/irc/adapter.py:_privmsg_budget
 * PoP: w33_adapter_eaa5eb_p1_split_message @ plugins/platforms/irc/adapter.py:IRCAdapter._split_message
 * PoP: w33_adapter_eaa5eb_p1_encode_line @ plugins/platforms/irc/adapter.py:_encode_line
 * PoP: w33_adapter_eaa5eb_p1_sanitize_outbound @ plugins/platforms/irc/adapter.py:_strip_irc_control_chars
 * PoP: w33_adapter_eaa5eb_p1_ping_reply @ plugins/platforms/irc/adapter.py:IRCAdapter._handle_line (PING)
 * PoP: w33_adapter_eaa5eb_p2_privmsg_decision @ plugins/platforms/irc/adapter.py:IRCAdapter._handle_privmsg
 * PoP: w33_adapter_eaa5eb_p2_next_nick_collision @ plugins/platforms/irc/adapter.py:IRCAdapter._handle_line (433)
 * PoP: w33_adapter_eaa5eb_p2_standalone_nick_base @ plugins/platforms/irc/adapter.py:_standalone_send (nick_base)
 * PoP: w33_adapter_eaa5eb_p2_standalone_nick @ plugins/platforms/irc/adapter.py:_sa_register (standalone_nick)
 * PoP: w33_adapter_eaa5eb_p2_nick_retry @ plugins/platforms/irc/adapter.py:_sa_register (_on_registration)
 * PoP: w33_adapter_eaa5eb_p2_standalone_target @ plugins/platforms/irc/adapter.py:_standalone_send (target guard)
 * PoP: w33_adapter_eaa5eb_p2_standalone_lines @ plugins/platforms/irc/adapter.py:_standalone_send (payload plan)
 * PoP: w33_adapter_eaa5eb_p2_is_channel @ plugins/platforms/irc/adapter.py:_is_irc_channel
 * PoP: w33_adapter_eaa5eb_p2_chat_info @ plugins/platforms/irc/adapter.py:IRCAdapter.get_chat_info
 * PoP: w33_adapter_eaa5eb_p2_env_enablement @ plugins/platforms/irc/adapter.py:_env_enablement
 * PoP: w33_adapter_eaa5eb_p2_server_channel @ plugins/platforms/irc/adapter.py:_server_channel
 * PoP: w33_adapter_eaa5eb_p2_config_ready @ plugins/platforms/irc/adapter.py:validate_config
 *
 * Seams (never ported here): the asyncio stream reader/writer and its drain,
 * the TLS context, the platform lock, the registration Event, the secret store
 * (_get_scoped_secret), the clock (_ms_id), uuid4 message ids and the
 * send_error envelope. What is ported is a pure function of values, fed by
 * fixtures the harnesses script.
 *
 * Corpus restrictions shared with the oracles: UTF-8 text (code-point aware
 * chunking), ASCII whitespace stripping, Python `\w` approximated as ASCII
 * alnum/'_' plus any non-ASCII byte, chunk limits >= 1 (a limit <= 0 makes
 * upstream _chunk_paragraph spin forever; this port keeps the tail whole
 * instead of hanging), and no embedded NUL bytes (a C string cannot carry one,
 * so fixtures express \x00 only at end-of-string).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w33_adapter_eaa5eb_p1.h"
#include "noc_w33_adapter_eaa5eb_p2.h"

/* ── small string buffer ────────────────────────────────────────────────── */

typedef struct {
    char *p;
    size_t n, cap;
} abuf;

static int ab_grow(abuf *b, size_t need) {
    size_t cap;
    char *np;
    if (b->n + need + 1 <= b->cap) return 1;
    cap = b->cap ? b->cap : 64;
    while (cap < b->n + need + 1) cap *= 2;
    np = (char *)realloc(b->p, cap);
    if (!np) return 0;
    b->p = np;
    b->cap = cap;
    return 1;
}

static void ab_putn(abuf *b, const char *s, size_t n) {
    if (n == 0 || !s) return;
    if (!ab_grow(b, n)) return;
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
}

static void ab_puts(abuf *b, const char *s) {
    if (s) ab_putn(b, s, strlen(s));
}

static void ab_putc(abuf *b, char c) { ab_putn(b, &c, 1); }

static char *ab_take(abuf *b) {
    char *out;
    if (!b->p) {
        out = (char *)malloc(1);
        if (out) out[0] = '\0';
    } else {
        out = b->p;
    }
    b->p = NULL;
    b->n = b->cap = 0;
    return out;
}

/* ── UTF-8 code points ──────────────────────────────────────────────────── */

static int a_iscont(unsigned char c) { return (c & 0xC0) == 0x80; }

/* Number of code points in a NUL-terminated UTF-8 string. */
static size_t a_u8len(const char *s) {
    size_t n = 0;
    const unsigned char *p = (const unsigned char *)s;
    for (; *p; p++) if (!a_iscont(*p)) n++;
    return n;
}

/* Byte offset of the k-th code point (k >= length -> strlen). */
static size_t a_u8off(const char *s, size_t chars) {
    size_t i = 0, n = 0;
    if (chars == 0) return 0;
    while (s[i]) {
        if (!a_iscont((unsigned char)s[i])) {
            if (n == chars) return i;
            n++;
        }
        i++;
    }
    return i;
}

/* Code points in s[0..bytes). */
static size_t a_u8count_upto(const char *s, size_t bytes) {
    size_t i, n = 0;
    for (i = 0; i < bytes && s[i]; i++)
        if (!a_iscont((unsigned char)s[i])) n++;
    return n;
}

/* ── Python str helpers ─────────────────────────────────────────────────── */

/* str.isspace() over the range Python's no-arg strip removes. */
static int a_ws(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' ||
           c == '\f' || (c >= 0x1c && c <= 0x1f);
}

/* Copy s[0..n) with Python .strip()/.lstrip()/.rstrip() applied. */
static char *a_dup_strip(const char *s, size_t n, int left, int right) {
    size_t a = 0, b = n, len;
    char *out;
    if (left) while (a < b && a_ws((unsigned char)s[a])) a++;
    if (right) while (b > a && a_ws((unsigned char)s[b - 1])) b--;
    len = b - a;
    out = (char *)malloc(len + 1);
    if (!out) return NULL;
    if (len) memcpy(out, s + a, len);
    out[len] = '\0';
    return out;
}

static char *a_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

/* Python '\w' (ASCII plus any non-ASCII byte, which stands for a letter). */
static int a_isword(unsigned char c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') || c == '_' || c >= 0x80;
}

static char a_lowerc(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

static int a_lower_eq(const char *a, const char *b) {
    if (!a) a = "";
    if (!b) b = "";
    while (*a && *b) {
        if (a_lowerc(*a) != a_lowerc(*b)) return 0;
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static int a_lower_prefix(const char *text, const char *prefix) {
    size_t i;
    if (!text) text = "";
    if (!prefix) prefix = "";
    for (i = 0; prefix[i]; i++)
        if (!text[i] || a_lowerc(text[i]) != a_lowerc(prefix[i])) return 0;
    return 1;
}

/* Python truthiness of a JSON value. */
static int a_truthy(const json_t *v) {
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

/* Object member as a C string (missing / non-string -> def). */
static const char *a_str(const json_t *obj, const char *key, const char *def) {
    const json_t *v = json_obj_get(obj, key);
    if (v && v->type == JSON_STRING && v->str_val) return v->str_val;
    return def;
}

/* ── markdown -> IRC plain text (the eight _MARKDOWN_RULES) ────────────── */

typedef struct {
    size_t start, end;   /* whole match */
    size_t g1s, g1e;     /* group 1 */
    size_t g2s, g2e;     /* group 2 */
} amatch;

typedef int (*amatch_fn)(const char *s, size_t len, size_t from, amatch *m);

enum { AREPL_G1, AREPL_G2, AREPL_G1_URL, AREPL_EMPTY };

/* Bold/italic/code pairs: one or two identical delimiters, a non-greedy
 * one-line group, and (for the single underscore) word guards on both sides. */
static int a_find_pair(const char *s, size_t len, size_t from, char c, int pair,
                       int wordguard, amatch *m) {
    size_t i, j, open;
    for (i = from; i + (size_t)pair <= len; i++) {
        if (s[i] != c) continue;
        if (pair == 2 && s[i + 1] != c) continue;
        if (wordguard && i > 0 && a_isword((unsigned char)s[i - 1])) continue;
        open = i + (size_t)pair;
        for (j = open + 1; j + (size_t)pair <= len; j++) {
            if (s[j] == '\n') break;   /* '.' never crosses a line */
            if (s[j] != c) continue;
            if (pair == 2 && s[j + 1] != c) continue;
            if (wordguard && j + 1 < len && a_isword((unsigned char)s[j + 1])) continue;
            m->start = i;
            m->end = j + (size_t)pair;
            m->g1s = open;
            m->g1e = j;
            m->g2s = m->g2e = 0;
            return 1;
        }
    }
    return 0;
}

static int a_find_bold(const char *s, size_t len, size_t from, amatch *m) {
    return a_find_pair(s, len, from, '*', 2, 0, m);
}
static int a_find_under2(const char *s, size_t len, size_t from, amatch *m) {
    return a_find_pair(s, len, from, '_', 2, 0, m);
}
static int a_find_star(const char *s, size_t len, size_t from, amatch *m) {
    return a_find_pair(s, len, from, '*', 1, 0, m);
}
static int a_find_under1(const char *s, size_t len, size_t from, amatch *m) {
    return a_find_pair(s, len, from, '_', 1, 1, m);
}
static int a_find_tick(const char *s, size_t len, size_t from, amatch *m) {
    return a_find_pair(s, len, from, '`', 1, 0, m);
}

/* ```\w*\n? -> removed. */
static int a_find_fence(const char *s, size_t len, size_t from, amatch *m) {
    size_t i, e;
    for (i = from; i + 3 <= len; i++) {
        if (s[i] != '`' || s[i + 1] != '`' || s[i + 2] != '`') continue;
        e = i + 3;
        while (e < len && a_isword((unsigned char)s[e])) e++;
        if (e < len && s[e] == '\n') e++;
        m->start = i;
        m->end = e;
        m->g1s = m->g1e = m->g2s = m->g2e = 0;
        return 1;
    }
    return 0;
}

/* ![alt](url) -> url (bang) or [text](url) -> "text (url)". */
static int a_find_bracket(const char *s, size_t len, size_t from, int bang, amatch *m) {
    size_t i, alt, k, close;
    for (i = from; i + 1 < len; i++) {
        if (bang) {
            if (s[i] != '!' || s[i + 1] != '[') continue;
            alt = i + 1;
        } else {
            if (s[i] != '[') continue;
            alt = i;
        }
        k = alt + 1;
        while (k < len && s[k] != ']') k++;
        if (k >= len) continue;
        if (!bang && k == alt + 1) continue;              /* text needs >= 1 */
        if (k + 1 >= len || s[k + 1] != '(') continue;
        close = k + 2;
        while (close < len && s[close] != ')') close++;
        if (close >= len) continue;
        if (close == k + 2) continue;                     /* url needs >= 1 */
        m->start = i;
        m->end = close + 1;
        m->g1s = alt + 1;
        m->g1e = k;
        m->g2s = k + 2;
        m->g2e = close;
        return 1;
    }
    return 0;
}

static int a_find_image(const char *s, size_t len, size_t from, amatch *m) {
    return a_find_bracket(s, len, from, 1, m);
}
static int a_find_link(const char *s, size_t len, size_t from, amatch *m) {
    return a_find_bracket(s, len, from, 0, m);
}

/* re.sub(fn, repl, s): left to right, resuming at the end of each match. */
static char *a_apply_rule(const char *s, amatch_fn fn, int kind) {
    size_t len = strlen(s), pos = 0;
    abuf out = {NULL, 0, 0};
    amatch m;
    while (pos < len && fn(s, len, pos, &m)) {
        ab_putn(&out, s + pos, m.start - pos);
        switch (kind) {
        case AREPL_G1: ab_putn(&out, s + m.g1s, m.g1e - m.g1s); break;
        case AREPL_G2: ab_putn(&out, s + m.g2s, m.g2e - m.g2s); break;
        case AREPL_G1_URL:
            ab_putn(&out, s + m.g1s, m.g1e - m.g1s);
            ab_puts(&out, " (");
            ab_putn(&out, s + m.g2s, m.g2e - m.g2s);
            ab_putc(&out, ')');
            break;
        case AREPL_EMPTY: break;
        }
        pos = m.end;
    }
    ab_putn(&out, s + pos, len - pos);
    {
        char *res = ab_take(&out);
        if (!res) return a_dup("");
        return res;
    }
}

static char *a_strip_markdown_c(const char *text) {
    char *cur = a_dup(text ? text : "");
    static const struct { amatch_fn fn; int kind; } rules[] = {
        {a_find_bold, AREPL_G1},
        {a_find_under2, AREPL_G1},
        {a_find_star, AREPL_G1},
        {a_find_under1, AREPL_G1},
        {a_find_tick, AREPL_G1},
        {a_find_fence, AREPL_EMPTY},
        {a_find_image, AREPL_G2},
        {a_find_link, AREPL_G1_URL},
    };
    size_t i;
    if (!cur) return NULL;
    for (i = 0; i < sizeof(rules) / sizeof(rules[0]); i++) {
        char *next = a_apply_rule(cur, rules[i].fn, rules[i].kind);
        free(cur);
        if (!next) return NULL;
        cur = next;
    }
    return cur;
}

/* ── byte budget / chunking ─────────────────────────────────────────────── */

/* 510 - (len("PRIVMSG " + target + " :") + 2). */
static long a_privmsg_budget(const char *target) {
    return (long)W33_ADAPTER_EAA5EB_P1_LINE_BUDGET -
           (long)(8 + strlen(target ? target : "") + 2 + 2);
}

static json_t *a_chunk_paragraph(const char *p0, long limit) {
    json_t *out = json_array();
    char *cur = a_dup(p0 ? p0 : "");
    if (!out) return NULL;
    if (!cur) return out;
    while (*cur) {
        size_t blen = strlen(cur), nchars, low, high, split_at, off;
        long space_ci = -1;
        char *piece, *rest;
        if ((long)blen <= limit) {
            json_append(out, json_string(cur));
            break;
        }
        nchars = a_u8len(cur);
        low = 1;
        high = nchars;
        split_at = 0;
        while (low <= high) {
            size_t mid = (low + high) / 2;
            if ((long)a_u8off(cur, mid) <= limit) {
                split_at = mid;
                low = mid + 1;
            } else {
                if (mid == 0) break;
                high = mid - 1;
            }
        }
        if (split_at == 0) {   /* upstream would loop forever; keep the tail */
            json_append(out, json_string(cur));
            break;
        }
        off = a_u8off(cur, split_at);
        {
            size_t i;
            for (i = 0; i < off; i++)
                if (cur[i] == ' ') space_ci = (long)a_u8count_upto(cur, i);
        }
        if (space_ci > (long)(split_at / 3)) split_at = (size_t)space_ci;
        off = a_u8off(cur, split_at);
        piece = a_dup_strip(cur, off, 0, 1);
        if (!piece) break;
        json_append(out, json_string(piece));
        free(piece);
        rest = a_dup_strip(cur + off, strlen(cur + off), 1, 0);
        free(cur);
        cur = rest;
        if (!cur) break;
    }
    free(cur);
    return out;
}

static json_t *a_split_lines(const json_t *paragraphs, long limit) {
    json_t *out = json_array();
    size_t i, n;
    if (!out) return NULL;
    n = json_len(paragraphs);
    for (i = 0; i < n; i++) {
        const json_t *p = json_get(paragraphs, i);
        json_t *chunks;
        size_t j, cn;
        if (!p || p->type != JSON_STRING) continue;
        chunks = a_chunk_paragraph(p->str_val ? p->str_val : "", limit);
        if (!chunks) continue;
        cn = json_len(chunks);
        for (j = 0; j < cn; j++) json_append(out, json_copy(json_get(chunks, j)));
        json_free(chunks);
    }
    return out;
}

/* ── p1: wire format ───────────────────────────────────────────────────── */

json_t *w33_adapter_eaa5eb_p1_parse_message(const json_t *req) {
    const char *raw = a_str(req, "raw", "");
    char *rest, *prefix;
    char *trail = NULL;
    json_t *params, *out;
    const char *command = "";
    char *cmd_dup = NULL;
    size_t rl, i;

    params = json_array();
    out = json_object();
    if (!params || !out) {
        json_free(params);
        json_free(out);
        return json_null();
    }

    prefix = a_dup("");
    if (raw[0] == ':') {
        const char *sp = strchr(raw + 1, ' ');
        if (sp) {
            free(prefix);
            prefix = a_dup_strip(raw + 1, (size_t)(sp - (raw + 1)), 0, 0);
            rest = a_dup(sp + 1);
        } else {
            free(prefix);
            prefix = a_dup(raw + 1);
            rest = a_dup("");
        }
    } else {
        rest = a_dup(raw);
    }
    if (!prefix || !rest) {
        free(prefix);
        free(rest);
        json_free(params);
        json_free(out);
        return json_null();
    }

    rl = strlen(rest);
    for (i = 0; i + 2 <= rl; i++) {
        if (rest[i] == ' ' && rest[i + 1] == ':') {
            trail = a_dup(rest + i + 2);
            rest[i] = '\0';
            break;
        }
    }

    /* str.split(): runs of whitespace, no empty tokens. */
    {
        const char *p = rest;
        int first = 1;
        while (*p) {
            const char *start;
            while (*p && a_ws((unsigned char)*p)) p++;
            if (!*p) break;
            start = p;
            while (*p && !a_ws((unsigned char)*p)) p++;
            if (first) {
                free(cmd_dup);
                cmd_dup = a_dup_strip(start, (size_t)(p - start), 0, 0);
                command = cmd_dup ? cmd_dup : "";
                first = 0;
            } else {
                char *tok = a_dup_strip(start, (size_t)(p - start), 0, 0);
                json_append(params, json_string(tok ? tok : ""));
                free(tok);
            }
        }
    }
    if (trail && trail[0]) json_append(params, json_string(trail));

    json_set(out, "prefix", json_string(prefix));
    json_set(out, "command", json_string(command));
    json_set(out, "params", params);
    free(prefix);
    free(rest);
    free(trail);
    free(cmd_dup);
    return out;
}

json_t *w33_adapter_eaa5eb_p1_extract_nick(const json_t *req) {
    const char *prefix = a_str(req, "prefix", "");
    const char *bang = strchr(prefix, '!');
    size_t n = bang ? (size_t)(bang - prefix) : strlen(prefix);
    char *nick = a_dup_strip(prefix, n, 0, 0);
    json_t *out;
    if (!nick) return json_null();
    out = json_string(nick);
    free(nick);
    return out;
}

json_t *w33_adapter_eaa5eb_p1_strip_markdown(const json_t *req) {
    const char *text = a_str(req, "text", "");
    char *plain = a_strip_markdown_c(text);
    json_t *out;
    if (!plain) return json_null();
    out = json_string(plain);
    free(plain);
    return out;
}

json_t *w33_adapter_eaa5eb_p1_chunk_paragraph(const json_t *req) {
    return a_chunk_paragraph(a_str(req, "paragraph", ""),
                             (long)json_get_num(req, "limit", 0.0));
}

json_t *w33_adapter_eaa5eb_p1_split_lines(const json_t *req) {
    return a_split_lines(json_obj_get(req, "paragraphs"),
                         (long)json_get_num(req, "limit", 0.0));
}

json_t *w33_adapter_eaa5eb_p1_privmsg_budget(const json_t *req) {
    return json_number((double)a_privmsg_budget(a_str(req, "target", "")));
}

json_t *w33_adapter_eaa5eb_p1_split_message(const json_t *req) {
    const char *content = a_str(req, "content", "");
    const char *target = a_str(req, "target", "");
    const json_t *maxj = json_obj_get(req, "max_message_length");
    long maxlen = maxj && maxj->type == JSON_NUMBER ? (long)maxj->num_val : 450;
    long limit = a_privmsg_budget(target);
    char *plain;
    json_t *paragraphs, *out;
    size_t i, n, start = 0;

    if (maxlen < limit) limit = maxlen;
    plain = a_strip_markdown_c(content);
    if (!plain) return json_null();
    paragraphs = json_array();
    if (!paragraphs) {
        free(plain);
        return json_null();
    }
    n = strlen(plain);
    for (i = 0; i <= n; i++) {
        if (i == n || plain[i] == '\n') {
            char *probe = a_dup_strip(plain + start, i - start, 1, 1);
            if (probe && probe[0]) {
                char *line = a_dup_strip(plain + start, i - start, 0, 0);
                json_append(paragraphs, json_string(line ? line : ""));
                free(line);
            }
            free(probe);
            start = i + 1;
        }
    }
    out = a_split_lines(paragraphs, limit);
    json_free(paragraphs);
    free(plain);
    if (!out) return json_null();
    if (json_len(out) == 0) json_append(out, json_string(""));
    return out;
}

json_t *w33_adapter_eaa5eb_p1_encode_line(const json_t *req) {
    const char *line = a_str(req, "line", "");
    abuf b = {NULL, 0, 0};
    json_t *out;
    char *s;
    ab_puts(&b, line);
    ab_puts(&b, "\r\n");
    s = ab_take(&b);
    if (!s) return json_null();
    out = json_string(s);
    free(s);
    return out;
}

json_t *w33_adapter_eaa5eb_p1_sanitize_outbound(const json_t *req) {
    const char *text = a_str(req, "text", "");
    abuf b = {NULL, 0, 0};
    const char *p;
    json_t *out;
    char *s;
    for (p = text; *p; p++) {
        if (*p == '\r' || *p == '\n') ab_putc(&b, ' ');
        else ab_putc(&b, *p);
    }
    s = ab_take(&b);
    if (!s) return json_null();
    out = json_string(s);
    free(s);
    return out;
}

json_t *w33_adapter_eaa5eb_p1_ping_reply(const json_t *req) {
    const json_t *params = json_obj_get(req, "params");
    const json_t *first = params && params->type == JSON_ARRAY ? json_get(params, 0) : NULL;
    const char *tok = first && first->type == JSON_STRING && first->str_val ? first->str_val : "";
    abuf b = {NULL, 0, 0};
    char *s;
    json_t *out;
    ab_puts(&b, "PONG :");
    ab_puts(&b, tok);
    s = ab_take(&b);
    if (!s) return json_null();
    out = json_string(s);
    free(s);
    return out;
}

/* ── p2: admission ─────────────────────────────────────────────────────── */

static json_t *a_decision(int dispatch, const char *text, const char *chat_id,
                          const char *chat_type, const char *sender) {
    json_t *out = json_object();
    if (!out) return json_null();
    json_set(out, "dispatch", json_bool(dispatch));
    json_set(out, "text", text ? json_string(text) : json_null());
    json_set(out, "chat_id", chat_id ? json_string(chat_id) : json_null());
    json_set(out, "chat_type", chat_type ? json_string(chat_type) : json_null());
    json_set(out, "user_id", dispatch && sender ? json_string(sender) : json_null());
    json_set(out, "user_name", dispatch && sender ? json_string(sender) : json_null());
    return out;
}

json_t *w33_adapter_eaa5eb_p2_privmsg_decision(const json_t *req) {
    const char *nick = a_str(req, "nick", "");
    const char *sender = a_str(req, "sender", "");
    const char *target = a_str(req, "target", "");
    const char *text = a_str(req, "text", "");
    const json_t *allowed = json_obj_get(req, "allowed_users");
    char *cur;
    int is_channel;
    json_t *out;

    if (a_lower_eq(sender, nick)) return a_decision(0, NULL, NULL, NULL, NULL);

    cur = a_dup(text);
    if (!cur) return json_null();

    /* CTCP ACTION (/me) folds into "* <sender> <body>". */
    if (strncmp(cur, "\x01" "ACTION ", 8) == 0) {
        size_t cl = strlen(cur);
        if (cl > 0 && cur[cl - 1] == '\x01') {
            abuf b = {NULL, 0, 0};
            char *folded;
            ab_puts(&b, "* ");
            ab_puts(&b, sender);
            ab_putc(&b, ' ');
            ab_putn(&b, cur + 8, cl - 8 - 1);
            folded = ab_take(&b);
            free(cur);
            cur = folded;
            if (!cur) return json_null();
        }
    }
    if (cur[0] == '\x01') {   /* any other CTCP */
        free(cur);
        return a_decision(0, NULL, NULL, NULL, NULL);
    }

    is_channel = (target[0] == '#' || target[0] == '&');

    if (is_channel) {
        static const char *const seps[3] = {":", ",", " "};
        int matched = 0;
        size_t k;
        for (k = 0; k < 3 && !matched; k++) {
            abuf b = {NULL, 0, 0};
            char *prefix, *trimmed;
            size_t plen;
            ab_puts(&b, nick);
            ab_puts(&b, seps[k]);
            prefix = ab_take(&b);
            if (!prefix) break;
            plen = strlen(prefix);
            if (a_lower_prefix(cur, prefix)) {
                trimmed = a_dup_strip(cur + plen, strlen(cur + plen), 1, 1);
                free(cur);
                cur = trimmed;
                matched = 1;
            }
            free(prefix);
            if (!cur) return json_null();
        }
        if (!matched) {
            free(cur);
            return a_decision(0, NULL, NULL, NULL, NULL);
        }
    }

    if (allowed && a_truthy(allowed)) {
        size_t i, n = json_len(allowed);
        int found = 0;
        for (i = 0; i < n && !found; i++) {
            const json_t *u = json_get(allowed, i);
            if (u && u->type == JSON_STRING && a_lower_eq(sender, u->str_val)) found = 1;
        }
        if (!found) {
            free(cur);
            return a_decision(0, NULL, NULL, NULL, NULL);
        }
    }

    out = a_decision(1, cur, is_channel ? target : sender,
                     is_channel ? "group" : "dm", sender);
    free(cur);
    return out;
}

/* _(\d+)$ matched at the leftmost start position. */
static int a_find_trailing_digits(const char *s, size_t *digits_at) {
    size_t len = strlen(s), i;
    for (i = 0; i < len; i++) {
        size_t j;
        if (s[i] != '_' || i + 1 >= len || s[i + 1] < '0' || s[i + 1] > '9') continue;
        for (j = i + 1; j < len && s[j] >= '0' && s[j] <= '9'; j++) {}
        if (j == len) {
            *digits_at = i + 1;
            return 1;
        }
    }
    return 0;
}

json_t *w33_adapter_eaa5eb_p2_next_nick_collision(const json_t *req) {
    const char *nickname = a_str(req, "nickname", "");
    const char *current = a_str(req, "current_nick", "");
    size_t digits_at;
    abuf b = {NULL, 0, 0};
    char *nick, *cmd;
    json_t *out;

    if (a_find_trailing_digits(current, &digits_at)) {
        long value = strtol(current + digits_at, NULL, 10) + 1;
        size_t n = strlen(nickname), keep = n;
        while (keep > 0 && (nickname[keep - 1] == '_' ||
                            (nickname[keep - 1] >= '0' && nickname[keep - 1] <= '9')))
            keep--;
        ab_putn(&b, nickname, keep);
        ab_putc(&b, '_');
        {
            char num[32];
            snprintf(num, sizeof(num), "%ld", value);
            ab_puts(&b, num);
        }
    } else {
        ab_puts(&b, nickname);
        ab_puts(&b, strcmp(current, nickname) == 0 ? "_" : "_1");
    }
    nick = ab_take(&b);
    if (!nick) return json_null();

    ab_puts(&b, "NICK ");
    ab_puts(&b, nick);
    cmd = ab_take(&b);
    if (!cmd) {
        free(nick);
        return json_null();
    }
    out = json_object();
    if (!out) {
        free(nick);
        free(cmd);
        return json_null();
    }
    json_set(out, "nick", json_string(nick));
    json_set(out, "command", json_string(cmd));
    free(nick);
    free(cmd);
    return out;
}

/* nickname.rstrip("_0123456789-")[:24] or "hermes-bot". */
static char *a_nick_base(const char *nickname) {
    size_t n, keep, take;
    if (!nickname) nickname = "";
    n = strlen(nickname);
    keep = n;
    while (keep > 0 && (nickname[keep - 1] == '_' || nickname[keep - 1] == '-' ||
                        (nickname[keep - 1] >= '0' && nickname[keep - 1] <= '9')))
        keep--;
    take = a_u8off(nickname, W33_ADAPTER_EAA5EB_P2_NICK_BASE_MAX);
    if (take > keep) take = keep;
    if (take == 0) return a_dup("hermes-bot");
    return a_dup_strip(nickname, take, 0, 0);
}

json_t *w33_adapter_eaa5eb_p2_standalone_nick_base(const json_t *req) {
    char *base = a_nick_base(a_str(req, "nickname", ""));
    json_t *out;
    if (!base) return json_null();
    out = json_string(base);
    free(base);
    return out;
}

/* f"{base}-cron[-<attempts>]"[:30].  The retry ladder stops after
 * W33_ADAPTER_EAA5EB_P2_NICK_ATTEMPTS, so a larger request is the last rung. */
static char *a_standalone_nick(const char *base, long attempts) {
    abuf b = {NULL, 0, 0};
    char *raw, *out;
    size_t take;
    if (attempts > (long)W33_ADAPTER_EAA5EB_P2_NICK_ATTEMPTS)
        attempts = (long)W33_ADAPTER_EAA5EB_P2_NICK_ATTEMPTS;
    ab_puts(&b, base ? base : "");
    ab_puts(&b, "-cron");
    if (attempts > 0) {
        char num[32];
        snprintf(num, sizeof(num), "-%ld", attempts);
        ab_puts(&b, num);
    }
    raw = ab_take(&b);
    if (!raw) return NULL;
    take = a_u8off(raw, W33_ADAPTER_EAA5EB_P2_NICK_MAX);
    out = a_dup_strip(raw, take, 0, 0);
    free(raw);
    return out;
}

json_t *w33_adapter_eaa5eb_p2_standalone_nick(const json_t *req) {
    char *nick = a_standalone_nick(a_str(req, "nick_base", ""),
                                   (long)json_get_num(req, "attempts", 0.0));
    json_t *out;
    if (!nick) return json_null();
    out = json_string(nick);
    free(nick);
    return out;
}

json_t *w33_adapter_eaa5eb_p2_nick_retry(const json_t *req) {
    const char *base = a_str(req, "nick_base", "");
    long collisions = (long)json_get_num(req, "collisions", 0.0);
    long attempts = 0;
    char *nick = a_standalone_nick(base, 0);
    json_t *out = json_object();
    if (!out) {
        free(nick);
        return json_null();
    }
    while (collisions > 0) {
        attempts++;
        collisions--;
        if (attempts > (long)W33_ADAPTER_EAA5EB_P2_NICK_ATTEMPTS) {
            /* The last rung's NICK was already written; only 001 is missing. */
            json_set(out, "ok", json_bool(0));
            json_set(out, "nick", nick ? json_string(nick) : json_null());
            json_set(out, "detail", json_string("too many nick collisions"));
            free(nick);
            return out;
        }
        free(nick);
        nick = a_standalone_nick(base, attempts);
        if (!nick) return out;
    }
    json_set(out, "ok", json_bool(1));
    json_set(out, "nick", nick ? json_string(nick) : json_null());
    json_set(out, "detail", json_null());
    free(nick);
    return out;
}

/* int(value) for a stripped decimal string (Python int() subset). */
static int a_parse_int(const char *s, long *out) {
    const char *p = s;
    int neg = 0, any = 0;
    long v = 0;
    if (!p || !*p) return 0;
    if (*p == '+' || *p == '-') {
        neg = *p == '-';
        p++;
    }
    while (*p) {
        if (*p < '0' || *p > '9') return 0;
        v = v * 10 + (*p - '0');
        any = 1;
        p++;
    }
    if (!any) return 0;
    *out = neg ? -v : v;
    return 1;
}

static json_t *a_target_error(const char *detail) {
    json_t *out = json_object();
    if (!out) return json_null();
    json_set(out, "target", json_null());
    json_set(out, "nick_base", json_null());
    json_set(out, "error", detail ? json_string(detail) : json_null());
    return out;
}

json_t *w33_adapter_eaa5eb_p2_standalone_target(const json_t *req) {
    const char *channel = a_str(req, "channel", "");
    const char *chat_id = a_str(req, "chat_id", "");
    const char *nickname = a_str(req, "nickname", "");
    const json_t *portj = json_obj_get(req, "port");
    const char *target;
    char *target_dup, *base;
    json_t *out;

    if (!a_truthy(json_obj_get(req, "server")) || !a_truthy(json_obj_get(req, "channel")))
        return a_target_error("IRC_SERVER and IRC_CHANNEL must be configured");

    /* int(port_value); a string that does not parse is the invalid-port error. */
    if (portj && portj->type == JSON_STRING) {
        const char *raw = portj->str_val ? portj->str_val : "";
        char *stripped = a_dup_strip(raw, strlen(raw), 1, 1);
        long parsed;
        int ok = stripped ? a_parse_int(stripped, &parsed) : 0;
        free(stripped);
        if (!ok) {
            abuf b = {NULL, 0, 0};
            char *msg;
            json_t *err;
            ab_puts(&b, "invalid port '");
            ab_puts(&b, raw);
            ab_putc(&b, '\'');
            msg = ab_take(&b);
            err = a_target_error(msg);
            free(msg);
            return err;
        }
    } else if (portj && portj->type == JSON_NULL) {
        return a_target_error("invalid port None");
    }

    target = (chat_id && chat_id[0]) ? chat_id : channel;
    {
        const char *p;
        for (p = target; *p; p++)
            if (*p == '\r' || *p == '\n' || *p == ' ')
                return a_target_error("chat_id contains illegal IRC characters");
    }

    target_dup = a_dup(target);
    base = a_nick_base((nickname && nickname[0]) ? nickname : "hermes-bot");
    out = json_object();
    if (!out) {
        free(target_dup);
        free(base);
        return json_null();
    }
    json_set(out, "target", target_dup ? json_string(target_dup) : json_null());
    json_set(out, "nick_base", base ? json_string(base) : json_null());
    json_set(out, "error", json_null());
    free(target_dup);
    free(base);
    return out;
}

json_t *w33_adapter_eaa5eb_p2_standalone_lines(const json_t *req) {
    const char *message = a_str(req, "message", "");
    const char *target = a_str(req, "target", "");
    char *plain = a_strip_markdown_c(message);
    json_t *paragraphs, *lines, *out;
    size_t i, n, start = 0;

    if (!plain) return json_null();
    paragraphs = json_array();
    if (!paragraphs) {
        free(plain);
        return json_null();
    }
    /* [q for q in (_strip_irc_control_chars(p).rstrip() for p in plain.split("\n")) if q] */
    n = strlen(plain);
    for (i = 0; i <= n; i++) {
        if (i == n || plain[i] == '\n') {
            char *sane = a_dup_strip(plain + start, i - start, 0, 0);
            if (sane) {
                char *p2;
                for (p2 = sane; *p2; p2++)
                    if (*p2 == '\r' || *p2 == '\n') *p2 = ' ';
                {
                    char *q = a_dup_strip(sane, strlen(sane), 0, 1);
                    if (q && q[0]) json_append(paragraphs, json_string(q));
                    free(q);
                }
            }
            free(sane);
            start = i + 1;
        }
    }

    lines = a_split_lines(paragraphs, a_privmsg_budget(target));
    json_free(paragraphs);
    free(plain);
    if (!lines) return json_null();
    out = json_object();
    if (!out) {
        json_free(lines);
        return json_null();
    }
    json_set(out, "lines", lines);
    if (json_len(lines) == 0)
        json_set(out, "error", json_string("empty message after stripping"));
    else
        json_set(out, "error", json_null());
    return out;
}

json_t *w33_adapter_eaa5eb_p2_is_channel(const json_t *req) {
    const char *target = a_str(req, "target", "");
    char c = target[0];
    return json_bool(target[0] && (c == '#' || c == '&' || c == '+' || c == '!'));
}

json_t *w33_adapter_eaa5eb_p2_chat_info(const json_t *req) {
    const char *chat_id = a_str(req, "chat_id", "");
    json_t *out = json_object();
    int group = chat_id[0] == '#' || chat_id[0] == '&';
    if (!out) return json_null();
    json_set(out, "name", json_string(chat_id));
    json_set(out, "type", json_string(group ? "group" : "dm"));
    return out;
}

/* ── p2: env -> extra key derivation ───────────────────────────────────── */

/* A raw (unstripped) env string; *found is set when the key holds a string. */
static char *a_env_raw(const json_t *env, const char *name, int *found) {
    const json_t *v = json_obj_get(env, name);
    if (found) *found = 0;
    if (!v || v->type != JSON_STRING) return NULL;
    if (found) *found = 1;
    return a_dup(v->str_val ? v->str_val : "");
}

/* str(env.get(name, "") or "").strip(), the seed_extra_from_env read. */
static char *a_env_str(const json_t *env, const char *name) {
    const json_t *v = json_obj_get(env, name);
    if (!v || v->type == JSON_NULL) return a_dup("");
    if (v->type == JSON_STRING)
        return a_dup_strip(v->str_val ? v->str_val : "",
                           v->str_val ? strlen(v->str_val) : 0, 1, 1);
    return NULL;   /* non-string env value: outside the ported corpus */
}

json_t *w33_adapter_eaa5eb_p2_env_enablement(const json_t *req) {
    const json_t *env = json_obj_get(req, "env");
    static const struct { const char *env; const char *key; } rows[5] = {
        {"IRC_PORT", "port"},
        {"IRC_NICKNAME", "nickname"},
        {"IRC_USE_TLS", "use_tls"},
        {"IRC_SERVER_PASSWORD", "server_password"},
        {"IRC_NICKSERV_PASSWORD", "nickserv_password"},
    };
    char *server, *channel, *home, *name;
    json_t *out;
    size_t i;

    server = a_env_str(env, "IRC_SERVER");
    channel = a_env_str(env, "IRC_CHANNEL");
    if (!server || !channel || !server[0] || !channel[0]) {
        free(server);
        free(channel);
        return json_null();
    }
    out = json_object();
    if (!out) {
        free(server);
        free(channel);
        return json_null();
    }
    json_set(out, "server", json_string(server));
    json_set(out, "channel", json_string(channel));

    for (i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        char *raw = a_env_str(env, rows[i].env);
        if (!raw || !raw[0]) {
            free(raw);
            continue;
        }
        if (!strcmp(rows[i].key, "port")) {
            long parsed;
            if (a_parse_int(raw, &parsed)) json_set(out, rows[i].key, json_number((double)parsed));
        } else if (!strcmp(rows[i].key, "use_tls")) {
            json_set(out, rows[i].key,
                     json_bool(a_lower_eq(raw, "1") || a_lower_eq(raw, "true") ||
                               a_lower_eq(raw, "yes")));
        } else {
            json_set(out, rows[i].key, json_string(raw));
        }
        free(raw);
    }

    home = a_env_str(env, "IRC_HOME_CHANNEL");
    if (!home || !home[0]) {
        free(home);
        home = a_dup(channel);
    }
    if (home && home[0]) {
        json_t *hc = json_object();
        int named = 0;
        name = a_env_raw(env, "IRC_HOME_CHANNEL_NAME", &named);
        if (!named) {
            free(name);
            name = a_dup("Home");
        }
        if (hc) {
            json_set(hc, "chat_id", json_string(home));
            json_set(hc, "name", json_string(name ? name : "Home"));
            json_set(out, "home_channel", hc);
        }
        free(name);
    }
    free(home);
    free(server);
    free(channel);
    return out;
}

/* _env_or_extra: env wins when truthy, else extra.get(key, default). */
static json_t *a_env_or_extra(const json_t *extra, const json_t *env, const char *name,
                              const char *key) {
    const json_t *v = env ? json_obj_get(env, name) : NULL;
    if (a_truthy(v)) return json_copy(v);
    v = extra ? json_obj_get(extra, key) : NULL;
    if (v) return json_copy(v);
    return json_string("");
}

json_t *w33_adapter_eaa5eb_p2_server_channel(const json_t *req) {
    const json_t *extra = json_obj_get(req, "extra");
    const json_t *env = json_obj_get(req, "env");
    json_t *out = json_object();
    if (!out) return json_null();
    json_set(out, "server", a_env_or_extra(extra, env, "IRC_SERVER", "server"));
    json_set(out, "channel", a_env_or_extra(extra, env, "IRC_CHANNEL", "channel"));
    return out;
}

json_t *w33_adapter_eaa5eb_p2_config_ready(const json_t *req) {
    const json_t *extra = json_obj_get(req, "extra");
    const json_t *env = json_obj_get(req, "env");
    const json_t *server = env ? json_obj_get(env, "IRC_SERVER") : NULL;
    const json_t *channel = env ? json_obj_get(env, "IRC_CHANNEL") : NULL;
    if (!a_truthy(server)) server = extra ? json_obj_get(extra, "server") : NULL;
    if (!a_truthy(channel)) channel = extra ? json_obj_get(extra, "channel") : NULL;
    return json_bool(a_truthy(server) && a_truthy(channel));
}
