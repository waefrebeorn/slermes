/*
 * client.c - C11 port of upstream agent/lsp/client.py (gate w23_client_785ca6).
 *
 * PoP: w23_client_785ca6_file_uri @ agent/lsp/client.py:file_uri
 * PoP: w23_client_785ca6_uri_to_path @ agent/lsp/client.py:uri_to_path
 * PoP: w23_client_785ca6_folder @ agent/lsp/client.py:_folder
 * PoP: w23_client_785ca6_end_position @ agent/lsp/client.py:_end_position
 * PoP: w23_client_785ca6_as_disk_read @ agent/lsp/client.py:as_disk_read
 * PoP: w23_client_785ca6_diagnostic_key @ agent/lsp/client.py:_diagnostic_key
 * PoP: w23_client_785ca6_dedupe @ agent/lsp/client.py:_dedupe
 * PoP: w23_client_785ca6_doc_fresh @ agent/lsp/client.py:_DocState.fresh
 * PoP: w23_client_785ca6_diagnostics_for @ agent/lsp/client.py:LSPClient.diagnostics_for
 * PoP: w23_client_785ca6_document_text @ agent/lsp/client.py:LSPClient.document_text
 * PoP: w23_client_785ca6_config_section @ agent/lsp/client.py:LSPClient._config_section
 * PoP: w23_client_785ca6_workspace_configuration @ agent/lsp/client.py:LSPClient._handle_workspace_configuration
 * PoP: w23_client_785ca6_limits @ agent/lsp/client.py:MAX_CONTENT_MODIFIED_RETRIES
 *
 * The server process, its stdio pipes and the asyncio loop are the seam: what
 * this file owns is the path/URI arithmetic, the UTF-16 end position, the
 * document-version freshness rule and diagnostic identity -- all of it
 * decidable from values the transport already returned.
 */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_w23_client_785ca6.h"

#if defined(_WIN32)
#include <direct.h>
#define NC_GETCWD _getcwd
#else
#include <unistd.h>
#define NC_GETCWD getcwd
#endif

#define U_BOM "\xef\xbb\xbf" /* U+FEFF */

/* ------------------------------------------------------------------ */
/* growable byte buffer                                                */
/* ------------------------------------------------------------------ */

typedef struct {
    char *p;
    size_t n, cap;
} sb_t;

static void sb_init(sb_t *b) {
    b->cap = 96;
    b->n = 0;
    b->p = (char *)malloc(b->cap);
    if (b->p) b->p[0] = '\0';
}

static void sb_grow(sb_t *b, size_t extra) {
    size_t want;
    char *q;
    if (!b->p) return;
    want = b->n + extra + 1;
    if (want <= b->cap) return;
    while (b->cap < want) b->cap *= 2;
    q = (char *)realloc(b->p, b->cap);
    if (!q) return;
    b->p = q;
}

static void sb_putn(sb_t *b, const char *s, size_t n) {
    if (!b->p || !s) return;
    sb_grow(b, n);
    if (!b->p) return;
    if (n) memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
}

static void sb_puts(sb_t *b, const char *s) {
    if (s) sb_putn(b, s, strlen(s));
}

static void sb_putc(sb_t *b, char c) { sb_putn(b, &c, 1); }

static char *sb_take(sb_t *b) {
    char *out = b->p;
    b->p = NULL;
    b->n = b->cap = 0;
    return out;
}

static char *nc_dup(const char *s) {
    size_t n;
    char *out;
    if (!s) s = "";
    n = strlen(s);
    out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

/* ------------------------------------------------------------------ */
/* UTF-8                                                               */
/* ------------------------------------------------------------------ */

/* Decode one code point; invalid input consumes the maximal invalid prefix
 * and answers U+FFFD, the way Python's errors="replace" decoder does. */
static unsigned nc_u8_dec(const unsigned char *s, size_t len, size_t *adv) {
    unsigned cp;
    size_t need, i;
    unsigned char c = s[0];

    if (c < 0x80) {
        *adv = 1;
        return c;
    }
    if ((c & 0xE0) == 0xC0) {
        need = 1;
        cp = (unsigned)(c & 0x1F);
        if (cp < 2) {
            *adv = 1;
            return 0xFFFD;
        }
    } else if ((c & 0xF0) == 0xE0) {
        need = 2;
        cp = (unsigned)(c & 0x0F);
    } else if ((c & 0xF8) == 0xF0) {
        need = 3;
        cp = (unsigned)(c & 0x07);
    } else {
        *adv = 1;
        return 0xFFFD;
    }
    for (i = 1; i <= need; i++) {
        if (i >= len) { /* truncated: the valid prefix is one bad unit */
            *adv = i;
            return 0xFFFD;
        }
        if ((s[i] & 0xC0) != 0x80) {
            *adv = i;
            return 0xFFFD;
        }
        cp = (cp << 6) | (unsigned)(s[i] & 0x3F);
    }
    if (need == 2 && cp < 0x800) {
        *adv = 1;
        return 0xFFFD;
    }
    if (need == 3 && (cp < 0x10000 || cp > 0x10FFFF)) {
        *adv = 1;
        return 0xFFFD;
    }
    if (cp >= 0xD800 && cp <= 0xDFFF) {
        *adv = 1;
        return 0xFFFD;
    }
    *adv = need + 1;
    return cp;
}

static void nc_u8_enc(sb_t *b, unsigned cp) {
    char o[4];
    if (cp < 0x80) {
        o[0] = (char)cp;
        sb_putn(b, o, 1);
    } else if (cp < 0x800) {
        o[0] = (char)(0xC0 | (cp >> 6));
        o[1] = (char)(0x80 | (cp & 0x3F));
        sb_putn(b, o, 2);
    } else if (cp < 0x10000) {
        o[0] = (char)(0xE0 | (cp >> 12));
        o[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        o[2] = (char)(0x80 | (cp & 0x3F));
        sb_putn(b, o, 3);
    } else {
        o[0] = (char)(0xF0 | (cp >> 18));
        o[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        o[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        o[3] = (char)(0x80 | (cp & 0x3F));
        sb_putn(b, o, 4);
    }
}

/* bytes -> valid UTF-8, undecodable bytes replaced with U+FFFD */
static char *nc_u8_sanitize(const char *s) {
    sb_t b;
    size_t i = 0, n;
    sb_init(&b);
    if (!s) return sb_take(&b);
    n = strlen(s);
    while (i < n) {
        size_t adv = 0;
        unsigned cp = nc_u8_dec((const unsigned char *)s + i, n - i, &adv);
        nc_u8_enc(&b, cp);
        i += adv ? adv : 1;
    }
    return sb_take(&b);
}

/* ------------------------------------------------------------------ */
/* Python str() / repr() coercions for JSON values                     */
/* ------------------------------------------------------------------ */

static void nc_py_repr_into(sb_t *b, const json_t *v);

static void nc_fmt_num(sb_t *b, double d) {
    char buf[64];
    if (d == (double)(long long)d) {
        snprintf(buf, sizeof(buf), "%lld", (long long)d);
    } else {
        int p;
        buf[0] = '\0';
        for (p = 12; p <= 17; p++) {
            snprintf(buf, sizeof(buf), "%.*g", p, d);
            if (strtod(buf, NULL) == d) break;
        }
        if (!buf[0]) snprintf(buf, sizeof(buf), "%g", d);
    }
    sb_puts(b, buf);
}

static void nc_repr_str_into(sb_t *b, const char *s) {
    sb_putc(b, '\'');
    if (s) {
        for (; *s; s++) {
            unsigned char c = (unsigned char)*s;
            if (c == '\'') sb_puts(b, "\\'");
            else if (c == '\\') sb_puts(b, "\\\\");
            else if (c == '\n') sb_puts(b, "\\n");
            else if (c == '\r') sb_puts(b, "\\r");
            else if (c == '\t') sb_puts(b, "\\t");
            else sb_putc(b, (char)c);
        }
    }
    sb_putc(b, '\'');
}

static void nc_py_repr_into(sb_t *b, const json_t *v) {
    size_t i, n;
    if (!v) {
        sb_puts(b, "None");
        return;
    }
    switch (v->type) {
    case JSON_NULL: sb_puts(b, "None"); break;
    case JSON_BOOL: sb_puts(b, v->bool_val ? "True" : "False"); break;
    case JSON_NUMBER: nc_fmt_num(b, v->num_val); break;
    case JSON_STRING: nc_repr_str_into(b, v->str_val); break;
    case JSON_ARRAY:
        sb_putc(b, '[');
        n = json_len(v);
        for (i = 0; i < n; i++) {
            if (i) sb_puts(b, ", ");
            nc_py_repr_into(b, json_get(v, i));
        }
        sb_putc(b, ']');
        break;
    case JSON_OBJECT:
        sb_putc(b, '{');
        n = json_object_size(v);
        for (i = 0; i < n; i++) {
            if (i) sb_puts(b, ", ");
            nc_repr_str_into(b, json_object_get_key_at(v, i));
            sb_puts(b, ": ");
            nc_py_repr_into(b, json_object_get_at(v, i));
        }
        sb_putc(b, '}');
        break;
    default: sb_puts(b, "None"); break;
    }
}

static void nc_py_str_into(sb_t *b, const json_t *v) {
    if (v && v->type == JSON_STRING) {
        sb_puts(b, v->str_val ? v->str_val : "");
        return;
    }
    if (v && v->type == JSON_NUMBER) {
        nc_fmt_num(b, v->num_val);
        return;
    }
    nc_py_repr_into(b, v);
}

/* truthiness of a Python value */
static int nc_truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL: return 0;
    case JSON_BOOL: return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0];
    case JSON_ARRAY: return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    default: return 0;
    }
}

static int nc_ws_char(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

/* str.strip() for ASCII whitespace */
static void nc_strip_copy(sb_t *b, const char *s) {
    size_t n, start = 0;
    if (!s) return;
    n = strlen(s);
    while (start < n && nc_ws_char(s[start])) start++;
    while (n > start && nc_ws_char(s[n - 1])) n--;
    sb_putn(b, s + start, n - start);
}

/* ------------------------------------------------------------------ */
/* ntpath-shaped path arithmetic (the host's os.path is ntpath)        */
/* ------------------------------------------------------------------ */

static int nc_is_sep(char c) { return c == '\\' || c == '/'; }

static int nc_has_drive(const char *p) {
    return p && p[0] && p[1] == ':' &&
           ((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z'));
}

static char *nc_normpath(const char *path) {
    char *work, *p;
    sb_t out;
    char **comps;
    size_t comps_n = 0, comps_cap = 16, i, len;

    work = nc_dup(path ? path : "");
    if (!work) return NULL;
    for (i = 0; work[i]; i++)
        if (work[i] == '/') work[i] = '\\';

    sb_init(&out);
    i = 0;
    if (nc_has_drive(work)) {
        sb_putn(&out, work, 2);
        i = 2;
        if (work[i] == '\\') {
            sb_putc(&out, '\\');
            i++;
        }
    } else if (work[0] == '\\') {
        if (work[1] == '\\') {
            sb_puts(&out, "\\\\");
            i = 2;
        } else {
            sb_putc(&out, '\\');
            i = 1;
        }
    }

    comps = (char **)malloc(comps_cap * sizeof(char *));
    if (!comps) {
        free(work);
        return sb_take(&out);
    }
    p = work + i;
    while (*p) {
        const char *start;
        while (*p == '\\') p++;
        if (!*p) break;
        start = p;
        while (*p && *p != '\\') p++;
        len = (size_t)(p - start);
        if (len == 1 && start[0] == '.') continue;
        if (len == 2 && start[0] == '.' && start[1] == '.') {
            if (comps_n > 0 && strcmp(comps[comps_n - 1], "..") != 0) {
                free(comps[--comps_n]);
            } else if (out.n == 0 || out.p[out.n - 1] != '\\') {
                char **nq;
                if (comps_n + 2 > comps_cap) {
                    comps_cap *= 2;
                    nq = (char **)realloc(comps, comps_cap * sizeof(char *));
                    if (!nq) break;
                    comps = nq;
                }
                comps[comps_n++] = nc_dup("..");
            }
            continue;
        }
        if (comps_n + 2 > comps_cap) {
            char **nq;
            comps_cap *= 2;
            nq = (char **)realloc(comps, comps_cap * sizeof(char *));
            if (!nq) break;
            comps = nq;
        }
        comps[comps_n] = (char *)malloc(len + 1);
        if (!comps[comps_n]) break;
        memcpy(comps[comps_n], start, len);
        comps[comps_n][len] = '\0';
        comps_n++;
    }

    for (i = 0; i < comps_n; i++) {
        if (i > 0 && (out.n == 0 || out.p[out.n - 1] != '\\')) sb_putc(&out, '\\');
        sb_puts(&out, comps[i]);
        free(comps[i]);
    }
    free(comps);
    free(work);
    if (out.n == 0) sb_puts(&out, ".");
    return sb_take(&out);
}

static char *nc_abspath(const char *path) {
    char joined[8192];
    char cwd[4096];

    if (!path) path = "";
    if (nc_has_drive(path) && nc_is_sep(path[2])) return nc_normpath(path);
    if (nc_is_sep(path[0]) && nc_is_sep(path[1])) return nc_normpath(path);
    if (!NC_GETCWD(cwd, (int)sizeof(cwd))) return nc_normpath(path);
    if (nc_is_sep(path[0])) {
        snprintf(joined, sizeof(joined), "%.2s%s", cwd, path);
        return nc_normpath(joined);
    }
    snprintf(joined, sizeof(joined), "%s\\%s", cwd, path);
    return nc_normpath(joined);
}

static int nc_unreserved(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return 1;
    if (c >= 'a' && c <= 'z') return 1;
    if (c >= '0' && c <= '9') return 1;
    return c == '-' || c == '.' || c == '_' || c == '~';
}

static void nc_quote_into(sb_t *b, const char *s) {
    static const char *hex = "0123456789ABCDEF";
    const unsigned char *p = (const unsigned char *)s;
    if (!s) return;
    for (; *p; p++) {
        if (nc_unreserved(*p) || *p == '/' || *p == ':') {
            sb_putc(b, (char)*p);
        } else {
            char esc[3];
            esc[0] = '%';
            esc[1] = hex[*p >> 4];
            esc[2] = hex[*p & 0x0F];
            sb_putn(b, esc, 3);
        }
    }
}

static int nc_hexval(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* urllib.parse.unquote: invalid escapes stay literal, bytes decode as UTF-8
 * with undecodable sequences replaced. */
static char *nc_unquote(const char *s) {
    sb_t raw, out;
    size_t i, n;
    sb_init(&raw);
    sb_init(&out);
    if (!s) {
        free(sb_take(&raw));
        return sb_take(&out);
    }
    n = strlen(s);
    for (i = 0; i < n; i++) {
        if (s[i] == '%' && i + 2 < n && nc_hexval(s[i + 1]) >= 0 && nc_hexval(s[i + 2]) >= 0) {
            char c = (char)((nc_hexval(s[i + 1]) << 4) | nc_hexval(s[i + 2]));
            sb_putc(&raw, c);
            i += 2;
        } else {
            sb_putc(&raw, s[i]);
        }
    }
    {
        char *san = nc_u8_sanitize(raw.p ? raw.p : "");
        sb_puts(&out, san);
        free(san);
    }
    free(sb_take(&raw));
    return sb_take(&out);
}

/* ------------------------------------------------------------------ */
/* public surface                                                      */
/* ------------------------------------------------------------------ */

json_t *w23_client_785ca6_limits(void) {
    json_t *o = json_object();
    json_t *live = json_array();
    json_set(o, "max_content_modified_retries", json_number((double)W23_CLIENT_785CA6_MAX_CONTENT_MODIFIED_RETRIES));
    json_set(o, "stderr_tail_lines", json_number((double)W23_CLIENT_785CA6_STDERR_TAIL_LINES));
    json_set(o, "max_tracked_files", json_number((double)W23_CLIENT_785CA6_MAX_TRACKED_FILES));
    json_set(o, "stream_limit", json_number((double)W23_CLIENT_785CA6_STREAM_LIMIT));
    json_set(o, "initialize_timeout", json_number(45.0));
    json_set(o, "diagnostics_document_wait", json_number(5.0));
    json_set(o, "diagnostics_full_wait", json_number(10.0));
    json_set(o, "diagnostics_request_timeout", json_number(3.0));
    json_set(o, "push_debounce", json_number(0.15));
    json_set(o, "shutdown_grace", json_number(1.0));
    json_set(o, "retry_base_delay", json_number(0.5));
    json_append(live, json_string("running"));
    json_append(live, json_string("starting"));
    json_set(o, "live_states", live);
    return o;
}

char *w23_client_785ca6_file_uri(const char *path, const char *os_name) {
    char *abs = nc_abspath(path);
    sb_t b;
    sb_init(&b);
    sb_puts(&b, "file://");
    if (!abs) return sb_take(&b);
    if (os_name && strcmp(os_name, "nt") == 0) {
        sb_t m;
        size_t i;
        sb_init(&m);
        for (i = 0; abs[i]; i++) sb_putc(&m, abs[i] == '\\' ? '/' : abs[i]);
        if (!m.p || m.p[0] != '/') sb_putc(&b, '/');
        nc_quote_into(&b, m.p ? m.p : "");
        free(sb_take(&m));
    } else {
        nc_quote_into(&b, abs);
    }
    free(abs);
    return sb_take(&b);
}

char *w23_client_785ca6_uri_to_path(const char *uri, const char *os_name) {
    const char *raw;
    char *dec, *res;
    if (!uri) uri = "";
    if (strncmp(uri, "file://", 7) != 0) return nc_dup(uri);
    raw = uri + 7;
    if (os_name && strcmp(os_name, "nt") == 0 && raw[0] == '/' && strlen(raw) > 2 && raw[2] == ':')
        raw++;
    dec = nc_unquote(raw);
    res = nc_normpath(dec ? dec : "");
    free(dec);
    return res;
}

/* ntpath.basename */
static char *nc_basename(const char *p) {
    const char *start = p, *q;
    if (!p) return nc_dup("");
    if (nc_has_drive(p) && nc_is_sep(p[2])) start = p + 2;
    for (q = start; *q; q++) {
        if (nc_is_sep(*q)) start = q + 1;
    }
    return nc_dup(start);
}

json_t *w23_client_785ca6_folder(const char *root, const char *os_name) {
    char *trimmed, *name, *uri;
    size_t n;
    json_t *o = json_object();
    if (!root) root = "";
    trimmed = nc_dup(root);
    if (trimmed) {
        n = strlen(trimmed);
        while (n > 0 && trimmed[n - 1] == '\\') trimmed[--n] = '\0';
    }
    name = nc_basename(trimmed ? trimmed : "");
    if (!name || !name[0]) {
        free(name);
        name = nc_dup(root);
    }
    uri = w23_client_785ca6_file_uri(root, os_name);
    json_set(o, "name", json_string(name ? name : ""));
    json_set(o, "uri", json_string(uri ? uri : ""));
    free(name);
    free(uri);
    free(trimmed);
    return o;
}

/* One splitlines() step at s[i]: break length, 0 when there is none. */
static size_t nc_line_break(const char *s, size_t i, size_t n) {
    unsigned char c = (unsigned char)s[i];
    if (c == '\n') return 1;
    if (c == '\r') {
        if (i + 1 < n && s[i + 1] == '\n') return 2;
        return 1;
    }
    if (c == '\v' || c == '\f') return 1;
    if (c >= 0x1C && c <= 0x1E) return 1;
    if (c == 0xC2 && i + 1 < n && (unsigned char)s[i + 1] == 0x85) return 2;
    if (c == 0xE2 && i + 2 < n && (unsigned char)s[i + 1] == 0x80 &&
        ((unsigned char)s[i + 2] == 0xA8 || (unsigned char)s[i + 2] == 0xA9))
        return 3;
    return 0;
}

json_t *w23_client_785ca6_end_position(const char *text) {
    size_t n, i = 0, breaks = 0, last_start = 0, last_break_end = 0;
    int ends_ascii;
    json_t *o = json_object();
    if (!text) text = "";
    n = strlen(text);
    if (n == 0) {
        json_set(o, "line", json_number(0));
        json_set(o, "character", json_number(0));
        return o;
    }
    while (i < n) {
        size_t brk = nc_line_break(text, i, n);
        if (brk) {
            breaks++;
            i += brk;
            last_start = i;
            last_break_end = i;
        } else {
            size_t adv = 0;
            (void)nc_u8_dec((const unsigned char *)text + i, n - i, &adv);
            i += adv ? adv : 1;
        }
    }
    ends_ascii = (unsigned char)text[n - 1] == '\n' || (unsigned char)text[n - 1] == '\r';
    {
        size_t total_lines = (breaks > 0 && last_break_end == n) ? breaks : breaks + 1;
        json_set(o, "line", json_number(ends_ascii ? (double)total_lines : (double)(total_lines - 1)));
    }
    if (ends_ascii) {
        json_set(o, "character", json_number(0));
    } else {
        size_t j = last_start, units = 0;
        while (j < n) {
            size_t adv = 0;
            unsigned cp = nc_u8_dec((const unsigned char *)text + j, n - j, &adv);
            units += cp > 0xFFFF ? 2 : 1;
            j += adv ? adv : 1;
        }
        json_set(o, "character", json_number((double)units));
    }
    return o;
}

char *w23_client_785ca6_as_disk_read(const char *text) {
    char *san = nc_u8_sanitize(text ? text : "");
    sb_t b;
    size_t i = 0;
    if (!san) return NULL;
    if (strncmp(san, U_BOM, 3) == 0) i = 3;
    sb_init(&b);
    while (san[i]) {
        if (san[i] == '\r') {
            if (san[i + 1] == '\n') i++;
            sb_putc(&b, '\n');
            i++;
        } else {
            sb_putc(&b, san[i]);
            i++;
        }
    }
    free(san);
    return sb_take(&b);
}

char *w23_client_785ca6_diagnostic_key(const json_t *diag, size_t *len) {
    const json_t *rng = NULL, *start = NULL, *end = NULL;
    const json_t *sev = NULL, *code = NULL, *src = NULL, *msg = NULL;
    sb_t b;

    if (len) *len = 0;

    if (diag && diag->type == JSON_OBJECT) {
        rng = json_obj_get(diag, "range");
        if (rng && rng->type != JSON_OBJECT) rng = NULL;
        if (rng) {
            start = json_obj_get(rng, "start");
            end = json_obj_get(rng, "end");
            if (start && start->type != JSON_OBJECT) start = NULL;
            if (end && end->type != JSON_OBJECT) end = NULL;
        }
        sev = json_obj_get(diag, "severity");
        code = json_obj_get(diag, "code");
        src = json_obj_get(diag, "source");
        msg = json_obj_get(diag, "message");
    }

    sb_init(&b);
    /* severity: d.get("severity") or 1 */
    if (nc_truthy(sev)) nc_py_str_into(&b, sev);
    else sb_putc(&b, '1');
    sb_putc(&b, '\0');
    /* code: "" when None, else str(code) */
    if (code && code->type != JSON_NULL) nc_py_str_into(&b, code);
    sb_putc(&b, '\0');
    /* source: str(d.get("source") or "") */
    if (nc_truthy(src)) nc_py_str_into(&b, src);
    sb_putc(&b, '\0');
    /* message: str(d.get("message") or "").strip() */
    {
        sb_t raw, stripped;
        sb_init(&raw);
        sb_init(&stripped);
        if (nc_truthy(msg)) nc_py_str_into(&raw, msg);
        nc_strip_copy(&stripped, raw.p ? raw.p : "");
        sb_puts(&b, stripped.p ? stripped.p : "");
        free(sb_take(&raw));
        free(sb_take(&stripped));
    }
    sb_putc(&b, '\0');
    {
        const json_t *ends[2];
        int k, m;
        ends[0] = start;
        ends[1] = end;
        for (k = 0; k < 2; k++) {
            for (m = 0; m < 2; m++) {
                const json_t *v = ends[k] ? json_obj_get(ends[k], m ? "character" : "line") : NULL;
                if (m) sb_putc(&b, ':');
                if (v) nc_py_str_into(&b, v);
                else sb_putc(&b, '0');
            }
            if (!k) sb_putc(&b, '-');
        }
    }
    if (len) *len = b.n;
    return sb_take(&b);
}

static int nc_key_seen(char **seen, size_t *seen_len, size_t n, const char *key, size_t key_len) {
    size_t i;
    for (i = 0; i < n; i++) {
        if (seen_len[i] == key_len && memcmp(seen[i], key, key_len) == 0) return 1;
    }
    return 0;
}

json_t *w23_client_785ca6_dedupe(const json_t *lists) {
    json_t *out = json_array();
    char **seen = NULL;
    size_t *seen_len = NULL;
    size_t seen_n = 0, seen_cap = 0, li, n;
    if (!lists || lists->type != JSON_ARRAY) return out;
    n = json_len(lists);
    for (li = 0; li < n; li++) {
        const json_t *lst = json_get(lists, li);
        size_t d, dn;
        if (!lst || lst->type != JSON_ARRAY) continue;
        dn = json_len(lst);
        for (d = 0; d < dn; d++) {
            json_t *item = json_get(lst, d);
            char *key;
            size_t key_len = 0;
            if (!item || item->type != JSON_OBJECT) continue;
            key = w23_client_785ca6_diagnostic_key(item, &key_len);
            if (!key) continue;
            if (nc_key_seen(seen, seen_len, seen_n, key, key_len)) {
                free(key);
                continue;
            }
            if (seen_n + 2 > seen_cap) {
                char **nq;
                size_t *nl;
                seen_cap = seen_cap ? seen_cap * 2 : 8;
                nq = (char **)realloc(seen, seen_cap * sizeof(char *));
                nl = (size_t *)realloc(seen_len, seen_cap * sizeof(size_t));
                if (!nq || !nl) {
                    if (nq) seen = nq;
                    if (nl) seen_len = nl;
                    free(key);
                    continue;
                }
                seen = nq;
                seen_len = nl;
            }
            seen[seen_n] = key;
            seen_len[seen_n] = key_len;
            seen_n++;
            json_append(out, json_copy(item));
        }
    }
    for (li = 0; li < seen_n; li++) free(seen[li]);
    free(seen);
    free(seen_len);
    return out;
}

static int nc_doc_int(const json_t *doc, const char *key, int def) {
    const json_t *v = doc && doc->type == JSON_OBJECT ? json_obj_get(doc, key) : NULL;
    if (!v || v->type != JSON_NUMBER) return def;
    return (int)v->num_val;
}

int w23_client_785ca6_doc_fresh(const json_t *doc, const char *kind, const json_t *at) {
    int version = nc_doc_int(doc, "version", 0);
    int push_version = nc_doc_int(doc, "push_version", -1);
    int pull_version = nc_doc_int(doc, "pull_version", -1);
    int against = version;
    int fresh_push, fresh_pull;
    if (at && at->type == JSON_NUMBER) against = (int)at->num_val;
    fresh_push = push_version >= against;
    fresh_pull = pull_version >= against;
    if (kind && strcmp(kind, "push") == 0) return fresh_push;
    if (kind && strcmp(kind, "pull") == 0) return fresh_pull;
    return fresh_push || fresh_pull;
}

static const json_t *nc_find_doc(const json_t *docs, const char *path) {
    size_t i, n;
    if (!docs || docs->type != JSON_ARRAY) return NULL;
    n = json_len(docs);
    for (i = 0; i < n; i++) {
        const json_t *d = json_get(docs, i);
        const char *p = d && d->type == JSON_OBJECT ? json_get_str(d, "path", NULL) : NULL;
        if (p && path && strcmp(p, path) == 0) return d;
    }
    return NULL;
}

json_t *w23_client_785ca6_diagnostics_for(const json_t *req) {
    const char *path = req && req->type == JSON_OBJECT ? json_get_str(req, "path", "") : "";
    const json_t *docs = req && req->type == JSON_OBJECT ? json_obj_get(req, "docs") : NULL;
    int fresh_only = (req && req->type == JSON_OBJECT && json_get_bool(req, "fresh_only", 0)) ? 1 : 0;
    const json_t *doc = nc_find_doc(docs, path);
    json_t *lists, *out;
    const json_t *push, *pull;
    if (!doc) return json_array();
    push = json_obj_get(doc, "push");
    pull = json_obj_get(doc, "pull");
    lists = json_array();
    if (!fresh_only || w23_client_785ca6_doc_fresh(doc, "push", NULL))
        json_append(lists, push && push->type == JSON_ARRAY ? json_copy(push) : json_array());
    else
        json_append(lists, json_array());
    if (!fresh_only || w23_client_785ca6_doc_fresh(doc, "pull", NULL))
        json_append(lists, pull && pull->type == JSON_ARRAY ? json_copy(pull) : json_array());
    else
        json_append(lists, json_array());
    out = w23_client_785ca6_dedupe(lists);
    json_free(lists);
    return out;
}

json_t *w23_client_785ca6_document_text(const json_t *req) {
    const char *path = req && req->type == JSON_OBJECT ? json_get_str(req, "path", "") : "";
    const json_t *docs = req && req->type == JSON_OBJECT ? json_obj_get(req, "docs") : NULL;
    const json_t *doc = nc_find_doc(docs, path);
    const json_t *text;
    if (!doc) return json_null();
    if (nc_doc_int(doc, "version", 0) < 0) return json_null();
    text = json_obj_get(doc, "text");
    if (text && text->type == JSON_STRING) return json_copy(text);
    return json_string("");
}

json_t *w23_client_785ca6_config_section(const json_t *item, const json_t *init_options) {
    const json_t *section;
    const json_t *cur = init_options;
    char *sec_str;
    size_t i, n;

    if (!item || item->type != JSON_OBJECT) return json_null();
    section = json_obj_get(item, "section");
    if (!nc_truthy(section) || !nc_truthy(init_options))
        return nc_truthy(init_options) ? json_copy(init_options) : json_null();

    {
        sb_t b;
        sb_init(&b);
        nc_py_str_into(&b, section);
        sec_str = sb_take(&b);
    }
    if (!sec_str) return json_null();
    n = strlen(sec_str);
    i = 0;
    for (;;) {
        size_t start = i, len;
        char *piece;
        const json_t *next;
        while (i < n && sec_str[i] != '.') i++;
        len = i - start;
        piece = (char *)malloc(len + 1);
        if (!piece) break;
        memcpy(piece, sec_str + start, len);
        piece[len] = '\0';
        next = (cur && cur->type == JSON_OBJECT) ? json_obj_get(cur, piece) : NULL;
        free(piece);
        if (!next) {
            free(sec_str);
            return json_null();
        }
        cur = next;
        if (i >= n) break;
        i++; /* skip the dot */
    }
    free(sec_str);
    return json_copy(cur);
}

json_t *w23_client_785ca6_workspace_configuration(const json_t *params, const json_t *init_options) {
    json_t *out = json_array();
    const json_t *items;
    size_t i, n;
    if (!params || params->type != JSON_OBJECT) {
        json_append(out, json_null());
        return out;
    }
    items = json_obj_get(params, "items");
    if (!nc_truthy(items) || items->type != JSON_ARRAY) return out;
    n = json_len(items);
    for (i = 0; i < n; i++)
        json_append(out, w23_client_785ca6_config_section(json_get(items, i), init_options));
    return out;
}
