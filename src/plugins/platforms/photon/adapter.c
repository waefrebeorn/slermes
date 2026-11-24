/*
 * adapter.c — C11 home for upstream Hermes plugins/platforms/photon/adapter.py
 *             (gates w27_adapter_878564_p1 .. p5).
 *
 * The pure decision core of the Photon/iMessage adapter: markdown→plain text
 * shaping, URL/rich-link classification, inbound content predicates and their
 * labels, the normalize ladder (text/type/media_urls/media_types), outbound
 * payload bodies, chat/space id keys, sidecar error typing, retry classification
 * and the presence/mention gates.  Every I/O seam (the Node sidecar socket, the
 * runtime record file, the media cache write, the sent-text index, the clock,
 * the PHOTON_* env reads) stays in the caller; this file owns only what the
 * values decide.
 *
 * PoP: w27_adapter_878564_p1_strip_markdown @ gateway/platforms/helpers.py:strip_markdown
 *      (the one foreign pure helper the photon text path cannot be decided
 *       without — reached as adapter.py:strip_markdown; ported, proven in p1)
 * PoP: w27_adapter_878564_p1_format_message @ plugins/platforms/photon/adapter.py:PhotonAdapter.format_message
 * PoP: w27_adapter_878564_p1_sidecar_payload_text @ plugins/platforms/photon/adapter.py:_sidecar_payload_text
 * PoP: w27_adapter_878564_p1_url_only_candidate @ plugins/platforms/photon/adapter.py:_url_only_candidate
 * PoP: w27_adapter_878564_p1_richlink_candidate @ plugins/platforms/photon/adapter.py:_richlink_candidate
 * PoP: w27_adapter_878564_p1_format_richlink_content @ plugins/platforms/photon/adapter.py:_format_richlink_content
 * PoP: w27_adapter_878564_p2_is_voice_payload @ plugins/platforms/photon/adapter.py:_is_voice_payload
 * PoP: w27_adapter_878564_p2_cached_media_type @ plugins/platforms/photon/adapter.py:_cached_media_type
 * PoP: w27_adapter_878564_p2_attachment_message_type @ plugins/platforms/photon/adapter.py:_attachment_message_type
 * PoP: w27_adapter_878564_p2_group_item_contents @ plugins/platforms/photon/adapter.py:_group_item_contents
 * PoP: w27_adapter_878564_p2_richlink_url_from_content @ plugins/platforms/photon/adapter.py:_richlink_url_from_content
 * PoP: w27_adapter_878564_p2_is_richlink_preview_attachment @ plugins/platforms/photon/adapter.py:_is_richlink_preview_attachment
 * PoP: w27_adapter_878564_p2_richlink_preview_label @ plugins/platforms/photon/adapter.py:_richlink_preview_label
 * PoP: w27_adapter_878564_p2_is_richlink_preview_content @ plugins/platforms/photon/adapter.py:_is_richlink_preview_content
 * PoP: w27_adapter_878564_p2_reply_inner @ plugins/platforms/photon/adapter.py:_reply_inner
 * PoP: w27_adapter_878564_p2_carries_voice @ plugins/platforms/photon/adapter.py:_carries_voice
 * PoP: w27_adapter_878564_p2_mention_gate_text @ plugins/platforms/photon/adapter.py:_mention_gate_text
 * PoP: w27_adapter_878564_p3_normalize_binary_payload @ plugins/platforms/photon/adapter.py:_normalize_binary_payload
 * PoP: w27_adapter_878564_p3_normalize_group_content @ plugins/platforms/photon/adapter.py:_normalize_group_content
 * PoP: w27_adapter_878564_p3_normalize_content @ plugins/platforms/photon/adapter.py:_normalize_content
 * PoP: w27_adapter_878564_p3_attachment_body @ plugins/platforms/photon/adapter.py:_attachment_body
 * PoP: w27_adapter_878564_p3_attachment_label @ plugins/platforms/photon/adapter.py:_attachment_label
 * PoP: w27_adapter_878564_p3_send_plan @ plugins/platforms/photon/adapter.py:PhotonAdapter._sidecar_send
 * PoP: w27_adapter_878564_p3_send_poll_plan @ plugins/platforms/photon/adapter.py:PhotonAdapter._sidecar_send_poll
 * PoP: w27_adapter_878564_p4_normalize_chat_key @ plugins/platforms/photon/adapter.py:PhotonAdapter._normalize_chat_key
 * PoP: w27_adapter_878564_p4_sent_text_key @ plugins/platforms/photon/adapter.py:_sent_text_key
 * PoP: w27_adapter_878564_p4_is_dm_guid @ plugins/platforms/photon/adapter.py:_SEND_DM_GUID_RE
 * PoP: w27_adapter_878564_p4_sidecar_error_from_response @ plugins/platforms/photon/adapter.py:_sidecar_error_from_response
 * PoP: w27_adapter_878564_p4_retryable_error @ plugins/platforms/photon/adapter.py:PhotonAdapter._is_retryable_error
 * PoP: w27_adapter_878564_p4_is_permanent_sidecar_failure @ plugins/platforms/photon/adapter.py:PhotonAdapter._is_permanent_sidecar_failure
 * PoP: w27_adapter_878564_p4_send_retry_is_final @ plugins/platforms/photon/adapter.py:PhotonAdapter._send_retry_is_final
 * PoP: w27_adapter_878564_p4_constants @ plugins/platforms/photon/adapter.py:<module>
 * PoP: w27_adapter_878564_p4_parse_timestamp @ plugins/platforms/photon/adapter.py:_parse_timestamp
 * PoP: w27_adapter_878564_p5_mention_pattern_texts @ plugins/platforms/photon/adapter.py:PhotonAdapter._compile_mention_patterns
 * PoP: w27_adapter_878564_p5_mention_match @ plugins/platforms/photon/adapter.py:PhotonAdapter._message_matches_mention_patterns
 * PoP: w27_adapter_878564_p5_clean_mention_text @ plugins/platforms/photon/adapter.py:PhotonAdapter._clean_mention_text
 * PoP: w27_adapter_878564_p5_gate_stt_chat @ plugins/platforms/photon/adapter.py:PhotonAdapter._gate_stt_chat
 * PoP: w27_adapter_878564_p5_require_mention_flag @ plugins/platforms/photon/adapter.py:PhotonAdapter.__init__
 * PoP: w27_adapter_878564_p5_markdown_enabled @ plugins/platforms/photon/adapter.py:_markdown_enabled
 * PoP: w27_adapter_878564_p5_reactions_enabled @ plugins/platforms/photon/adapter.py:PhotonAdapter._reactions_enabled
 * PoP: w27_adapter_878564_p5_autostart_enabled @ plugins/platforms/photon/adapter.py:PhotonAdapter.__init__
 * PoP: w27_adapter_878564_p5_is_recent_richlink_preview @ plugins/platforms/photon/adapter.py:PhotonAdapter._is_recent_richlink_preview
 * PoP: w27_adapter_878564_p5_record_recent_richlink @ plugins/platforms/photon/adapter.py:PhotonAdapter._record_recent_richlink
 *
 * Seams (injected by the harness, never decided here): the media-cache write
 * (_cache_inbound_attachment → a path or None, consumed per binary payload), the
 * PHOTON_* env values (markdown/reactions/autostart/require_mention), the base
 * adapter's retryable verdict, the clock behind the rich-link preview window and
 * the timestamp fallback, and the compiled regex objects behind the mention gate.
 *
 * Corpus restrictions (documented, not silently assumed):
 *  - Text predicates assume ASCII case folding (Python's .lower() is unicode-aware).
 *  - A URL whose authority carries unbalanced '[' / ']' raises in urlparse and is
 *    out of corpus (the flow1 corpus documents the same exclusion).
 *  - str(value) on containers is out of corpus for the label/format helpers.
 *  - Mention patterns use the wake-word subset the adapter ships by default:
 *    (?<![\w@]) @? \s+ \s* \b [,:;-]? and literals.
 *  - datetime.fromisoformat is the ISO-8601 subset Python 3.11+ accepts for the
 *    forms in the fixtures (date, T/space separator, Z/±HH:MM, 1-6 frac digits).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include "hermes_json.h"
#include "noc_w27_adapter_878564_p1.h"
#include "noc_w27_adapter_878564_p2.h"
#include "noc_w27_adapter_878564_p3.h"
#include "noc_w27_adapter_878564_p4.h"
#include "noc_w27_adapter_878564_p5.h"

/* ------------------------------------------------------------------ helpers */

static char *a_dup(const char *s) {
    size_t n;
    char *p;
    if (!s) return NULL;
    n = strlen(s);
    p = (char *)malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

static bool a_isspace(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static bool a_is_word(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}

/* str.strip() over the ASCII whitespace set.  Caller frees. */
static char *a_strip(const char *s) {
    size_t n, i, j;
    char *p;
    if (!s) return a_dup("");
    n = strlen(s);
    i = 0;
    while (i < n && a_isspace((unsigned char)s[i])) i++;
    j = n;
    while (j > i && a_isspace((unsigned char)s[j - 1])) j--;
    p = (char *)malloc(j - i + 1);
    if (!p) return NULL;
    memcpy(p, s + i, j - i);
    p[j - i] = '\0';
    return p;
}

/* str.lstrip(chars) with an explicit char set.  Caller frees. */
static char *a_lstrip_chars(const char *s, const char *chars) {
    size_t i = 0;
    if (!s) return a_dup("");
    while (s[i] && strchr(chars, s[i])) i++;
    return a_dup(s + i);
}

/* str.lstrip() (ASCII whitespace).  Caller frees. */
static char *a_lstrip_ws(const char *s) {
    size_t i = 0;
    if (!s) return a_dup("");
    while (s[i] && a_isspace((unsigned char)s[i])) i++;
    return a_dup(s + i);
}

static char *a_lower(const char *s) {
    size_t n, i;
    char *p;
    if (!s) return a_dup("");
    n = strlen(s);
    p = (char *)malloc(n + 1);
    if (!p) return NULL;
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        p[i] = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : (char)c;
    }
    p[n] = '\0';
    return p;
}

static bool a_starts_with(const char *s, const char *pre) {
    return s && pre && strncmp(s, pre, strlen(pre)) == 0;
}

static bool a_ends_with(const char *s, const char *suf) {
    size_t n, m;
    if (!s || !suf) return false;
    n = strlen(s);
    m = strlen(suf);
    return n >= m && memcmp(s + n - m, suf, m) == 0;
}

static bool a_ci_eq(unsigned char a, unsigned char b) {
    if (a >= 'A' && a <= 'Z') a = (unsigned char)(a - 'A' + 'a');
    if (b >= 'A' && b <= 'Z') b = (unsigned char)(b - 'A' + 'a');
    return a == b;
}

/* http:// or https:// at index `at`, case-insensitively. */
static bool a_http_at(const char *s, size_t at) {
    static const char *pre = "http";
    size_t k;
    for (k = 0; k < 4; k++) {
        if (!s[at + k] || !a_ci_eq((unsigned char)s[at + k], (unsigned char)pre[k])) return false;
    }
    k = (s[at + 4] == 's' || s[at + 4] == 'S') ? 5 : 4;
    return s[at + k] == ':' && s[at + k + 1] == '/' && s[at + k + 2] == '/';
}

/* ^https?:// (IGNORECASE) */
static bool a_http_prefix(const char *s) {
    return s && a_http_at(s, 0);
}

/* Python len(): UTF-8 code points. */
static size_t a_utf8_len(const char *s) {
    size_t n = 0;
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    while (*p) {
        if ((*p & 0xC0) != 0x80) n++;
        p++;
    }
    return n;
}

/* s[:limit] on code points.  Caller frees. */
static char *a_utf8_slice(const char *s, size_t limit) {
    size_t n = 0;
    const char *p = s ? s : "";
    const char *start = p;
    while (*p) {
        if (((unsigned char)*p & 0xC0) != 0x80) {
            if (n == limit) break;
            n++;
        }
        p++;
    }
    {
        size_t len = (size_t)(p - start);
        char *out = (char *)malloc(len + 1);
        if (!out) return NULL;
        memcpy(out, start, len);
        out[len] = '\0';
        return out;
    }
}

/* strip a freshly allocated string in place: returns a new allocation and frees
 * the input (NULL-safe). */
static char *a_strip_take(char *s) {
    char *out;
    if (!s) return NULL;
    out = a_strip(s);
    free(s);
    return out;
}

/* Python str(number) for the scalar shapes the adapter stringifies. */
static char *a_num_repr(double v) {
    char buf[64];
    if (!isfinite(v)) {
        if (isnan(v)) return a_dup("nan");
        return a_dup(v > 0 ? "inf" : "-inf");
    }
    if (v == (double)(long long)v && fabs(v) < 1e18) {
        snprintf(buf, sizeof(buf), "%.0f", v);
        return a_dup(buf);
    }
    snprintf(buf, sizeof(buf), "%.15g", v);
    if (strtod(buf, NULL) != v) snprintf(buf, sizeof(buf), "%.17g", v);
    return a_dup(buf);
}

static char *a_pystr(const json_t *v, const char *def) {
    if (!v || v->type == JSON_NULL) return a_dup(def ? def : "");
    if (v->type == JSON_STRING) return a_dup(v->str_val ? v->str_val : "");
    if (v->type == JSON_BOOL) return a_dup(v->bool_val ? "True" : "False");
    if (v->type == JSON_NUMBER) return a_num_repr(v->num_val);
    return a_dup(def ? def : "");
}

static const char *a_jstr(const json_t *v) {
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}

static bool a_jtrue(const json_t *v) {
    if (!v) return false;
    if (v->type == JSON_NULL) return false;
    if (v->type == JSON_BOOL) return v->bool_val;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_STRING) return v->str_val && v->str_val[0];
    return v->c.count > 0;
}

/* ------------------------------------------------------------------------- */
/* dynamic string buffer                                                      */
/* ------------------------------------------------------------------------- */

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} a_buf;

static void a_buf_init(a_buf *b) {
    b->buf = (char *)malloc(64);
    b->len = 0;
    b->cap = 64;
    if (b->buf) b->buf[0] = '\0';
}

static void a_buf_addn(a_buf *b, const char *s, size_t n) {
    if (!b->buf || !s || n == 0) return;
    if (b->len + n + 1 > b->cap) {
        size_t cap = b->cap ? b->cap : 64;
        char *nb;
        while (cap < b->len + n + 1) cap *= 2;
        nb = (char *)realloc(b->buf, cap);
        if (!nb) return;
        b->buf = nb;
        b->cap = cap;
    }
    memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = '\0';
}

static void a_buf_add(a_buf *b, const char *s) { a_buf_addn(b, s, s ? strlen(s) : 0); }

static void a_buf_addc(a_buf *b, char c) { a_buf_addn(b, &c, 1); }

/* ------------------------------------------------------------------------- */
/* p1 — text, markdown, urls                                                  */
/* ------------------------------------------------------------------------- */

/* `\*\*(.+?)\*\*` and friends: replace delim..delim with its body (non-greedy,
 * leftmost, DOTALL).  When `word_semantics` (the __/_ rules), the opening
 * delimiter must sit on a word boundary and must not be followed by whitespace
 * or the delimiter char; the closing one must not follow whitespace/delim and
 * must end on a word boundary. */
static char *a_md_pair(const char *text, const char *delim, bool word_semantics) {
    size_t n = strlen(text);
    size_t dlen = strlen(delim);
    a_buf out;
    size_t i = 0;
    a_buf_init(&out);
    if (!out.buf) return a_dup("");
    while (i < n) {
        size_t start = i;
        bool found_start = false;
        while (start + dlen <= n) {
            if (memcmp(text + start, delim, dlen) == 0) {
                if (!word_semantics) { found_start = true; break; }
                if (start > 0 && a_is_word((unsigned char)text[start - 1])) { start++; continue; }
                if (start + dlen >= n) { start++; continue; }
                {
                    unsigned char nxt = (unsigned char)text[start + dlen];
                    if (a_isspace(nxt) || nxt == (unsigned char)delim[0]) { start++; continue; }
                }
                found_start = true;
                break;
            }
            start++;
        }
        if (!found_start) {
            a_buf_add(&out, text + i);
            break;
        }
        {
            size_t j = start + dlen + 1; /* `.+?` needs at least one body char */
            bool found_close = false;
            size_t close_at = 0;
            while (j + dlen <= n) {
                if (memcmp(text + j, delim, dlen) == 0) {
                    if (!word_semantics) { found_close = true; close_at = j; break; }
                    if (j > start + dlen) {
                        unsigned char prv = (unsigned char)text[j - 1];
                        unsigned char aft = (j + dlen < n) ? (unsigned char)text[j + dlen] : 0;
                        bool ok_prv = !(a_isspace(prv) || prv == (unsigned char)delim[0]);
                        bool ok_aft = (j + dlen >= n) || !a_is_word(aft);
                        if (ok_prv && ok_aft) { found_close = true; close_at = j; break; }
                    }
                }
                j++;
            }
            if (!found_close) {
                a_buf_addn(&out, text + i, start + dlen - i);
                i = start + dlen;
                continue;
            }
            a_buf_addn(&out, text + i, start - i);
            a_buf_addn(&out, text + start + dlen, close_at - (start + dlen));
            i = close_at + dlen;
        }
    }
    return out.buf;
}

/* ``` + [A-Za-z0-9_+-]* + optional '\n'  → "" */
static char *a_md_fence(const char *text) {
    size_t n = strlen(text);
    a_buf out;
    size_t i = 0;
    a_buf_init(&out);
    if (!out.buf) return a_dup("");
    while (i < n) {
        if (i + 3 <= n && memcmp(text + i, "```", 3) == 0) {
            size_t j = i + 3;
            while (j < n && (isalnum((unsigned char)text[j]) || text[j] == '_' ||
                             text[j] == '+' || text[j] == '-')) j++;
            if (j < n && text[j] == '\n') j++;
            i = j;
            continue;
        }
        a_buf_addc(&out, text[i]);
        i++;
    }
    return out.buf;
}

/* `^#{1,6}\s+` MULTILINE → "" */
static char *a_md_headings(const char *text) {
    size_t n = strlen(text);
    a_buf out;
    size_t i = 0;
    bool line_start = true;
    a_buf_init(&out);
    if (!out.buf) return a_dup("");
    while (i < n) {
        if (line_start && text[i] == '#') {
            size_t j = i;
            while (j < n && text[j] == '#' && j - i < 6) j++;
            if (j < n && a_isspace((unsigned char)text[j])) {
                while (j < n && a_isspace((unsigned char)text[j])) j++;
                i = j;
                line_start = false;
                continue;
            }
        }
        a_buf_addc(&out, text[i]);
        line_start = (text[i] == '\n');
        i++;
    }
    return out.buf;
}

/* `\[([^\]]+)\]\(([^\)]+)\)` global sub. */
static char *a_md_links(const char *text, bool keep_link_targets) {
    size_t n = strlen(text);
    a_buf out;
    size_t i = 0;
    a_buf_init(&out);
    if (!out.buf) return a_dup("");
    while (i < n) {
        if (text[i] == '[') {
            size_t lab_end = i + 1;
            while (lab_end < n && text[lab_end] != ']') lab_end++;
            if (lab_end < n && lab_end > i + 1 && lab_end + 1 < n && text[lab_end + 1] == '(') {
                size_t tgt_end = lab_end + 2;
                while (tgt_end < n && text[tgt_end] != ')') tgt_end++;
                if (tgt_end < n && tgt_end > lab_end + 2) {
                    if (!keep_link_targets) {
                        a_buf_addn(&out, text + i + 1, lab_end - i - 1);
                    } else {
                        size_t lz = i + 1, le = lab_end;
                        size_t tz = lab_end + 2, te = tgt_end;
                        char *label, *target;
                        while (lz < le && a_isspace((unsigned char)text[lz])) lz++;
                        while (le > lz && a_isspace((unsigned char)text[le - 1])) le--;
                        while (tz < te && a_isspace((unsigned char)text[tz])) tz++;
                        while (te > tz && a_isspace((unsigned char)text[te - 1])) te--;
                        label = (char *)malloc(le - lz + 1);
                        target = (char *)malloc(te - tz + 1);
                        if (!label || !target) {
                            free(label); free(target);
                            a_buf_addc(&out, text[i]);
                            i++;
                            continue;
                        }
                        memcpy(label, text + lz, le - lz);
                        label[le - lz] = '\0';
                        memcpy(target, text + tz, te - tz);
                        target[te - tz] = '\0';
                        if (!a_http_prefix(target)) {
                            /* non-http target: label-only, raw group(1) */
                            a_buf_addn(&out, text + i + 1, lab_end - i - 1);
                        } else if (strcmp(label, target) == 0) {
                            a_buf_add(&out, target);
                        } else {
                            a_buf_add(&out, label);
                            a_buf_addc(&out, '\n');
                            a_buf_add(&out, target);
                        }
                        free(label);
                        free(target);
                    }
                    i = tgt_end + 1;
                    continue;
                }
            }
        }
        a_buf_addc(&out, text[i]);
        i++;
    }
    return out.buf;
}

char *w27_adapter_878564_p1_strip_markdown(const char *text, bool keep_link_targets) {
    char *s1, *s2, *s3, *s4, *s5, *s6, *s7, *s8;
    char *res;
    if (!text) text = "";
    s1 = a_md_pair(text, "**", false);            if (!s1) return NULL;
    s2 = a_md_pair(s1, "*", false);               free(s1); if (!s2) return NULL;
    s3 = a_md_pair(s2, "__", true);               free(s2); if (!s3) return NULL;
    s4 = a_md_pair(s3, "_", true);                free(s3); if (!s4) return NULL;
    s5 = a_md_fence(s4);                          free(s4); if (!s5) return NULL;
    s6 = a_md_pair(s5, "`", false);               free(s5); if (!s6) return NULL;
    s7 = a_md_headings(s6);                       free(s6); if (!s7) return NULL;
    s8 = a_md_links(s7, keep_link_targets);       free(s7); if (!s8) return NULL;
    /* `\n{3,}` -> "\n\n" then .strip() */
    {
        a_buf out;
        size_t i = 0, n = strlen(s8);
        a_buf_init(&out);
        if (!out.buf) { free(s8); return NULL; }
        while (i < n) {
            if (s8[i] == '\n') {
                size_t j = i;
                while (j < n && s8[j] == '\n') j++;
                if (j - i >= 3) a_buf_add(&out, "\n\n");
                else a_buf_addn(&out, s8 + i, j - i);
                i = j;
                continue;
            }
            a_buf_addc(&out, s8[i]);
            i++;
        }
        free(s8);
        res = a_strip(out.buf);
        free(out.buf);
    }
    return res;
}

char *w27_adapter_878564_p1_format_message(const char *content, bool markdown_enabled) {
    if (markdown_enabled) return a_dup(content ? content : "");
    return w27_adapter_878564_p1_strip_markdown(content, true);
}

/* `_SIDECAR_URL_RE = https?://[^\s)'"<>]+` — search. */
static bool a_has_sidecar_url(const char *text) {
    size_t i, n;
    if (!text) return false;
    n = strlen(text);
    for (i = 0; i + 4 <= n; i++) {
        if (a_http_at(text, i)) {
            size_t j = i + ((text[i + 4] == 's' || text[i + 4] == 'S') ? 8 : 7);
            if (j < n && !a_isspace((unsigned char)text[j]) && text[j] != ')' &&
                text[j] != '\'' && text[j] != '"' && text[j] != '<' && text[j] != '>')
                return true;
        }
    }
    return false;
}

char *w27_adapter_878564_p1_sidecar_payload_text(const char *text, bool send_markdown) {
    if (send_markdown && !a_has_sidecar_url(text)) return a_dup(text ? text : "");
    return w27_adapter_878564_p1_strip_markdown(text, true);
}

/* re.fullmatch(r"https?://\S+", candidate, I) + urlparse scheme/netloc checks. */
char *w27_adapter_878564_p1_url_only_candidate(const char *text) {
    char *candidate = a_strip(text);
    size_t n, i, scheme_end;
    bool ok = false;
    if (!candidate) return NULL;
    n = strlen(candidate);
    if (a_http_prefix(candidate)) {
        scheme_end = (candidate[4] == 's' || candidate[4] == 'S') ? 8 : 7;
        if (scheme_end < n) {
            ok = true;
            for (i = 0; i < n; i++) {
                if (a_isspace((unsigned char)candidate[i])) { ok = false; break; }
            }
        }
    }
    if (ok) {
        size_t auth_start = scheme_end;
        size_t auth_end = auth_start;
        int depth = 0;
        bool bad = false;
        while (auth_end < n && candidate[auth_end] != '/' && candidate[auth_end] != '?' &&
               candidate[auth_end] != '#') auth_end++;
        for (i = auth_start; i < auth_end; i++) {
            if (candidate[i] == '[') { if (depth) { bad = true; break; } depth = 1; }
            else if (candidate[i] == ']') { if (!depth) { bad = true; break; } depth = 0; }
        }
        if (depth) bad = true;
        if (bad || auth_end == auth_start) ok = false;
    }
    if (!ok) { free(candidate); return NULL; }
    return candidate;
}

char *w27_adapter_878564_p1_richlink_candidate(const char *text, bool markdown_enabled) {
    if (!markdown_enabled) return NULL;
    return w27_adapter_878564_p1_url_only_candidate(text);
}

char *w27_adapter_878564_p1_format_richlink_content(const json_t *content) {
    char *url = a_strip_take(a_pystr(json_obj_get(content, "url"), ""));
    char *title = a_strip_take(a_pystr(json_obj_get(content, "title"), ""));
    char *summary = a_strip_take(a_pystr(json_obj_get(content, "summary"), ""));
    a_buf out;
    bool any = false;
    if (!url || !title || !summary) { free(url); free(title); free(summary); return NULL; }
    a_buf_init(&out);
    if (!out.buf) { free(url); free(title); free(summary); return NULL; }
    if (title[0]) { a_buf_add(&out, title); any = true; }
    if (summary[0] && strcmp(summary, title) != 0) {
        if (any) a_buf_addc(&out, '\n');
        a_buf_add(&out, summary);
        any = true;
    }
    if (url[0]) {
        if (any) a_buf_addc(&out, '\n');
        a_buf_add(&out, url);
        any = true;
    }
    free(url); free(title); free(summary);
    if (!any) { free(out.buf); return a_dup("[Photon rich link received with no URL]"); }
    return out.buf;
}

/* ------------------------------------------------------------------------- */
/* p2 — content predicates and labels                                          */
/* ------------------------------------------------------------------------- */

int w27_adapter_878564_p2_is_voice_payload(const json_t *payload) {
    const char *ctype = a_jstr(json_obj_get(payload, "type"));
    if (ctype && strcmp(ctype, "voice") == 0) return 1;
    if (ctype && strcmp(ctype, "attachment") == 0) {
        char *name = a_lower(a_jstr(json_obj_get(payload, "name")));
        const char *mime = a_jstr(json_obj_get(payload, "mimeType"));
        bool caf = name && a_ends_with(name, ".caf");
        bool m = mime && strcmp(mime, "audio/x-caf") == 0;
        free(name);
        return (caf || m) ? 1 : 0;
    }
    return 0;
}

char *w27_adapter_878564_p2_cached_media_type(const json_t *payload, const char *mime,
                                              int is_voice) {
    char *low;
    if (!is_voice) return a_dup((mime && mime[0]) ? mime : "application/octet-stream");
    low = a_lower(mime);
    if (low && a_starts_with(low, "audio/")) { free(low); return a_dup(mime ? mime : ""); }
    free(low);
    {
        char *name = a_lower(a_jstr(json_obj_get(payload, "name")));
        bool caf = name && a_ends_with(name, ".caf");
        free(name);
        return a_dup(caf ? "audio/x-caf" : "audio/mp4");
    }
}

const char *w27_adapter_878564_p2_attachment_message_type(const char *mime) {
    char *low = a_lower(mime);
    const char *out = "document";
    if (low) {
        if (a_starts_with(low, "image/")) out = "photo";
        else if (a_starts_with(low, "video/")) out = "video";
        else if (a_starts_with(low, "audio/")) out = "audio";
    }
    free(low);
    return out;
}

/* content.get("items") or [] keeping only items whose .content is a dict. */
static json_t *a_group_contents(const json_t *content) {
    json_t *out = json_array();
    const json_t *items = json_obj_get(content, "items");
    size_t i;
    if (!out) return NULL;
    if (!items || items->type != JSON_ARRAY) return out;
    for (i = 0; i < items->c.count; i++) {
        const json_t *item = items->c.items[i];
        const json_t *inner;
        if (!item || item->type != JSON_OBJECT) continue;
        inner = json_obj_get(item, "content");
        if (inner && inner->type == JSON_OBJECT) json_append(out, json_copy(inner));
    }
    return out;
}

json_t *w27_adapter_878564_p2_group_item_contents(const json_t *content) {
    return a_group_contents(content);
}

static char *a_richlink_url_from_content(const json_t *content, int depth) {
    const char *ctype = a_jstr(json_obj_get(content, "type"));
    if (depth > 8) return NULL;
    if (ctype && (strcmp(ctype, "text") == 0 || strcmp(ctype, "richlink") == 0)) {
        const char *key = strcmp(ctype, "text") == 0 ? "text" : "url";
        char *raw = a_pystr(json_obj_get(content, key), "");
        char *res = w27_adapter_878564_p1_url_only_candidate(raw);
        free(raw);
        return res;
    }
    if (ctype && strcmp(ctype, "group") == 0) {
        json_t *contents = a_group_contents(content);
        size_t i;
        char *hit = NULL;
        if (!contents) return NULL;
        for (i = 0; i < contents->c.count && !hit; i++)
            hit = a_richlink_url_from_content(contents->c.items[i], depth + 1);
        json_free(contents);
        return hit;
    }
    return NULL;
}

char *w27_adapter_878564_p2_richlink_url_from_content(const json_t *content) {
    return a_richlink_url_from_content(content, 0);
}

int w27_adapter_878564_p2_is_richlink_preview_attachment(const json_t *payload) {
    const char *ctype = a_jstr(json_obj_get(payload, "type"));
    static const char *const keys[] = {"name", "id"};
    size_t k;
    if (!ctype || strcmp(ctype, "attachment") != 0) return 0;
    for (k = 0; k < sizeof(keys) / sizeof(keys[0]); k++) {
        char *s = a_lower(a_pystr(json_obj_get(payload, keys[k]), ""));
        bool hit = s && strstr(s, ".pluginpayloadattachment") != NULL;
        free(s);
        if (hit) return 1;
    }
    return 0;
}

static char *a_label_one(const json_t *c) {
    char *name = a_pystr(json_obj_get(c, "name"), "");
    char *id;
    if (name && name[0]) return name;
    free(name);
    id = a_pystr(json_obj_get(c, "id"), "");
    if (id && id[0]) return id;
    free(id);
    return a_dup("(unnamed)");
}

char *w27_adapter_878564_p2_richlink_preview_label(const json_t *content) {
    const char *ctype = a_jstr(json_obj_get(content, "type"));
    if (ctype && strcmp(ctype, "attachment") == 0) return a_label_one(content);
    if (ctype && strcmp(ctype, "group") == 0) {
        json_t *contents = a_group_contents(content);
        a_buf out;
        size_t i;
        if (!contents) return NULL;
        a_buf_init(&out);
        if (!out.buf) { json_free(contents); return NULL; }
        for (i = 0; i < contents->c.count; i++) {
            char *lab = a_label_one(contents->c.items[i]);
            if (i) a_buf_add(&out, ", ");
            a_buf_add(&out, lab);
            free(lab);
        }
        json_free(contents);
        if (out.len == 0) { free(out.buf); return a_dup("(group)"); }
        return out.buf;
    }
    return a_dup("(unknown)");
}

int w27_adapter_878564_p2_is_richlink_preview_content(const json_t *content) {
    const char *ctype;
    const json_t *items;
    json_t *contents;
    size_t i;
    bool ok = true;
    if (w27_adapter_878564_p2_is_richlink_preview_attachment(content)) return 1;
    ctype = a_jstr(json_obj_get(content, "type"));
    if (!ctype || strcmp(ctype, "group") != 0) return 0;
    items = json_obj_get(content, "items");
    contents = a_group_contents(content);
    if (!contents) return 0;
    if (!a_jtrue(items) || items->type != JSON_ARRAY || contents->c.count != items->c.count) {
        json_free(contents);
        return 0;
    }
    for (i = 0; i < contents->c.count; i++) {
        if (!w27_adapter_878564_p2_is_richlink_preview_attachment(contents->c.items[i])) {
            ok = false;
            break;
        }
    }
    json_free(contents);
    return ok ? 1 : 0;
}

json_t *w27_adapter_878564_p2_reply_inner(const json_t *content) {
    const char *ctype = a_jstr(json_obj_get(content, "type"));
    const json_t *inner;
    if (ctype && strcmp(ctype, "reply") == 0) {
        inner = json_obj_get(content, "content");
        if (inner && inner->type == JSON_OBJECT) return json_copy(inner);
    }
    return content ? json_copy(content) : json_object();
}

int w27_adapter_878564_p2_carries_voice(const json_t *content) {
    const char *ctype = a_jstr(json_obj_get(content, "type"));
    if (ctype && strcmp(ctype, "group") == 0) {
        json_t *contents = a_group_contents(content);
        size_t i;
        int hit = 0;
        if (!contents) return 0;
        for (i = 0; i < contents->c.count; i++) {
            if (w27_adapter_878564_p2_is_voice_payload(contents->c.items[i])) { hit = 1; break; }
        }
        json_free(contents);
        return hit;
    }
    return w27_adapter_878564_p2_is_voice_payload(content);
}

char *w27_adapter_878564_p2_mention_gate_text(const json_t *content) {
    const char *ctype = a_jstr(json_obj_get(content, "type"));
    if (ctype && strcmp(ctype, "text") == 0)
        return a_pystr(json_obj_get(content, "text"), "");
    if (ctype && strcmp(ctype, "richlink") == 0)
        return w27_adapter_878564_p1_format_richlink_content(content);
    if (ctype && strcmp(ctype, "group") == 0) {
        json_t *contents = a_group_contents(content);
        a_buf out;
        size_t i;
        if (!contents) return NULL;
        a_buf_init(&out);
        if (!out.buf) { json_free(contents); return NULL; }
        for (i = 0; i < contents->c.count; i++) {
            char *part = w27_adapter_878564_p2_mention_gate_text(contents->c.items[i]);
            if (part && part[0]) {
                if (out.len) a_buf_addc(&out, '\n');
                a_buf_add(&out, part);
            }
            free(part);
        }
        json_free(contents);
        return out.buf;
    }
    return a_dup("");
}

/* ------------------------------------------------------------------------- */
/* p3 — normalize ladder and outbound bodies                                   */
/* ------------------------------------------------------------------------- */

/* Normalized tuple -> {"text","type","media_urls","media_types"} */
static json_t *a_normalized(char *text, const char *mtype, json_t *urls, json_t *types) {
    json_t *o = json_object();
    if (!o) { free(text); json_free(urls); json_free(types); return NULL; }
    json_set(o, "text", json_string(text ? text : ""));
    json_set(o, "type", json_string(mtype ? mtype : "text"));
    json_set(o, "media_urls", urls ? urls : json_array());
    json_set(o, "media_types", types ? types : json_array());
    free(text);
    return o;
}

/* `[Photon {label} received: {name} ({mime or 'unknown MIME'}{, duration: Ns})]` */
static char *a_binary_marker(const json_t *payload, const char *label) {
    char *name;
    char *mime = a_pystr(json_obj_get(payload, "mimeType"), "");
    const json_t *dur = json_obj_get(payload, "duration");
    a_buf out;
    bool is_voice = strcmp(label, "voice") == 0;
    if (!mime) return NULL;
    name = a_pystr(json_obj_get(payload, "name"), "");
    if (!name || !name[0]) {
        free(name);
        name = a_dup(is_voice ? "voice" : "(unnamed)");
    }
    a_buf_init(&out);
    if (!out.buf) { free(name); free(mime); return NULL; }
    a_buf_add(&out, "[Photon ");
    a_buf_add(&out, label);
    a_buf_add(&out, " received: ");
    a_buf_add(&out, name);
    a_buf_add(&out, " (");
    a_buf_add(&out, mime[0] ? mime : "unknown MIME");
    if (dur && (dur->type == JSON_NUMBER || dur->type == JSON_BOOL)) {
        char *d = a_pystr(dur, "");
        a_buf_add(&out, ", duration: ");
        a_buf_add(&out, d ? d : "");
        a_buf_add(&out, "s");
        free(d);
    }
    a_buf_add(&out, ")]");
    free(name);
    free(mime);
    return out.buf;
}

static json_t *a_normalize_binary(const json_t *payload, const char *cached_path) {
    int is_voice = w27_adapter_878564_p2_is_voice_payload(payload);
    const char *label = is_voice ? "voice" : "attachment";
    const char *mtype;
    if (is_voice) {
        mtype = "voice";
    } else {
        char *mime = a_pystr(json_obj_get(payload, "mimeType"), "");
        mtype = w27_adapter_878564_p2_attachment_message_type(mime);
        free(mime);
    }
    if (cached_path && cached_path[0]) {
        json_t *urls = json_array();
        json_t *types = json_array();
        const char *raw_mime = a_jstr(json_obj_get(payload, "mimeType"));
        char *mt = w27_adapter_878564_p2_cached_media_type(payload, raw_mime ? raw_mime : "",
                                                          is_voice);
        char *text = (char *)malloc(strlen(label) + 3);
        if (!urls || !types || !text) {
            json_free(urls); json_free(types); free(mt); free(text);
            return NULL;
        }
        sprintf(text, "(%s)", label);
        json_append(urls, json_string(cached_path));
        json_append(types, json_string(mt ? mt : ""));
        free(mt);
        return a_normalized(text, mtype, urls, types);
    }
    {
        char *marker = a_binary_marker(payload, label);
        return a_normalized(marker, mtype, json_array(), json_array());
    }
}

json_t *w27_adapter_878564_p3_normalize_binary_payload(const json_t *payload,
                                                       const char *cached_path) {
    return a_normalize_binary(payload, cached_path);
}

/* Cache seam: `cache` is a JSON array of (null | path) consumed in traversal
 * order, one entry per binary payload the ladder reaches. */
typedef struct {
    const json_t *cache;
    size_t cursor;
} a_cache;

static const char *a_cache_next(a_cache *c) {
    const json_t *v;
    if (!c->cache || c->cache->type != JSON_ARRAY) return NULL;
    if (c->cursor >= c->cache->c.count) { c->cursor++; return NULL; }
    v = c->cache->c.items[c->cursor++];
    return a_jstr(v);
}

static void a_parts_push(char ***parts, size_t *n, size_t *cap, char *s) {
    if (*n == *cap) {
        size_t ncap = *cap ? *cap * 2 : 8;
        char **np = (char **)realloc(*parts, ncap * sizeof(char *));
        if (!np) { free(s); return; }
        *parts = np;
        *cap = ncap;
    }
    (*parts)[(*n)++] = s;
}

static json_t *a_normalize_group(const json_t *content, a_cache *cache) {
    json_t *contents = a_group_contents(content);
    char **parts = NULL;
    size_t nparts = 0, capparts = 0;
    json_t *urls = json_array();
    json_t *types = json_array();
    char *mtype = a_dup("text");
    size_t i;
    char *text;
    if (!contents || !urls || !types || !mtype) {
        json_free(contents); json_free(urls); json_free(types); free(mtype);
        return NULL;
    }
    for (i = 0; i < contents->c.count; i++) {
        const json_t *ic = contents->c.items[i];
        const char *it = a_jstr(json_obj_get(ic, "type"));
        if (it && (strcmp(it, "attachment") == 0 || strcmp(it, "voice") == 0)) {
            json_t *norm = a_normalize_binary(ic, cache ? a_cache_next(cache) : NULL);
            const json_t *nl, *nt, *nu, *nty;
            if (!norm) continue;
            nl = json_obj_get(norm, "type");
            nu = json_obj_get(norm, "media_urls");
            nty = json_obj_get(norm, "media_types");
            nt = json_obj_get(norm, "text");
            if (strcmp(mtype, "text") == 0 && a_jstr(nl)) {
                free(mtype);
                mtype = a_dup(a_jstr(nl));
            }
            if (nu && nu->type == JSON_ARRAY) {
                size_t k;
                for (k = 0; k < nu->c.count; k++) json_append(urls, json_copy(nu->c.items[k]));
                if (nty && nty->type == JSON_ARRAY)
                    for (k = 0; k < nty->c.count; k++)
                        json_append(types, json_copy(nty->c.items[k]));
            }
            if (!nu || nu->type != JSON_ARRAY || nu->c.count == 0)
                a_parts_push(&parts, &nparts, &capparts, a_dup(a_jstr(nt) ? a_jstr(nt) : ""));
            json_free(norm);
        } else if (it && strcmp(it, "text") == 0) {
            a_parts_push(&parts, &nparts, &capparts, a_pystr(json_obj_get(ic, "text"), ""));
        } else if (it && strcmp(it, "richlink") == 0) {
            a_parts_push(&parts, &nparts, &capparts,
                         w27_adapter_878564_p1_format_richlink_content(ic));
        } else if (it && it[0]) {
            a_buf m;
            a_buf_init(&m);
            a_buf_add(&m, "[Photon content type not handled: ");
            a_buf_add(&m, it);
            a_buf_add(&m, "]");
            a_parts_push(&parts, &nparts, &capparts, m.buf);
        } else if (!it) {
            const json_t *raw = json_obj_get(ic, "type");
            if (a_jtrue(raw)) {
                char *s = a_pystr(raw, "");
                a_buf m;
                a_buf_init(&m);
                a_buf_add(&m, "[Photon content type not handled: ");
                a_buf_add(&m, s ? s : "");
                a_buf_add(&m, "]");
                free(s);
                a_parts_push(&parts, &nparts, &capparts, m.buf);
            }
        }
    }
    json_free(contents);
    if (urls->c.count > 0 && strcmp(mtype, "text") == 0) {
        free(mtype);
        mtype = a_dup("document");
    }
    {
        a_buf joined;
        a_buf_init(&joined);
        for (i = 0; i < nparts; i++) {
            if (!parts[i] || !parts[i][0]) continue;
            if (joined.len) a_buf_addc(&joined, '\n');
            a_buf_add(&joined, parts[i]);
        }
        for (i = 0; i < nparts; i++) free(parts[i]);
        free(parts);
        text = a_strip(joined.buf ? joined.buf : "");
        free(joined.buf);
    }
    if (!text || !text[0]) {
        free(text);
        text = a_dup(urls->c.count > 0 ? "(attachment)" : "[Photon empty group received]");
    }
    {
        json_t *out = a_normalized(text, mtype, urls, types);
        free(mtype);
        return out;
    }
}

json_t *w27_adapter_878564_p3_normalize_group_content(const json_t *content,
                                                      const json_t *cache) {
    a_cache c;
    c.cache = cache;
    c.cursor = 0;
    return a_normalize_group(content, &c);
}

json_t *w27_adapter_878564_p3_normalize_content(const json_t *content, const json_t *cache) {
    const json_t *raw_type = json_obj_get(content, "type");
    const char *ctype = a_jstr(raw_type);
    a_cache c;
    if (!ctype) {
        a_buf m;
        char *txt;
        char *ts = a_jtrue(raw_type) ? a_pystr(raw_type, "") : a_dup("None");
        a_buf_init(&m);
        a_buf_add(&m, "[Photon content type not handled: ");
        a_buf_add(&m, ts ? ts : "None");
        a_buf_add(&m, "]");
        free(ts);
        txt = m.buf;
        return a_normalized(txt, "text", json_array(), json_array());
    }
    if (strcmp(ctype, "text") == 0)
        return a_normalized(a_pystr(json_obj_get(content, "text"), ""), "text",
                            json_array(), json_array());
    if (strcmp(ctype, "attachment") == 0 || strcmp(ctype, "voice") == 0) {
        const json_t *cell0 = (cache && cache->type == JSON_ARRAY && cache->c.count)
                                  ? cache->c.items[0] : NULL;
        return a_normalize_binary(content, a_jstr(cell0));
    }
    if (strcmp(ctype, "richlink") == 0)
        return a_normalized(w27_adapter_878564_p1_format_richlink_content(content), "text",
                            json_array(), json_array());
    if (strcmp(ctype, "group") == 0) {
        c.cache = cache;
        c.cursor = 0;
        return a_normalize_group(content, &c);
    }
    {
        a_buf m;
        char *txt;
        a_buf_init(&m);
        a_buf_add(&m, "[Photon content type not handled: ");
        a_buf_add(&m, ctype);
        a_buf_add(&m, "]");
        txt = m.buf;
        return a_normalized(txt, "text", json_array(), json_array());
    }
}

json_t *w27_adapter_878564_p3_attachment_body(const char *space_id, const char *safe_path,
                                              const char *kind, const char *name,
                                              const char *mime_type, const char *caption) {
    json_t *body = json_object();
    if (!body) return NULL;
    json_set(body, "spaceId", json_string(space_id ? space_id : ""));
    json_set(body, "path", json_string(safe_path ? safe_path : ""));
    json_set(body, "kind",
             json_string((kind && strcmp(kind, "voice") == 0) ? "voice" : "attachment"));
    if (name && name[0]) json_set(body, "name", json_string(name));
    if (mime_type && mime_type[0]) json_set(body, "mimeType", json_string(mime_type));
    if (caption && caption[0]) json_set(body, "caption", json_string(caption));
    return body;
}

char *w27_adapter_878564_p3_attachment_label(const char *kind, const char *name) {
    a_buf out;
    a_buf_init(&out);
    if (!out.buf) return NULL;
    a_buf_add(&out, "[");
    a_buf_add(&out, kind ? kind : "");
    a_buf_add(&out, ": ");
    a_buf_add(&out, name ? name : "");
    a_buf_add(&out, "]");
    return out.buf;
}

/* _sidecar_send's pure half: the ordered attempt plan (rich-link first, then the
 * plain/stripped text send), its truncation flag and the recorded sent text. */
json_t *w27_adapter_878564_p3_send_plan(const char *space_id, const char *text, int richlink,
                                        int markdown, int markdown_enabled) {
    const size_t max_len = 8000;
    char *rich_url = richlink ? w27_adapter_878564_p1_richlink_candidate(text, markdown_enabled != 0)
                              : NULL;
    json_t *out = json_object();
    json_t *attempts = json_array();
    bool send_markdown;
    char *payload;
    if (!out || !attempts) { json_free(out); json_free(attempts); free(rich_url); return NULL; }
    if (rich_url) {
        json_t *a = json_object();
        json_t *body = json_object();
        json_set(a, "path", json_string("/send-richlink"));
        json_set(body, "spaceId", json_string(space_id ? space_id : ""));
        json_set(body, "url", json_string(rich_url));
        json_set(a, "body", body);
        json_append(attempts, a);
        send_markdown = false; /* a rich-link failure disables markdown for the retry */
    } else {
        send_markdown = (markdown && markdown_enabled) ? true : false;
    }
    payload = w27_adapter_878564_p1_sidecar_payload_text(text, send_markdown);
    if (payload && a_utf8_len(payload) > max_len) {
        char *cut = a_utf8_slice(payload, max_len);
        free(payload);
        payload = cut;
    }
    {
        json_t *a = json_object();
        json_t *body = json_object();
        json_set(a, "path", json_string("/send"));
        json_set(body, "spaceId", json_string(space_id ? space_id : ""));
        json_set(body, "text", json_string(payload ? payload : ""));
        if (send_markdown) json_set(body, "format", json_string("markdown"));
        json_set(a, "body", body);
        json_append(attempts, a);
    }
    json_set(out, "sent_text", json_string(text ? text : ""));
    if (rich_url) json_set(out, "rich_url", json_string(rich_url));
    else json_set(out, "rich_url", json_null());
    json_set(out, "attempts", attempts);
    free(rich_url);
    free(payload);
    return out;
}

json_t *w27_adapter_878564_p3_send_poll_plan(const char *space_id, const char *title,
                                             const json_t *options) {
    json_t *opts = json_array();
    json_t *out = json_object();
    char *stripped_title = a_strip(title);
    size_t i;
    if (!opts || !out || !stripped_title) {
        json_free(opts); json_free(out); free(stripped_title); return NULL;
    }
    if (options && options->type == JSON_ARRAY) {
        for (i = 0; i < options->c.count; i++) {
            char *s = a_pystr(options->c.items[i], "");
            char *t = a_strip(s);
            free(s);
            if (t && t[0]) json_append(opts, json_string(t));
            free(t);
        }
    }
    if (!stripped_title[0]) {
        json_set(out, "ok", json_bool(false));
        json_set(out, "body", json_null());
        json_set(out, "error", json_string("poll title is required"));
        json_set(out, "sent_text", json_null());
        json_free(opts);
        free(stripped_title);
        return out;
    }
    if (opts->c.count < 2) {
        json_set(out, "ok", json_bool(false));
        json_set(out, "body", json_null());
        json_set(out, "error", json_string("poll needs at least two options"));
        json_set(out, "sent_text", json_null());
        json_free(opts);
        free(stripped_title);
        return out;
    }
    {
        char *t = a_utf8_slice(stripped_title, 8000);
        json_t *body = json_object();
        json_set(body, "spaceId", json_string(space_id ? space_id : ""));
        json_set(body, "title", json_string(t ? t : ""));
        json_set(body, "options", opts);
        json_set(out, "ok", json_bool(true));
        json_set(out, "body", body);
        json_set(out, "error", json_null());
        json_set(out, "sent_text", json_string(t ? t : ""));
        free(t);
    }
    free(stripped_title);
    return out;
}

/* ------------------------------------------------------------------------- */
/* p4 — ids, error typing, retry, constants, timestamps                        */
/* ------------------------------------------------------------------------- */

/* ^any;-;(\+\d{6,})$ */
static bool a_dm_guid_groups(const char *chat_id, size_t *start, size_t *len) {
    const char *prefix = "any;-;";
    size_t plen = strlen(prefix);
    size_t i;
    size_t d0;
    if (!chat_id) return false;
    if (strncmp(chat_id, prefix, plen) != 0) return false;
    i = plen;
    if (chat_id[i] != '+') return false;
    i++;
    d0 = i;
    while (chat_id[i] >= '0' && chat_id[i] <= '9') i++;
    if (i - d0 < 6) return false;
    if (chat_id[i] != '\0') return false;
    if (start) *start = plen;
    if (len) *len = i - plen;
    return true;
}

char *w27_adapter_878564_p4_normalize_chat_key(const char *chat_id) {
    size_t start = 0, len = 0;
    if (a_dm_guid_groups(chat_id, &start, &len)) return a_utf8_slice(chat_id + start, len);
    return a_dup(chat_id ? chat_id : "");
}

char *w27_adapter_878564_p4_sent_text_key(const char *chat_id) {
    if (!chat_id || !chat_id[0]) return NULL;
    return w27_adapter_878564_p4_normalize_chat_key(chat_id);
}

int w27_adapter_878564_p4_is_dm_guid(const char *text) {
    return a_dm_guid_groups(text, NULL, NULL) ? 1 : 0;
}

static const char *const k_target_not_allowed =
    "shared/free-tier Photon lines cannot initiate outbound sends to new "
    "targets — upgrade to a dedicated line or use another delivery channel";

json_t *w27_adapter_878564_p4_sidecar_error_from_response(const char *path, double status_code,
                                                          const char *text,
                                                          const json_t *data) {
    json_t *parsed = NULL;
    const json_t *src = data;
    char *error = NULL;
    char *error_class = NULL;
    bool retryable = false;
    json_t *out;
    if (!src || src->type != JSON_OBJECT) {
        if (text) {
            parsed = json_parse(text, NULL);
            if (parsed && parsed->type != JSON_OBJECT) { json_free(parsed); parsed = NULL; }
        }
        src = parsed;
    }
    error_class = a_pystr(json_obj_get(src, "error_class"), "sidecar_error");
    if (!error_class || !error_class[0]) {
        free(error_class);
        error_class = a_dup("sidecar_error");
    }
    retryable = a_jtrue(json_obj_get(src, "retryable"));
    if (a_jtrue(json_obj_get(src, "error"))) {
        error = a_pystr(json_obj_get(src, "error"), "");
    } else {
        char *head = a_utf8_slice(text ? text : "", 200);
        if (head && head[0]) error = head;
        else { free(head); error = a_dup("sidecar error"); }
    }
    if (strcmp(error_class, "target_not_allowed") == 0) {
        free(error);
        error = a_dup(k_target_not_allowed);
        retryable = false;
    }
    out = json_object();
    if (!out) { free(error); free(error_class); json_free(parsed); return NULL; }
    json_set(out, "path", json_string(path ? path : ""));
    json_set(out, "status_code", json_number(status_code));
    json_set(out, "error", json_string(error ? error : ""));
    json_set(out, "error_class", json_string(error_class));
    json_set(out, "retryable", json_bool(retryable));
    {
        a_buf m;
        a_buf_init(&m);
        a_buf_add(&m, "Photon sidecar ");
        a_buf_add(&m, path ? path : "");
        a_buf_add(&m, " returned ");
        {
            char num[32];
            if (status_code == (double)(long long)status_code)
                snprintf(num, sizeof(num), "%.0f", status_code);
            else snprintf(num, sizeof(num), "%.17g", status_code);
            a_buf_add(&m, num);
        }
        a_buf_add(&m, " (");
        a_buf_add(&m, error_class);
        a_buf_add(&m, ", retryable=");
        a_buf_add(&m, retryable ? "True" : "False");
        a_buf_add(&m, "): ");
        a_buf_add(&m, error ? error : "");
        json_set(out, "message", json_string(m.buf ? m.buf : ""));
        free(m.buf);
    }
    free(error);
    free(error_class);
    json_free(parsed);
    return out;
}

static const char *const k_photon_retryable[] = {
    "internal sidecar error", "upstream connect error", "upstream unavailable",
    "connection dropped", "reset reason: overflow", "upstream_overflow",
    "upstream_unavailable"
};

int w27_adapter_878564_p4_retryable_error(const char *error, int base_retryable) {
    char *low;
    size_t i;
    bool hit = false;
    if (base_retryable) return 1;
    low = a_lower(error);
    if (!low) return 0;
    if (!low[0] || strstr(low, "retryable=false") || strstr(low, "auth_or_config")) {
        free(low);
        return 0;
    }
    for (i = 0; i < sizeof(k_photon_retryable) / sizeof(k_photon_retryable[0]); i++) {
        if (strstr(low, k_photon_retryable[i])) { hit = true; break; }
    }
    free(low);
    return hit ? 1 : 0;
}

int w27_adapter_878564_p4_is_permanent_sidecar_failure(const json_t *result) {
    const json_t *raw = json_obj_get(result, "raw_response");
    const json_t *rt, *ec;
    if (!raw || raw->type != JSON_OBJECT) return 0;
    rt = json_obj_get(raw, "retryable");
    ec = json_obj_get(raw, "error_class");
    if (!rt || rt->type != JSON_BOOL || rt->bool_val) return 0;
    if (!ec || ec->type != JSON_STRING) return 0;
    return (strcmp(ec->str_val, "auth_or_config") == 0 ||
            strcmp(ec->str_val, "target_not_allowed") == 0) ? 1 : 0;
}

int w27_adapter_878564_p4_send_retry_is_final(const json_t *result) {
    return w27_adapter_878564_p4_is_permanent_sidecar_failure(result);
}

json_t *w27_adapter_878564_p4_constants(void) {
    json_t *o = json_object();
    json_t *pats = json_array();
    json_t *mentions = json_array();
    size_t i;
    if (!o || !pats || !mentions) {
        json_free(o); json_free(pats); json_free(mentions);
        return NULL;
    }
    json_set(o, "default_sidecar_port", json_number(8789));
    json_set(o, "default_sidecar_bind", json_string("127.0.0.1"));
    json_set(o, "max_message_length", json_number(8000));
    json_set(o, "dedup_max_size", json_number(4000));
    json_set(o, "dedup_window_seconds", json_number(48 * 3600));
    json_set(o, "fffc_wait_seconds", json_number(15.0));
    json_set(o, "npm_reinstall_timeout", json_number(600));
    json_set(o, "richlink_preview_suppress_seconds", json_number(30.0));
    json_set(o, "typing_cooldown_seconds", json_number(5.0));
    json_set(o, "runtime_record_name", json_string("photon-sidecar.json"));
    json_set(o, "richlink_preview_attachment_suffix", json_string(".pluginpayloadattachment"));
    json_set(o, "target_not_allowed_message", json_string(k_target_not_allowed));
    for (i = 0; i < sizeof(k_photon_retryable) / sizeof(k_photon_retryable[0]); i++)
        json_append(pats, json_string(k_photon_retryable[i]));
    json_set(o, "retryable_patterns", pats);
    json_append(mentions, json_string("(?<![\\w@])@?hermes\\s+agent\\b[,:\\-]?"));
    json_append(mentions, json_string("(?<![\\w@])@?hermes\\b[,:\\-]?"));
    json_set(o, "default_mention_patterns", mentions);
    return o;
}

/* --- datetime.fromisoformat subset ---------------------------------------- */

#define A_NAIVE_OFFSET INT_MIN

static bool a_parse_iso(const char *s, int *y, int *mo, int *d, int *h, int *mi, int *se,
                        long *us, int *offset_min) {
    size_t i;
    if (!s) return false;
    if (strlen(s) < 10) return false;
    for (i = 0; i < 10; i++) {
        if (i == 4 || i == 7) { if (s[i] != '-') return false; }
        else if (!isdigit((unsigned char)s[i])) return false;
    }
    *y = atoi(s);
    *mo = atoi(s + 5);
    *d = atoi(s + 8);
    if (*mo < 1 || *mo > 12 || *d < 1 || *d > 31) return false;
    *h = *mi = *se = 0;
    *us = 0;
    *offset_min = A_NAIVE_OFFSET;
    i = 10;
    if (s[i] == '\0') return true;
    if (s[i] != 'T' && s[i] != 't' && s[i] != ' ') return false;
    i++;
    if (!isdigit((unsigned char)s[i]) || !isdigit((unsigned char)s[i + 1]) ||
        s[i + 2] != ':' || !isdigit((unsigned char)s[i + 3]) ||
        !isdigit((unsigned char)s[i + 4])) return false;
    *h = atoi(s + i);
    *mi = atoi(s + i + 3);
    if (*h > 23 || *mi > 59) return false;
    i += 5;
    if (s[i] == ':') {
        size_t f0, digits;
        if (!isdigit((unsigned char)s[i + 1]) || !isdigit((unsigned char)s[i + 2])) return false;
        *se = atoi(s + i + 1);
        if (*se > 59) return false;
        i += 3;
        if (s[i] == '.' || s[i] == ',') {
            f0 = ++i;
            while (isdigit((unsigned char)s[i])) i++;
            digits = i - f0;
            if (digits == 0) return false;
            {
                char tmp[7];
                size_t k;
                for (k = 0; k < 6; k++) tmp[k] = (k < digits) ? s[f0 + k] : '0';
                tmp[6] = '\0';
                *us = strtol(tmp, NULL, 10);
            }
        }
    }
    if (s[i] == 'Z') {
        *offset_min = 0;
        i++;
    } else if (s[i] == '+' || s[i] == '-') {
        int sign = s[i] == '-' ? -1 : 1;
        int oh, om = 0;
        i++;
        if (!isdigit((unsigned char)s[i]) || !isdigit((unsigned char)s[i + 1])) return false;
        oh = atoi(s + i);
        i += 2;
        if (s[i] == ':') {
            if (!isdigit((unsigned char)s[i + 1]) || !isdigit((unsigned char)s[i + 2])) return false;
            om = atoi(s + i + 1);
            i += 3;
        }
        *offset_min = sign * (oh * 60 + om);
    }
    return s[i] == '\0';
}

static char *a_iso_format(int y, int mo, int d, int h, int mi, int se, long us, int off_min) {
    char buf[80];
    char *p = buf;
    p += sprintf(p, "%04d-%02d-%02dT%02d:%02d:%02d", y, mo, d, h, mi, se);
    if (us) p += sprintf(p, ".%06ld", us);
    if (off_min != A_NAIVE_OFFSET) {
        int sign = off_min < 0 ? -1 : 1;
        int a = off_min * sign;
        p += sprintf(p, "%c%02d:%02d", sign < 0 ? '-' : '+', a / 60, a % 60);
    }
    return a_dup(buf);
}

json_t *w27_adapter_878564_p4_parse_timestamp(const char *ts, const char *now_iso) {
    int y, mo, d, h, mi, se, off;
    long us;
    json_t *out = json_object();
    char *iso;
    bool used_now = false;
    if (!out) return NULL;
    if (ts && ts[0] && a_parse_iso(ts, &y, &mo, &d, &h, &mi, &se, &us, &off)) {
        iso = a_iso_format(y, mo, d, h, mi, se, us, off);
    } else {
        used_now = true;
        if (now_iso && now_iso[0] && a_parse_iso(now_iso, &y, &mo, &d, &h, &mi, &se, &us, &off))
            iso = a_iso_format(y, mo, d, h, mi, se, us, off);
        else iso = a_dup(now_iso ? now_iso : "");
    }
    json_set(out, "iso", json_string(iso ? iso : ""));
    json_set(out, "used_now", json_bool(used_now));
    free(iso);
    return out;
}

/* ------------------------------------------------------------------------- */
/* p5 — mention + presence gates                                               */
/* ------------------------------------------------------------------------- */

static const char *const k_default_mention_patterns[2] = {
    "(?<![\\w@])@?hermes\\s+agent\\b[,:\\-]?",
    "(?<![\\w@])@?hermes\\b[,:\\-]?"
};

/* compile_mention_patterns' normalization ladder, wakeword style (no platform
 * label): the pattern texts that reach re.compile, in order. */
json_t *w27_adapter_878564_p5_mention_pattern_texts(const json_t *raw) {
    json_t *texts = json_array();
    json_t *patterns = NULL;
    bool owned = false;
    size_t i;
    if (!texts) return NULL;
    if (!raw || raw->type == JSON_NULL) {
        json_append(texts, json_string(k_default_mention_patterns[0]));
        json_append(texts, json_string(k_default_mention_patterns[1]));
        return texts;
    }
    if (raw->type == JSON_STRING) {
        char *body = a_strip(raw->str_val);
        json_t *loaded = NULL;
        bool listed = false;
        if (body && body[0]) {
            loaded = json_parse(body, NULL);
            if (loaded && loaded->type == JSON_ARRAY) listed = true;
        }
        if (listed) {
            patterns = loaded;
            owned = true;
        } else {
            json_free(loaded);
            patterns = json_array();
            owned = true;
            {
                const char *p = body ? body : "";
                a_buf piece;
                a_buf_init(&piece);
                while (true) {
                    if (*p == '\0' || *p == '\n' || *p == ',') {
                        char *t = a_strip(piece.buf ? piece.buf : "");
                        if (t && t[0]) json_append(patterns, json_string(t));
                        free(t);
                        piece.len = 0;
                        if (piece.buf) piece.buf[0] = '\0';
                        if (*p == '\0') break;
                        p++;
                        continue;
                    }
                    a_buf_addc(&piece, *p);
                    p++;
                }
                free(piece.buf);
            }
        }
        free(body);
    } else if (raw->type == JSON_ARRAY) {
        patterns = (json_t *)raw;
    } else {
        patterns = json_array();
        owned = true;
        json_append(patterns, json_copy(raw));
    }
    for (i = 0; i < patterns->c.count; i++) {
        char *t = a_pystr(patterns->c.items[i], "");
        char *s = a_strip(t);
        if (s && s[0]) json_append(texts, json_string(s));
        free(t);
        free(s);
    }
    if (owned) json_free(patterns);
    return texts;
}

/* wake-word pattern tokenizer: literals, \b, \s+ , \s* , @? , [,:\-]? ,
 * (?<![\w@]) , (?<![\w]) */
typedef enum {
    MT_LIT, MT_BOUND, MT_WSPLUS, MT_WSSTAR, MT_ATOPT, MT_PUNCTOPT,
    MT_NEG_BEFORE_WORDAT, MT_NEG_BEFORE_WORD
} mt_kind;

typedef struct {
    mt_kind kind;
    char lit[128];
} mt_tok;

static size_t a_mt_compile(const char *pat, mt_tok *toks, size_t max) {
    size_t n = 0;
    const char *p = pat ? pat : "";
    while (*p && n < max) {
        if (strncmp(p, "(?<![\\w@])", 10) == 0) {
            toks[n].kind = MT_NEG_BEFORE_WORDAT; toks[n].lit[0] = 0; n++; p += 10; continue;
        }
        if (strncmp(p, "(?<![\\w])", 9) == 0) {
            toks[n].kind = MT_NEG_BEFORE_WORD; toks[n].lit[0] = 0; n++; p += 9; continue;
        }
        if (strncmp(p, "\\b", 2) == 0) {
            toks[n].kind = MT_BOUND; toks[n].lit[0] = 0; n++; p += 2; continue;
        }
        if (strncmp(p, "\\s+", 3) == 0) {
            toks[n].kind = MT_WSPLUS; toks[n].lit[0] = 0; n++; p += 3; continue;
        }
        if (strncmp(p, "\\s*", 3) == 0) {
            toks[n].kind = MT_WSSTAR; toks[n].lit[0] = 0; n++; p += 3; continue;
        }
        if (strncmp(p, "@?", 2) == 0) {
            toks[n].kind = MT_ATOPT; toks[n].lit[0] = 0; n++; p += 2; continue;
        }
        if (strncmp(p, "[,:\\-]?", 7) == 0) {
            toks[n].kind = MT_PUNCTOPT; toks[n].lit[0] = 0; n++; p += 7; continue;
        }
        if (strncmp(p, "(?i)", 4) == 0) { p += 4; continue; }
        {
            size_t k = 0;
            toks[n].kind = MT_LIT;
            while (p[k] && p[k] != '\\' && p[k] != '(' && p[k] != '@' && p[k] != '[' &&
                   p[k] != '*' && p[k] != '+' && p[k] != '?' && p[k] != '|' &&
                   p[k] != '^' && p[k] != '$' && p[k] != '{' && p[k] != '}' &&
                   k + 1 < sizeof(toks[n].lit)) {
                toks[n].lit[k] = p[k];
                k++;
            }
            toks[n].lit[k] = '\0';
            if (k == 0) { p++; continue; }
            n++;
            p += k;
        }
    }
    return n;
}

static int a_mt_at(const mt_tok *toks, size_t ntoks, const char *text, size_t pos) {
    size_t i = 0;
    size_t p = pos;
    size_t n = strlen(text);
    while (i < ntoks) {
        const mt_tok *t = &toks[i];
        switch (t->kind) {
        case MT_LIT: {
            size_t L = strlen(t->lit), k;
            if (p + L > n) return -1;
            for (k = 0; k < L; k++) {
                if (!a_ci_eq((unsigned char)text[p + k], (unsigned char)t->lit[k])) return -1;
            }
            p += L;
            break;
        }
        case MT_BOUND: {
            bool before = p > 0 && a_is_word((unsigned char)text[p - 1]);
            bool after = p < n && a_is_word((unsigned char)text[p]);
            if (before == after) return -1;
            break;
        }
        case MT_WSPLUS: {
            size_t s = p;
            while (p < n && a_isspace((unsigned char)text[p])) p++;
            if (p == s) return -1;
            break;
        }
        case MT_WSSTAR:
            while (p < n && a_isspace((unsigned char)text[p])) p++;
            break;
        case MT_ATOPT:
            if (p < n && text[p] == '@') p++;
            break;
        case MT_PUNCTOPT:
            if (p < n && (text[p] == ',' || text[p] == ':' || text[p] == '-')) p++;
            break;
        case MT_NEG_BEFORE_WORDAT:
            if (p > 0) {
                unsigned char c = (unsigned char)text[p - 1];
                if (a_is_word(c) || c == '@') return -1;
            }
            break;
        case MT_NEG_BEFORE_WORD:
            if (p > 0 && a_is_word((unsigned char)text[p - 1])) return -1;
            break;
        }
        i++;
    }
    return (int)p;
}

static json_t *a_patterns_or_defaults(const json_t *patterns) {
    json_t *out;
    size_t i;
    if (patterns) return (json_t *)patterns;
    out = json_array();
    if (!out) return NULL;
    for (i = 0; i < 2; i++) json_append(out, json_string(k_default_mention_patterns[i]));
    return out;
}

int w27_adapter_878564_p5_mention_match(const char *text, const json_t *patterns) {
    json_t *pats = a_patterns_or_defaults(patterns);
    size_t i, pos, n;
    int hit = 0;
    if (!pats) return 0;
    if (!text || !text[0]) { if (pats != patterns) json_free(pats); return 0; }
    n = strlen(text);
    for (i = 0; i < pats->c.count && !hit; i++) {
        const char *pat = a_jstr(pats->c.items[i]);
        mt_tok toks[32];
        size_t ntoks = a_mt_compile(pat, toks, 32);
        if (!ntoks) continue;
        for (pos = 0; pos <= n; pos++) {
            if (a_mt_at(toks, ntoks, text, pos) >= 0) { hit = 1; break; }
        }
    }
    if (pats != patterns) json_free(pats);
    return hit;
}

char *w27_adapter_878564_p5_clean_mention_text(const char *text, const json_t *patterns) {
    json_t *pats = a_patterns_or_defaults(patterns);
    char *stripped = (text && text[0]) ? a_lstrip_ws(text) : a_dup("");
    size_t i;
    char *res = NULL;
    if (!pats || !stripped) {
        free(stripped);
        if (pats && pats != patterns) json_free(pats);
        return a_dup(text ? text : "");
    }
    for (i = 0; i < pats->c.count; i++) {
        const char *pat = a_jstr(pats->c.items[i]);
        mt_tok toks[32];
        size_t ntoks = a_mt_compile(pat, toks, 32);
        int end;
        if (!ntoks) continue;
        end = a_mt_at(toks, ntoks, stripped, 0);
        if (end >= 0) {
            char *rest = a_lstrip_chars(stripped + end, " ,:-");
            if (rest && rest[0]) res = rest;
            else { free(rest); res = a_dup(text ? text : ""); }
            break;
        }
    }
    if (!res) res = a_dup(text ? text : "");
    free(stripped);
    if (pats != patterns) json_free(pats);
    return res;
}

json_t *w27_adapter_878564_p5_gate_stt_chat(const char *space_id, const char *space_type,
                                            const json_t *content, int require_mention,
                                            int voice_gate_enabled) {
    json_t *out = json_object();
    bool transcribes;
    if (!out) return NULL;
    transcribes = (space_type && strcmp(space_type, "group") == 0) && require_mention &&
                  w27_adapter_878564_p2_carries_voice(content) && voice_gate_enabled;
    json_set(out, "chat", json_string(space_id ? space_id : ""));
    json_set(out, "transcribes", json_bool(transcribes));
    return out;
}

static bool a_flag_in(const char *v, const char *const *set, size_t nset) {
    size_t i;
    if (!v) return false;
    for (i = 0; i < nset; i++) if (strcmp(v, set[i]) == 0) return true;
    return false;
}

int w27_adapter_878564_p5_require_mention_flag(const json_t *value, const json_t *env) {
    static const char *const yes[] = {"true", "1", "yes", "on"};
    const json_t *src = (value && value->type != JSON_NULL) ? value : env;
    char *s, *low, *trim;
    bool hit;
    if (!src || src->type == JSON_NULL) return 0;
    s = a_pystr(src, "");
    low = a_lower(s);
    trim = a_strip(low);
    hit = a_flag_in(trim, yes, sizeof(yes) / sizeof(yes[0]));
    free(s);
    free(low);
    free(trim);
    return hit ? 1 : 0;
}

int w27_adapter_878564_p5_markdown_enabled(const json_t *value) {
    static const char *const off[] = {"false", "0", "no"};
    char *s = (value && value->type != JSON_NULL) ? a_pystr(value, "true") : a_dup("true");
    char *low = a_lower(s);
    char *trim = a_strip(low);
    bool hit = a_flag_in(trim, off, sizeof(off) / sizeof(off[0]));
    free(s);
    free(low);
    free(trim);
    return hit ? 0 : 1;
}

int w27_adapter_878564_p5_reactions_enabled(const json_t *value) {
    static const char *const yes[] = {"true", "1", "yes", "on"};
    char *s = (value && value->type != JSON_NULL) ? a_pystr(value, "false") : a_dup("false");
    char *low = a_lower(s);
    char *trim = a_strip(low);
    bool hit = a_flag_in(trim, yes, sizeof(yes) / sizeof(yes[0]));
    free(s);
    free(low);
    free(trim);
    return hit ? 1 : 0;
}

int w27_adapter_878564_p5_autostart_enabled(const json_t *value) {
    static const char *const off[] = {"0", "false", "no"};
    char *s = (value && value->type != JSON_NULL) ? a_pystr(value, "true") : a_dup("true");
    char *low = a_lower(s); /* no .strip(): " false " stays truthy */
    bool hit = a_flag_in(low, off, sizeof(off) / sizeof(off[0]));
    free(s);
    free(low);
    return hit ? 0 : 1;
}

json_t *w27_adapter_878564_p5_is_recent_richlink_preview(const char *chat_id,
                                                         const json_t *content,
                                                         const json_t *last_ts, double now) {
    json_t *out = json_object();
    bool hit = false, popped = false;
    if (!out) return NULL;
    if (chat_id && chat_id[0] && w27_adapter_878564_p2_is_richlink_preview_content(content) &&
        last_ts && last_ts->type == JSON_NUMBER) {
        if (now - last_ts->num_val > 30.0) popped = true;
        else hit = true;
    }
    json_set(out, "hit", json_bool(hit));
    json_set(out, "popped", json_bool(popped));
    return out;
}

json_t *w27_adapter_878564_p5_record_recent_richlink(const char *chat_id, const char *text) {
    json_t *out = json_object();
    char *cand = w27_adapter_878564_p1_url_only_candidate(text);
    bool recorded = false;
    char *key = NULL;
    if (!out) { free(cand); return NULL; }
    if (chat_id && chat_id[0] && cand) {
        key = w27_adapter_878564_p4_normalize_chat_key(chat_id);
        recorded = true;
    }
    json_set(out, "recorded", json_bool(recorded));
    json_set(out, "key", key ? json_string(key) : json_null());
    free(cand);
    free(key);
    return out;
}
