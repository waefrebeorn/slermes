/*
 * adapter_html.c -- real C11 port of the Matrix outbound HTML layer of
 * upstream Hermes plugins/platforms/matrix/adapter_html.py.
 *
 * PoP: w20_adapterhtm_fd926b_latex_to_tokens @ plugins/platforms/matrix/adapter_html.py:_latex_to_tokens
 * PoP: w20_adapterhtm_fd926b_tokens_to_mx_maths @ plugins/platforms/matrix/adapter_html.py:_tokens_to_mx_maths
 * PoP: w20_adapterhtm_fd926b_tokens_to_tex_source @ plugins/platforms/matrix/adapter_html.py:_tokens_to_tex_source
 * PoP: w20_adapterhtm_fd926b_sanitize @ plugins/platforms/matrix/adapter_html.py:_sanitize_matrix_html
 * PoP: w20_adapterhtm_fd926b_pre_sanitize_markdown @ plugins/platforms/matrix/adapter_html.py:_pre_sanitize_matrix_markdown
 * PoP: w20_adapterhtm_fd926b_safe_url @ plugins/platforms/matrix/adapter_html.py:_MatrixHtmlSanitizer._safe_url
 *
 * The sanitizer is an allowlist tokenizer with the same observable behaviour as
 * the HTMLParser subclass: allowed tags survive with their safe attributes,
 * script/style bodies vanish, text is escaped and TeX sentinels expand to
 * data-mx-maths in text nodes only (in an attribute they revert to $tex$).
 */
#include "noc_w20_adapterhtm_fd926b.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- growable string ------------------------------------------------------ */

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} ada_sb;

static void ada_sb_init(ada_sb *b) {
    b->buf = NULL;
    b->len = 0;
    b->cap = 0;
}

static void ada_sb_reserve(ada_sb *b, size_t extra) {
    size_t need = b->len + extra + 1;
    if (need <= b->cap) return;
    {
        size_t cap = b->cap ? b->cap : 64;
        char *grown;
        while (cap < need) cap *= 2;
        grown = (char *)realloc(b->buf, cap);
        if (!grown) return;
        b->buf = grown;
        b->cap = cap;
    }
}

static void ada_sb_addn(ada_sb *b, const char *s, size_t n) {
    ada_sb_reserve(b, n);
    if (!b->buf) return;
    memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = '\0';
}

static void ada_sb_add(ada_sb *b, const char *s) { ada_sb_addn(b, s, strlen(s ? s : "")); }

static void ada_sb_putc(ada_sb *b, char c) { ada_sb_addn(b, &c, 1); }

static char *ada_sb_take(ada_sb *b) {
    char *out = b->buf;
    if (!out) {
        out = (char *)malloc(1);
        if (out) out[0] = '\0';
    }
    b->buf = NULL;
    b->len = b->cap = 0;
    return out;
}

static char *ada_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static int ada_isspace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

static int ada_isword(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

static char ada_lower(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }

static int ada_ci_prefix(const char *s, size_t n, const char *pat) {
    size_t m = strlen(pat), i;
    if (n < m) return 0;
    for (i = 0; i < m; i++) {
        if (ada_lower(s[i]) != ada_lower(pat[i])) return 0;
    }
    return 1;
}

/* str.strip() over the ASCII whitespace set, into a fresh string. */
static char *ada_strip_range(const char *s, size_t n) {
    size_t start = 0, end = n;
    char *out;
    while (start < end && ada_isspace(s[start])) start++;
    while (end > start && ada_isspace(s[end - 1])) end--;
    out = (char *)malloc(end - start + 1);
    if (!out) return NULL;
    memcpy(out, s + start, end - start);
    out[end - start] = '\0';
    return out;
}

/* --- TeX sentinel tokens -------------------------------------------------- */

#define ADA_TOKEN_HEAD "HERMESTEX"
#define ADA_TOKEN_TAIL "HERMESTEXEND"

/* Match a sentinel token at s[i]; returns 1 and fills idx/length. */
static int ada_token_at(const char *s, size_t i, size_t n, size_t *idx, size_t *length) {
    size_t p = i, digits = 0;
    long long value = 0;
    if (n - p < strlen(ADA_TOKEN_HEAD)) return 0;
    if (strncmp(s + p, ADA_TOKEN_HEAD, strlen(ADA_TOKEN_HEAD))) return 0;
    p += strlen(ADA_TOKEN_HEAD);
    if (n - p >= 7 && !strncmp(s + p, "DISPLAY", 7)) {
        p += 7;
    } else if (n - p >= 6 && !strncmp(s + p, "INLINE", 6)) {
        p += 6;
    } else {
        return 0;
    }
    while (p < n && s[p] >= '0' && s[p] <= '9') {
        if (value < 100000000LL) value = value * 10 + (s[p] - '0');
        p++;
        digits++;
    }
    if (!digits) return 0;
    if (n - p < strlen(ADA_TOKEN_TAIL)) return 0;
    if (strncmp(s + p, ADA_TOKEN_TAIL, strlen(ADA_TOKEN_TAIL))) return 0;
    p += strlen(ADA_TOKEN_TAIL);
    *idx = (size_t)value;
    *length = p - i;
    return 1;
}

static int ada_token_tag_is_display(const char *s, size_t i) {
    size_t p = i + strlen(ADA_TOKEN_HEAD);
    return !strncmp(s + p, "DISPLAY", 7);
}

/* --- html.escape(quote=True) ---------------------------------------------- */

static void ada_html_escape_n(ada_sb *out, const char *s, size_t n, int quote) {
    const unsigned char *p = (const unsigned char *)(s ? s : "");
    const unsigned char *end = p + n;
    for (; p < end; p++) {
        switch (*p) {
        case '&': ada_sb_add(out, "&amp;"); break;
        case '<': ada_sb_add(out, "&lt;"); break;
        case '>': ada_sb_add(out, "&gt;"); break;
        case '"': if (quote) ada_sb_add(out, "&quot;"); else ada_sb_putc(out, '"'); break;
        case '\'': if (quote) ada_sb_add(out, "&#x27;"); else ada_sb_putc(out, '\''); break;
        default: ada_sb_putc(out, (char)*p); break;
        }
    }
}

static void ada_html_escape(ada_sb *out, const char *s, int quote) {
    ada_html_escape_n(out, s, strlen(s ? s : ""), quote);
}

/* --- _latex_to_tokens ----------------------------------------------------- */

json_t *w20_adapterhtm_fd926b_latex_to_tokens(const char *text) {
    const char *s = text ? text : "";
    size_t n = strlen(s), i = 0;
    ada_sb first, second;
    json_t *store = json_array();
    json_t *out = json_object();

    ada_sb_init(&first);
    ada_sb_init(&second);

    /* Pass 1: $$...$$ -> display token. */
    while (i < n) {
        if (s[i] == '$' && i + 1 < n && s[i + 1] == '$') {
            size_t j = i + 2;
            while (j < n && s[j] != '\n' && s[j] != '$') j++;
            if (j > i + 2 && j + 1 < n && s[j] == '$' && s[j + 1] == '$') {
                char *tex = ada_strip_range(s + i + 2, j - (i + 2));
                char token[64];
                json_t *entry = json_object();
                json_set(entry, "tag", json_string("div"));
                json_set(entry, "tex", json_string(tex ? tex : ""));
                json_append(store, entry);
                snprintf(token, sizeof(token), "HERMESTEXDISPLAY%uHERMESTEXEND",
                         (unsigned)(store->c.count - 1));
                ada_sb_add(&first, token);
                free(tex);
                i = j + 2;
                continue;
            }
        }
        ada_sb_putc(&first, s[i++]);
    }

    /* Pass 2: $...$ -> inline token, over pass 1's output. */
    {
        const char *t = first.buf ? first.buf : "";
        size_t m = first.len;
        i = 0;
        while (i < m) {
            int lookbehind_ok = (i == 0) ||
                                !(t[i - 1] == '\\' || t[i - 1] == '$' || ada_isword(t[i - 1]));
            if (t[i] == '$' && lookbehind_ok) {
                size_t j = i + 1;
                while (j < m && t[j] != '\n' && t[j] != '$') j++;
                if (j > i + 1 && j < m && t[j] == '$' && (j + 1 >= m || !ada_isword(t[j + 1]))) {
                    char *tex = ada_strip_range(t + i + 1, j - (i + 1));
                    char token[64];
                    json_t *entry = json_object();
                    json_set(entry, "tag", json_string("span"));
                    json_set(entry, "tex", json_string(tex ? tex : ""));
                    json_append(store, entry);
                    snprintf(token, sizeof(token), "HERMESTEXINLINE%uHERMESTEXEND",
                             (unsigned)(store->c.count - 1));
                    ada_sb_add(&second, token);
                    free(tex);
                    i = j + 1;
                    continue;
                }
            }
            ada_sb_putc(&second, t[i++]);
        }
    }

    json_set(out, "text", json_string(second.buf ? second.buf : ""));
    json_set(out, "store", store);
    free(first.buf);
    free(second.buf);
    return out;
}

/* --- store lookup --------------------------------------------------------- */

static const json_t *ada_store_at(const json_t *store, size_t idx) {
    if (!store || store->type != JSON_ARRAY) return NULL;
    if (idx >= store->c.count) return NULL;
    return store->c.items[idx];
}

char *w20_adapterhtm_fd926b_tokens_to_mx_maths(const char *html, const json_t *store) {
    const char *s = html ? html : "";
    size_t n = strlen(s), i = 0;
    ada_sb out;
    ada_sb_init(&out);
    while (i < n) {
        size_t idx = 0, length = 0;
        if (ada_token_at(s, i, n, &idx, &length)) {
            const json_t *entry = ada_store_at(store, idx);
            if (!entry) {
                ada_sb_addn(&out, s + i, length);   /* not one of ours: verbatim */
            } else {
                const char *tag = json_get_str(entry, "tag", "span");
                const char *tex = json_get_str(entry, "tex", "");
                ada_sb_add(&out, "<");
                ada_sb_add(&out, tag);
                ada_sb_add(&out, " data-mx-maths=\"");
                ada_html_escape(&out, tex, 1);
                ada_sb_add(&out, "\">");
                ada_html_escape(&out, tex, 1);
                ada_sb_add(&out, "</");
                ada_sb_add(&out, tag);
                ada_sb_add(&out, ">");
            }
            i += length;
            continue;
        }
        ada_sb_putc(&out, s[i++]);
    }
    return ada_sb_take(&out);
}

char *w20_adapterhtm_fd926b_tokens_to_tex_source(const char *text, const json_t *store) {
    const char *s = text ? text : "";
    size_t n = strlen(s), i = 0;
    ada_sb out;
    ada_sb_init(&out);
    while (i < n) {
        size_t idx = 0, length = 0;
        if (ada_token_at(s, i, n, &idx, &length)) {
            const json_t *entry = ada_store_at(store, idx);
            if (!entry) {
                ada_sb_addn(&out, s + i, length);
            } else {
                int display = ada_token_tag_is_display(s, i);
                ada_sb_add(&out, display ? "$$" : "$");
                ada_sb_add(&out, json_get_str(entry, "tex", ""));
                ada_sb_add(&out, display ? "$$" : "$");
            }
            i += length;
            continue;
        }
        ada_sb_putc(&out, s[i++]);
    }
    return ada_sb_take(&out);
}

/* --- _safe_url ------------------------------------------------------------ */

static int ada_scheme_allowed(const char *scheme) {
    static const char *const ALLOWED[] = {"http", "https", "matrix", "mailto", NULL};
    int i;
    for (i = 0; ALLOWED[i]; i++) {
        size_t k = strlen(ALLOWED[i]);
        size_t j;
        int same = 1;
        for (j = 0; j < k; j++) {
            if (!scheme[j] || ada_lower(scheme[j]) != ALLOWED[i][j]) { same = 0; break; }
        }
        if (same && !scheme[k]) return 1;
    }
    return 0;
}

char *w20_adapterhtm_fd926b_safe_url(const char *value) {
    ada_sb stripped;
    char *cleaned;
    size_t n, i, colon = 0;
    int has_scheme = 0;
    ada_sb_init(&stripped);
    {
        const unsigned char *p = (const unsigned char *)(value ? value : "");
        for (; *p; p++) {
            if (*p < 0x20 || *p == 0x7f) continue;   /* [\x00-\x1f\x7f]+ */
            ada_sb_putc(&stripped, (char)*p);
        }
    }
    cleaned = ada_strip_range(stripped.buf ? stripped.buf : "", stripped.len);
    free(stripped.buf);
    if (!cleaned) return NULL;
    n = strlen(cleaned);
    if (n && isalpha((unsigned char)cleaned[0])) {
        for (i = 1; i < n; i++) {
            char c = cleaned[i];
            if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                c == '+' || c == '.' || c == '-') {
                continue;
            }
            if (c == ':') { has_scheme = 1; colon = i; }
            break;
        }
    }
    if (has_scheme) {
        char *scheme = (char *)malloc(colon + 1);
        if (!scheme) { free(cleaned); return NULL; }
        memcpy(scheme, cleaned, colon);
        scheme[colon] = '\0';
        if (!ada_scheme_allowed(scheme)) {
            free(scheme);
            free(cleaned);
            return ada_dup("");
        }
        free(scheme);
    }
    return cleaned;
}

/* --- allowlist tables ----------------------------------------------------- */

static const char *const ADA_ALLOWED_TAGS[] = {
    "a", "b", "blockquote", "br", "code", "del", "em", "h1", "h2", "h3", "h4", "h5", "h6",
    "hr", "i", "li", "ol", "p", "pre", "s", "strike", "strong", "table", "tbody", "td", "th",
    "thead", "tr", "ul", NULL
};

static int ada_tag_allowed(const char *tag) {
    int i;
    for (i = 0; ADA_ALLOWED_TAGS[i]; i++) {
        if (!strcmp(tag, ADA_ALLOWED_TAGS[i])) return 1;
    }
    return 0;
}

static int ada_tag_is_void(const char *tag) {
    return !strcmp(tag, "br") || !strcmp(tag, "hr");
}

/* --- attribute value unescape (html.unescape for the refs we meet) -------- */

static void ada_unescape_into(ada_sb *out, const char *s, size_t n) {
    size_t i = 0;
    while (i < n) {
        if (s[i] == '&') {
            if (i + 2 < n && s[i + 1] == '#') {
                size_t j = i + 2, k;
                long value = 0;
                int hex = 0;
                if (s[j] == 'x' || s[j] == 'X') { hex = 1; j++; }
                k = j;
                while (k < n && (hex ? isxdigit((unsigned char)s[k]) : isdigit((unsigned char)s[k]))) {
                    value = value * (hex ? 16 : 10) + (hex
                        ? (isdigit((unsigned char)s[k]) ? s[k] - '0' : ada_lower(s[k]) - 'a' + 10)
                        : s[k] - '0');
                    if (value > 0x10FFFF) value = 0x10FFFF;
                    k++;
                }
                if (k > j && k < n && s[k] == ';') {
                    if (value < 0x80) ada_sb_putc(out, (char)value);
                    else ada_sb_add(out, "?");
                    i = k + 1;
                    continue;
                }
            }
            {
                size_t j = i + 1;
                char name[16];
                size_t at = 0;
                while (j < n && (isalnum((unsigned char)s[j]) || s[j] == '.' || s[j] == '-') &&
                       at + 1 < sizeof(name)) {
                    name[at++] = s[j++];
                }
                name[at] = '\0';
                if (at && j < n && s[j] == ';') {
                    if (!strcmp(name, "amp")) ada_sb_putc(out, '&');
                    else if (!strcmp(name, "lt")) ada_sb_putc(out, '<');
                    else if (!strcmp(name, "gt")) ada_sb_putc(out, '>');
                    else if (!strcmp(name, "quot")) ada_sb_putc(out, '"');
                    else if (!strcmp(name, "apos")) ada_sb_putc(out, '\'');
                    else {
                        ada_sb_putc(out, '&');
                        ada_sb_addn(out, name, at);
                        ada_sb_putc(out, ';');
                    }
                    i = j + 1;
                    continue;
                }
            }
        }
        ada_sb_putc(out, s[i++]);
    }
}

/* --- sanitizer ------------------------------------------------------------ */

typedef struct {
    const json_t *store;
    int skip_depth;
    const char *cdata;   /* raw-text element whose body is data, not markup */
    ada_sb out;
} ada_ctx;

static void ada_data_text(ada_ctx *ctx, const char *s, size_t n) {
    ada_sb escaped;
    char *expanded;
    if (ctx->skip_depth) return;
    ada_sb_init(&escaped);
    ada_html_escape_n(&escaped, s, n, 1);
    /* Only the escaped run's own bytes matter; escaping cannot create a token. */
    expanded = w20_adapterhtm_fd926b_tokens_to_mx_maths(escaped.buf ? escaped.buf : "", ctx->store);
    if (expanded) ada_sb_add(&ctx->out, expanded);
    free(expanded);
    free(escaped.buf);
}

/* Emit one data run, dispatching &entity; / &#char; verbatim like HTMLParser. */
static void ada_emit_data(ada_ctx *ctx, const char *s, size_t n) {
    size_t i = 0, start = 0;
    while (i < n) {
        if (s[i] != '&') { i++; continue; }
        {
            size_t end = 0;
            if (i + 2 < n && s[i + 1] == '#') {
                size_t j = i + 2, k;
                int hex = 0;
                if (s[j] == 'x' || s[j] == 'X') { hex = 1; j++; }
                k = j;
                while (k < n && (hex ? isxdigit((unsigned char)s[k]) : isdigit((unsigned char)s[k]))) k++;
                if (k > j && k < n && s[k] == ';') end = k + 1;
            } else if (i + 1 < n && isalpha((unsigned char)s[i + 1])) {
                size_t j = i + 1;
                while (j < n && (isalnum((unsigned char)s[j]) || s[j] == '.' || s[j] == '-')) j++;
                if (j < n && s[j] == ';') end = j + 1;
            }
            if (end) {
                ada_data_text(ctx, s + start, i - start);
                if (!ctx->skip_depth) ada_sb_addn(&ctx->out, s + i, end - i);
                i = end;
                start = end;
                continue;
            }
        }
        i++;
    }
    if (start < n) ada_data_text(ctx, s + start, n - start);
}

/* _safe_attrs(tag, attrs) */
static void ada_safe_attrs(ada_sb *out, const char *tag, const char *attrs, size_t attrs_len,
                           const json_t *store) {
    size_t i = 0;
    while (i < attrs_len) {
        size_t name_start, name_end, value_start = 0, value_end = 0;
        int has_value = 0;
        char name[64], *raw_value;

        while (i < attrs_len && (ada_isspace(attrs[i]) || attrs[i] == '/')) i++;
        if (i >= attrs_len) break;
        name_start = i;
        while (i < attrs_len && !ada_isspace(attrs[i]) && attrs[i] != '=' && attrs[i] != '>' &&
               attrs[i] != '/') {
            i++;
        }
        name_end = i;
        while (i < attrs_len && ada_isspace(attrs[i])) i++;
        if (i < attrs_len && attrs[i] == '=') {
            i++;
            while (i < attrs_len && ada_isspace(attrs[i])) i++;
            if (i < attrs_len && (attrs[i] == '"' || attrs[i] == '\'')) {
                char quote = attrs[i++];
                value_start = i;
                while (i < attrs_len && attrs[i] != quote) i++;
                value_end = i;
                if (i < attrs_len) i++;
            } else {
                value_start = i;
                while (i < attrs_len && !ada_isspace(attrs[i]) && attrs[i] != '>') i++;
                value_end = i;
            }
            has_value = 1;
        }
        {
            size_t at = 0, k;
            for (k = name_start; k < name_end && at + 1 < sizeof(name); k++) {
                name[at++] = ada_lower(attrs[k]);
            }
            name[at] = '\0';
        }
        if (!name[0]) continue;
        {
            ada_sb unescaped;
            ada_sb_init(&unescaped);
            if (has_value) ada_unescape_into(&unescaped, attrs + value_start, value_end - value_start);
            raw_value = w20_adapterhtm_fd926b_tokens_to_tex_source(
                unescaped.buf ? unescaped.buf : "", store);
            free(unescaped.buf);
        }
        if (!raw_value) continue;

        if (!strcmp(tag, "a") && !strcmp(name, "href")) {
            char *href = w20_adapterhtm_fd926b_safe_url(raw_value);
            if (href && href[0]) {
                ada_sb_add(out, " href=\"");
                ada_html_escape(out, href, 1);
                ada_sb_add(out, "\"");
            }
            free(href);
        } else if (!strcmp(tag, "code") && !strcmp(name, "class")) {
            /* re.fullmatch(r"language-[A-Za-z0-9_+.-]{1,64}", value) */
            const char *p = raw_value;
            int ok = !strncmp(p, "language-", 9);
            size_t k = 9, body = 0;
            if (ok) {
                for (; p[k]; k++) {
                    char c = p[k];
                    if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                          (c >= '0' && c <= '9') || c == '_' || c == '+' || c == '.' || c == '-')) {
                        ok = 0;
                        break;
                    }
                    body++;
                }
            }
            if (ok && body >= 1 && body <= 64) {
                ada_sb_add(out, " class=\"");
                ada_html_escape(out, raw_value, 1);
                ada_sb_add(out, "\"");
            }
        }
        free(raw_value);
    }
}

static void ada_start_tag(ada_ctx *ctx, const char *tag, const char *attrs, size_t attrs_len,
                          int self_closing) {
    if (!strcmp(tag, "script") || !strcmp(tag, "style")) {
        ctx->skip_depth++;
        ctx->cdata = tag;     /* HTMLParser's CDATA_CONTENT_ELEMENTS */
        return;
    }
    if (ctx->skip_depth || !ada_tag_allowed(tag)) return;
    ada_sb_add(&ctx->out, "<");
    ada_sb_add(&ctx->out, tag);
    if (ada_tag_is_void(tag)) {
        ada_sb_add(&ctx->out, ">");
    } else {
        ada_safe_attrs(&ctx->out, tag, attrs, attrs_len, ctx->store);
        ada_sb_add(&ctx->out, ">");
    }
    if (self_closing) {
        /* HTMLParser.handle_startendtag: start then end. */
        if (!ctx->skip_depth && ada_tag_allowed(tag) && !ada_tag_is_void(tag)) {
            ada_sb_add(&ctx->out, "</");
            ada_sb_add(&ctx->out, tag);
            ada_sb_add(&ctx->out, ">");
        }
    }
}

static void ada_end_tag(ada_ctx *ctx, const char *tag) {
    if ((!strcmp(tag, "script") || !strcmp(tag, "style")) && ctx->skip_depth) {
        ctx->skip_depth--;
        if (!ctx->skip_depth) ctx->cdata = NULL;
        return;
    }
    if (ctx->skip_depth || !ada_tag_allowed(tag) || ada_tag_is_void(tag)) return;
    ada_sb_add(&ctx->out, "</");
    ada_sb_add(&ctx->out, tag);
    ada_sb_add(&ctx->out, ">");
}

char *w20_adapterhtm_fd926b_sanitize(const char *html, const json_t *store) {
    const char *s = html ? html : "";
    size_t n = strlen(s), i = 0;
    ada_ctx ctx;
    ctx.store = store;
    ctx.skip_depth = 0;
    ctx.cdata = NULL;
    ada_sb_init(&ctx.out);

    while (i < n) {
        if (ctx.skip_depth && ctx.cdata) {
            /* Raw text: everything up to the matching end tag is data. */
            size_t m = i, found = 0, after = 0;
            size_t taglen = strlen(ctx.cdata);
            while (m + 2 < n) {
                if (s[m] == '<' && s[m + 1] == '/') {
                    size_t q = m + 2;
                    while (q < n && ada_isspace(s[q])) q++;
                    if (ada_ci_prefix(s + q, n - q, ctx.cdata)) {
                        size_t r = q + taglen;
                        while (r < n && ada_isspace(s[r])) r++;
                        if (r < n && s[r] == '>') {
                            found = 1;
                            after = r + 1;
                            break;
                        }
                    }
                }
                m++;
            }
            if (!found) { i = n; break; }
            {
                char raw_tag[16];
                snprintf(raw_tag, sizeof(raw_tag), "%s", ctx.cdata);
                ada_end_tag(&ctx, raw_tag);
            }
            i = after;
            continue;
        }
        if (s[i] != '<') {
            size_t start = i;
            while (i < n && s[i] != '<') i++;
            ada_emit_data(&ctx, s + start, i - start);
            continue;
        }
        if (n - i >= 4 && !strncmp(s + i, "<!--", 4)) {
            size_t j = i + 4;
            while (j + 2 < n && !(s[j] == '-' && s[j + 1] == '-' && s[j + 2] == '>')) j++;
            i = (j + 2 < n) ? j + 3 : n;
            continue;
        }
        if (i + 1 < n && (s[i + 1] == '!' || s[i + 1] == '?')) {
            size_t j = i + 2;
            while (j < n && s[j] != '>') j++;
            i = (j < n) ? j + 1 : n;
            continue;
        }
        if (i + 1 < n && s[i + 1] == '/') {
            size_t j = i + 2, name_end;
            char name[64];
            size_t at = 0, k;
            while (j < n && ada_isspace(s[j])) j++;
            name_end = j;
            if (name_end < n && isalpha((unsigned char)s[name_end])) {
                while (name_end < n && !ada_isspace(s[name_end]) && s[name_end] != '/' &&
                       s[name_end] != '>') {
                    name_end++;
                }
                for (k = j; k < name_end && at + 1 < sizeof(name); k++) name[at++] = ada_lower(s[k]);
                name[at] = '\0';
                while (name_end < n && s[name_end] != '>') name_end++;
                if (name_end < n) {
                    ada_end_tag(&ctx, name);
                    i = name_end + 1;
                    continue;
                }
            }
            ada_emit_data(&ctx, "<", 1);
            i++;
            continue;
        }
        if (i + 1 < n && isalpha((unsigned char)s[i + 1])) {
            size_t j = i + 1, name_end, attr_start, k;
            char name[64];
            size_t at = 0;
            int self_closing = 0, closed = 0;
            /* tag name: [a-zA-Z][^\t\n\r\f />\x00]* */
            while (j < n && !ada_isspace(s[j]) && s[j] != '/' && s[j] != '>' && s[j] != '\0') j++;
            name_end = j;
            for (k = i + 1; k < name_end && at + 1 < sizeof(name); k++) name[at++] = ada_lower(s[k]);
            name[at] = '\0';
            /* find the end of the tag, honouring quoted attribute values */
            attr_start = name_end;
            k = name_end;
            while (k < n) {
                if (s[k] == '"' || s[k] == '\'') {
                    char quote = s[k++];
                    while (k < n && s[k] != quote) k++;
                    if (k < n) k++;
                    continue;
                }
                if (s[k] == '>') {
                    closed = 1;
                    if (k > attr_start && s[k - 1] == '/') self_closing = 1;
                    break;
                }
                k++;
            }
            if (closed) {
                size_t attrs_len = k - attr_start;
                ada_start_tag(&ctx, name, s + attr_start, attrs_len, self_closing);
                i = k + 1;
                continue;
            }
            /* No '>' at all: HTMLParser calls handle_data with the raw text. */
            ada_emit_data(&ctx, s + i, n - i);
            i = n;
            continue;
        }
        ada_emit_data(&ctx, "<", 1);
        i++;
    }
    return ada_sb_take(&ctx.out);
}

/* --- _pre_sanitize_matrix_markdown ---------------------------------------- */

/* Remove "<script|style ...> ... </script|style>". */
static void ada_strip_blocks(ada_sb *out, const char *s) {
    size_t n = strlen(s), i = 0;
    while (i < n) {
        if (s[i] == '<') {
            size_t j = i + 1;
            const char *name = NULL;
            size_t namelen = 0, k;
            while (j < n && ada_isspace(s[j])) j++;
            if (ada_ci_prefix(s + j, n - j, "script")) { name = "script"; namelen = 6; }
            else if (ada_ci_prefix(s + j, n - j, "style")) { name = "style"; namelen = 5; }
            if (name && (j + namelen >= n || !ada_isword(s[j + namelen]))) {
                k = j + namelen;
                while (k < n && s[k] != '>') k++;
                if (k < n) {
                    size_t m = k + 1, found = 0, after = 0;
                    while (m < n) {
                        if (s[m] == '<') {
                            size_t q = m + 1;
                            while (q < n && ada_isspace(s[q])) q++;
                            if (q < n && s[q] == '/') {
                                q++;
                                while (q < n && ada_isspace(s[q])) q++;
                                if (ada_ci_prefix(s + q, n - q, name)) {
                                    size_t r = q + namelen;
                                    while (r < n && ada_isspace(s[r])) r++;
                                    if (r < n && s[r] == '>') {
                                        found = 1;
                                        after = r + 1;
                                        break;
                                    }
                                }
                            }
                        }
                        m++;
                    }
                    if (found) {
                        i = after;
                        continue;
                    }
                }
            }
        }
        ada_sb_putc(out, s[i++]);
    }
}

static int ada_has_danger_scheme(const char *s, size_t n) {
    static const char *const BAD[] = {"javascript", "data", "vbscript", NULL};
    size_t i;
    int b;
    for (b = 0; BAD[b]; b++) {
        size_t m = strlen(BAD[b]);
        for (i = 0; i + m < n; i++) {
            if (ada_ci_prefix(s + i, n - i, BAD[b]) && s[i + m] == ':') return 1;
        }
    }
    return 0;
}

/* Remove " on...=value" and " href|src=value-with-dangerous-scheme". */
static void ada_strip_attrs(ada_sb *out, const char *s, int want_url) {
    size_t n = strlen(s), i = 0;
    while (i < n) {
        if (ada_isspace(s[i])) {
            size_t j = i;
            size_t name_end;
            int matches = 0;
            while (j < n && ada_isspace(s[j])) j++;
            if (want_url) {
                if (ada_ci_prefix(s + j, n - j, "href")) { name_end = j + 4; matches = 1; }
                else if (ada_ci_prefix(s + j, n - j, "src")) { name_end = j + 3; matches = 1; }
            } else if (ada_ci_prefix(s + j, n - j, "on")) {
                name_end = j + 2;
                if (name_end < n && (isalnum((unsigned char)s[name_end]) || s[name_end] == '_' ||
                                     s[name_end] == '-')) {
                    while (name_end < n && (isalnum((unsigned char)s[name_end]) ||
                                            s[name_end] == '_' || s[name_end] == '-')) {
                        name_end++;
                    }
                    matches = 1;
                }
            }
            if (matches) {
                size_t k = name_end;
                int removed = 0;
                while (k < n && ada_isspace(s[k])) k++;
                if (k < n && s[k] == '=') {
                    k++;
                    while (k < n && ada_isspace(s[k])) k++;
                    if (k < n && (s[k] == '"' || s[k] == '\'')) {
                        char quote = s[k++];
                        size_t value_start = k;
                        while (k < n && s[k] != quote) k++;
                        if (k < n) {
                            /* quoted value: the scheme test looks inside the quotes */
                            if (!want_url || ada_has_danger_scheme(s + value_start, k - value_start)) {
                                k++;
                                removed = 1;
                            }
                        }
                    } else {
                        size_t value_start = k;
                        while (k < n && !ada_isspace(s[k]) && s[k] != '>') k++;
                        if (k > value_start) {
                            if (!want_url ||
                                ada_has_danger_scheme(s + value_start, k - value_start)) {
                                removed = 1;
                            }
                        }
                    }
                }
                if (removed) {
                    i = k;
                    continue;
                }
            }
        }
        ada_sb_putc(out, s[i++]);
    }
}

char *w20_adapterhtm_fd926b_pre_sanitize_markdown(const char *text) {
    const char *s = text ? text : "";
    ada_sb step1, step2, step3;
    char *result;
    ada_sb_init(&step1);
    ada_sb_init(&step2);
    ada_sb_init(&step3);
    ada_strip_blocks(&step1, s);
    ada_strip_attrs(&step2, step1.buf ? step1.buf : "", 0);
    ada_strip_attrs(&step3, step2.buf ? step2.buf : "", 1);
    result = ada_sb_take(&step3);
    free(step1.buf);
    free(step2.buf);
    return result;
}
