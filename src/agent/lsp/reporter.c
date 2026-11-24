/*
 * reporter.c - C11 port of upstream agent/lsp/reporter.py
 * (gate w25_reporter_f01f70).
 *
 * PoP: w25_reporter_f01f70_sanitize @ agent/lsp/reporter.py:_sanitize_field
 * PoP: w25_reporter_f01f70_format_diagnostic @ agent/lsp/reporter.py:format_diagnostic
 * PoP: w25_reporter_f01f70_report_for_file @ agent/lsp/reporter.py:report_for_file
 * PoP: w25_reporter_f01f70_truncate @ agent/lsp/reporter.py:truncate
 * PoP: w25_reporter_f01f70_constants @ agent/lsp/reporter.py:<module constants>
 *
 * The language server, the edit that produced the diagnostics and the tool
 * result the block is spliced into are all seams.  What is ported is the pure
 * string law: how one field is made safe to embed (CR/LF collapse, control
 * chars dropped, code-point cap, HTML escape), how one diagnostic renders, how
 * the severity filter and the per-file cap bound a block, and the elision
 * marker the hard cap uses.
 *
 * Python semantics honoured here: len()/slicing count CODE POINTS, not bytes;
 * ``or 1`` makes a falsy severity fall back to ERROR; dict lookups compare
 * numerically so ``True`` and ``2.0`` find the int keys.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_reporter_f01f70.h"

/* ---------------------------------------------------------------- buffers */

typedef struct {
    char  *p;
    size_t n, cap;
} rp_buf;

static void rp_reserve(rp_buf *b, size_t extra) {
    size_t need = b->n + extra + 1;
    size_t cap;
    char *np;
    if (need <= b->cap) return;
    cap = b->cap ? b->cap : 128;
    while (cap < need) cap *= 2;
    np = (char *)realloc(b->p, cap);
    if (!np) return;
    b->p = np;
    b->cap = cap;
    if (b->n < cap) b->p[b->n] = '\0';
}

static void rp_putn(rp_buf *b, const char *s, size_t n) {
    if (!s || !n) return;
    rp_reserve(b, n);
    if (b->cap < b->n + n + 1) return;
    memcpy(b->p + b->n, s, n);
    b->n += n;
    b->p[b->n] = '\0';
}

static void rp_puts(rp_buf *b, const char *s) { if (s) rp_putn(b, s, strlen(s)); }

static const char *rp_cstr(rp_buf *b) { return b->p ? b->p : ""; }

static void rp_free(rp_buf *b) { free(b->p); b->p = NULL; b->n = b->cap = 0; }

/* ------------------------------------------------------------- utf8 count */

/* Decode one code point at byte offset i; *used is the byte width (>=1). */
static unsigned int rp_decode(const unsigned char *s, size_t len, size_t i, size_t *used) {
    unsigned int c = s[i];
    if (c < 0x80) { *used = 1; return c; }
    if ((c & 0xE0) == 0xC0 && i + 1 < len) { *used = 2; return ((c & 0x1Fu) << 6) | (s[i + 1] & 0x3Fu); }
    if ((c & 0xF0) == 0xE0 && i + 2 < len) {
        *used = 3;
        return ((c & 0x0Fu) << 12) | ((s[i + 1] & 0x3Fu) << 6) | (s[i + 2] & 0x3Fu);
    }
    if ((c & 0xF8) == 0xF0 && i + 3 < len) {
        *used = 4;
        return ((c & 0x07u) << 18) | ((s[i + 1] & 0x3Fu) << 12) | ((s[i + 2] & 0x3Fu) << 6) | (s[i + 3] & 0x3Fu);
    }
    *used = 1;
    return c;
}

static size_t rp_cp_len(const char *s) {
    size_t i = 0, n = 0, len = s ? strlen(s) : 0;
    while (i < len) { size_t u = 1; (void)rp_decode((const unsigned char *)s, len, i, &u); i += u ? u : 1; n++; }
    return n;
}

/* Bytes holding the first ``cps`` code points. */
static size_t rp_prefix_bytes(const char *s, size_t cps) {
    size_t i = 0, n = 0, len = s ? strlen(s) : 0;
    while (i < len && n < cps) { size_t u = 1; (void)rp_decode((const unsigned char *)s, len, i, &u); i += u ? u : 1; n++; }
    return i;
}

/* str.isprintable() for the code points a diagnostic field can carry. */
static int rp_printable(unsigned int cp) {
    if (cp == ' ') return 1;
    if (cp < 0x20 || cp == 0x7F) return 0;
    if (cp < 0x7F) return 1;
    if (cp >= 0x80 && cp <= 0x9F) return 0;
    if (cp == 0xAD) return 0;
    if (cp >= 0x2000 && cp <= 0x200F) return 0;
    if (cp == 0x2028 || cp == 0x2029) return 0;
    if (cp >= 0x202A && cp <= 0x202E) return 0;
    if (cp >= 0x2060 && cp <= 0x206F) return 0;
    if (cp == 0xFEFF) return 0;
    if (cp >= 0xFFF9 && cp <= 0xFFFB) return 0;
    return 1;
}

/* ---------------------------------------------------------- python values */

static int rp_truthy(const json_t *v) {
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

static int rp_number(const json_t *v, double *out) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) { *out = v->num_val; return 1; }
    if (v->type == JSON_BOOL) { *out = v->bool_val ? 1.0 : 0.0; return 1; }
    return 0;
}

/* repr(float) for the values a field can hold: ints print bare, else %.15g. */
static void rp_num_str(double v, char *buf, size_t n) {
    if (v == floor(v) && fabs(v) < 1e15) snprintf(buf, n, "%lld", (long long)v);
    else snprintf(buf, n, "%.15g", v);
}

/* str(value) for the scalar shapes a diagnostic field carries. */
static void rp_str_of(const json_t *v, char *buf, size_t n) {
    if (!v || v->type == JSON_NULL) { snprintf(buf, n, "None"); return; }
    switch (v->type) {
    case JSON_BOOL:   snprintf(buf, n, "%s", v->bool_val ? "True" : "False"); return;
    case JSON_NUMBER: rp_num_str(v->num_val, buf, n); return;
    case JSON_STRING: snprintf(buf, n, "%s", v->str_val ? v->str_val : ""); return;
    /* str() of an EMPTY container is "[]" / "{}" -- that text is what gets
     * sanitized and shown. A non-empty container would need Python's
     * single-quoted repr, which stays a documented gap. */
    case JSON_ARRAY:  snprintf(buf, n, "%s", json_len(v) ? "" : "[]"); return;
    case JSON_OBJECT: snprintf(buf, n, "%s", json_object_size(v) ? "" : "{}"); return;
    default:          snprintf(buf, n, "%s", ""); return;
    }
}

/* int(value): truncating for floats, string-of-int for strings. */
static int rp_int_of(const json_t *v, long long *out) {
    double d;
    if (!v) { *out = 0; return 1; }
    if (rp_number(v, &d)) { *out = (long long)d; return 1; }
    if (v->type == JSON_STRING && v->str_val) {
        const char *s = v->str_val;
        char *end = NULL;
        long long got;
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
        got = strtoll(s, &end, 10);
        if (end && end != s) { *out = got; return 1; }
    }
    *out = 0;
    return 0;
}

/* Python '==' over scalar JSON values (True == 1 == 1.0, null == null). */
static int rp_equal(const json_t *a, const json_t *b) {
    double x = 0.0, y = 0.0;
    if (!a || a->type == JSON_NULL) return !b || b->type == JSON_NULL;
    if (!b || b->type == JSON_NULL) return 0;
    if (rp_number(a, &x) && rp_number(b, &y)) return x == y;
    if (a->type == JSON_STRING && b->type == JSON_STRING)
        return strcmp(a->str_val ? a->str_val : "", b->str_val ? b->str_val : "") == 0;
    return 0;
}

/* ------------------------------------------------------------ html escape */

static void rp_html_escape(const char *s, int quote, rp_buf *out) {
    size_t i;
    if (!s) return;
    for (i = 0; s[i]; i++) {
        switch (s[i]) {
        case '&':  rp_puts(out, "&amp;"); break;
        case '<':  rp_puts(out, "&lt;"); break;
        case '>':  rp_puts(out, "&gt;"); break;
        case '"':  rp_puts(out, quote ? "&quot;" : "\""); break;
        case '\'': rp_puts(out, quote ? "&#x27;" : "'"); break;
        default:   rp_putn(out, s + i, 1); break;
        }
    }
}

/* --------------------------------------------------------------- sanitize */

static void rp_sanitize_into(const json_t *value, long long limit, rp_buf *out) {
    char raw[8192];
    rp_buf kept = {0};
    size_t i, len, start, end;
    const char *s;

    if (!value || value->type == JSON_NULL) return;
    rp_str_of(value, raw, sizeof raw);

    /* \r and \n collapse to a space, then non-printable code points drop. */
    len = strlen(raw);
    i = 0;
    while (i < len) {
        unsigned int cp;
        size_t used = 1;
        if (raw[i] == '\r' || raw[i] == '\n') { rp_putn(&kept, " ", 1); i++; continue; }
        cp = rp_decode((const unsigned char *)raw, len, i, &used);
        if (used == 0) used = 1;
        if (rp_printable(cp)) rp_putn(&kept, raw + i, used);
        i += used;
    }

    /* raw.strip(): only spaces survive the filter, so only spaces can strip. */
    s = rp_cstr(&kept);
    start = 0;
    end = kept.n;
    while (start < end && s[start] == ' ') start++;
    while (end > start && s[end - 1] == ' ') end--;
    if (limit >= 0) {
        size_t bytes = rp_prefix_bytes(s + start, (size_t)limit);
        if (start + bytes < end) end = start + bytes;
    } else {
        /* Python slicing: raw.strip()[:limit] with a NEGATIVE limit drops the last
         * |limit| characters ("abc"[:-1] == "ab"). The port treated any negative
         * limit as "no cap" and answered the whole string. */
        size_t drop = (size_t)(-limit);
        if (end - start > drop) end -= drop;
        else end = start;
    }
    {
        char *tmp = (char *)malloc(end - start + 1);
        if (tmp) {
            memcpy(tmp, s + start, end - start);
            tmp[end - start] = '\0';
            rp_html_escape(tmp, 0, out);
            free(tmp);
        }
    }
    rp_free(&kept);
}

json_t *w25_reporter_f01f70_sanitize(const json_t *req) {
    rp_buf b = {0};
    json_t *limit_j;
    long long limit = W25_REPORTER_F01F70_MAX_MESSAGE_CHARS;
    json_t *out;

    if (req && req->type == JSON_OBJECT) {
        limit_j = json_obj_get(req, "limit");
        if (limit_j) (void)rp_int_of(limit_j, &limit);
        rp_sanitize_into(json_obj_get(req, "value"), limit, &b);
    }
    out = json_string(rp_cstr(&b));
    rp_free(&b);
    return out;
}

/* ------------------------------------------------------- format_diagnostic */

/* SEVERITY_NAMES.get(d.get("severity") or 1, "ERROR") */
static const char *rp_sev_name(const json_t *d) {
    const json_t *key;
    double v = 1.0;
    key = d && d->type == JSON_OBJECT ? json_obj_get(d, "severity") : NULL;
    if (!rp_truthy(key)) v = 1.0;
    else if (!rp_number(key, &v)) return "ERROR";
    if (v == 1.0) return "ERROR";
    if (v == 2.0) return "WARN";
    if (v == 3.0) return "INFO";
    if (v == 4.0) return "HINT";
    return "ERROR";
}

/* (d.get("range") or {}).get("start") or {} */
static const json_t *rp_start_of(const json_t *d) {
    const json_t *range, *start;
    if (!d || d->type != JSON_OBJECT) return NULL;
    range = json_obj_get(d, "range");
    if (!rp_truthy(range) || range->type != JSON_OBJECT) return NULL;
    start = json_obj_get(range, "start");
    if (!rp_truthy(start) || start->type != JSON_OBJECT) return NULL;
    return start;
}

static long long rp_pos(const json_t *start, const char *field) {
    long long v = 0;
    if (start) (void)rp_int_of(json_obj_get(start, field), &v);
    return v;
}

static void rp_format_into(const json_t *d, rp_buf *out) {
    const char *sev = rp_sev_name(d);
    const json_t *start = rp_start_of(d);
    long long line = rp_pos(start, "line") + 1;
    long long col = rp_pos(start, "character") + 1;
    rp_buf msg = {0}, code = {0}, src = {0};
    char head[256];

    if (d && d->type == JSON_OBJECT) {
        rp_sanitize_into(json_obj_get(d, "message"), W25_REPORTER_F01F70_MAX_MESSAGE_CHARS, &msg);
        rp_sanitize_into(json_obj_get(d, "code"), W25_REPORTER_F01F70_MAX_CODE_CHARS, &code);
        rp_sanitize_into(json_obj_get(d, "source"), W25_REPORTER_F01F70_MAX_SOURCE_CHARS, &src);
    }

    snprintf(head, sizeof head, "%s [%lld:%lld] ", sev, line, col);
    rp_puts(out, head);
    rp_puts(out, rp_cstr(&msg));
    if (code.n > 0) {
        rp_puts(out, " [");
        rp_puts(out, rp_cstr(&code));
        rp_puts(out, "]");
    }
    if (src.n > 0) {
        rp_puts(out, " (");
        rp_puts(out, rp_cstr(&src));
        rp_puts(out, ")");
    }
    rp_free(&msg);
    rp_free(&code);
    rp_free(&src);
}

json_t *w25_reporter_f01f70_format_diagnostic(const json_t *d) {
    rp_buf out = {0};
    json_t *res;
    rp_format_into(d, &out);
    res = json_string(rp_cstr(&out));
    rp_free(&out);
    return res;
}

/* -------------------------------------------------------- report_for_file */

/* (d.get("severity") or 1) in severities */
static int rp_sev_selected(const json_t *d, const json_t *severities) {
    const json_t *key;
    size_t i, n;
    if (!severities || severities->type != JSON_ARRAY) return 0;
    key = d && d->type == JSON_OBJECT ? json_obj_get(d, "severity") : NULL;
    n = json_len(severities);
    if (!rp_truthy(key)) {
        for (i = 0; i < n; i++) {
            double v = 0.0;
            if (rp_number(json_get(severities, i), &v) && v == 1.0) return 1;
        }
        return 0;
    }
    for (i = 0; i < n; i++)
        if (rp_equal(key, json_get(severities, i))) return 1;
    return 0;
}

json_t *w25_reporter_f01f70_report_for_file(const json_t *req) {
    const json_t *diags, *mpf, *severities;
    json_t *sev_owned = NULL;
    long long max_per_file = W25_REPORTER_F01F70_MAX_PER_FILE;
    size_t i, n = 0, kept = 0, end, seen = 0;
    rp_buf body = {0}, out = {0};
    json_t *res;

    diags = req && req->type == JSON_OBJECT ? json_obj_get(req, "diagnostics") : NULL;
    if (!diags || diags->type != JSON_ARRAY) return json_string("");

    severities = json_obj_get(req, "severities");
    if (!severities || severities->type != JSON_ARRAY) {
        sev_owned = json_array();
        json_append(sev_owned, json_number(1));
        severities = sev_owned;
    }
    mpf = json_obj_get(req, "max_per_file");
    if (mpf) (void)rp_int_of(mpf, &max_per_file);

    n = json_len(diags);
    for (i = 0; i < n; i++)
        if (rp_sev_selected(json_get(diags, i), severities)) kept++;

    if (kept == 0) {
        json_free(sev_owned);
        return json_string("");
    }

    if (max_per_file >= 0) {
        end = (size_t)max_per_file < kept ? (size_t)max_per_file : kept;
    } else {
        long long e = (long long)kept + max_per_file;
        end = e > 0 ? (size_t)e : 0;
    }

    for (i = 0; i < n && seen < end; i++) {
        const json_t *d = json_get(diags, i);
        if (!rp_sev_selected(d, severities)) continue;
        if (seen) rp_puts(&body, "\n");
        rp_format_into(d, &body);
        seen++;
    }
    if ((long long)kept > max_per_file) {
        char tail[64];
        snprintf(tail, sizeof tail, "\n... and %lld more", (long long)kept - max_per_file);
        rp_puts(&body, tail);
    }

    rp_puts(&out, "<diagnostics file=\"");
    {
        const json_t *pj = json_obj_get(req, "file_path");
        char path[8192];
        path[0] = '\0';
        if (pj && pj->type == JSON_STRING) {
            snprintf(path, sizeof path, "%s", pj->str_val ? pj->str_val : "");
        } else if (pj && pj->type != JSON_NULL) {
            rp_str_of(pj, path, sizeof path);
        }
        rp_html_escape(path, 1, &out);
    }
    rp_puts(&out, "\">\n");
    rp_puts(&out, rp_cstr(&body));
    rp_puts(&out, "\n</diagnostics>");

    res = json_string(rp_cstr(&out));
    json_free(sev_owned);
    rp_free(&body);
    rp_free(&out);
    return res;
}

/* --------------------------------------------------------------- elision */

static void rp_thousands(long long v, char *buf, size_t n) {
    char digits[32];
    char out[48];
    size_t nd, i, o = 0;
    snprintf(digits, sizeof digits, "%lld", v < 0 ? -v : v);
    nd = strlen(digits);
    if (v < 0 && o + 1 < sizeof out) out[o++] = '-';
    for (i = 0; i < nd; i++) {
        if (i && (nd - i) % 3 == 0 && o + 1 < sizeof out) out[o++] = ',';
        if (o + 1 < sizeof out) out[o++] = digits[i];
    }
    out[o] = '\0';
    snprintf(buf, n, "%s", out);
}

/* _elision_marker: first sentence of the compression marker, per instance. */
static size_t rp_marker(char *buf, size_t n, long long omitted, long long total) {
    char o[48], t[48];
    rp_thousands(omitted, o, sizeof o);
    rp_thousands(total, t, sizeof t);
    return (size_t)snprintf(buf, n,
        "\xe2\x9f\xaaHERMES-CONTEXT-COMPRESSION: %s of %s chars omitted here by Hermes's "
        "context compressor.\xe2\x9f\xab", o, t);
}

static void rp_rstrip(char *s) {
    size_t n = s ? strlen(s) : 0;
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\n' || s[n - 1] == '\r' ||
                     s[n - 1] == '\f' || s[n - 1] == '\v'))
        s[--n] = '\0';
}

static json_t *rp_elide(const char *text, long long limit) {
    size_t total = rp_cp_len(text);
    char marker[256];
    long long head;
    size_t bytes;
    char *kept;
    rp_buf out = {0};
    json_t *res;

    if ((long long)total <= limit) return json_string(text ? text : "");
    (void)rp_marker(marker, sizeof marker, (long long)total, (long long)total);
    head = limit - (long long)rp_cp_len(marker);
    if (head <= 0) return json_string(marker);

    bytes = rp_prefix_bytes(text, (size_t)head);
    kept = (char *)malloc(bytes + 1);
    if (!kept) return json_string(marker);
    memcpy(kept, text, bytes);
    kept[bytes] = '\0';
    rp_rstrip(kept);
    (void)rp_marker(marker, sizeof marker, (long long)(total - rp_cp_len(kept)), (long long)total);
    rp_puts(&out, kept);
    rp_puts(&out, marker);
    res = json_string(rp_cstr(&out));
    rp_free(&out);
    free(kept);
    return res;
}

json_t *w25_reporter_f01f70_truncate(const json_t *req) {
    const char *s = "";
    long long limit = W25_REPORTER_F01F70_MAX_TOTAL_CHARS;
    const json_t *j;
    if (req && req->type == JSON_OBJECT) {
        j = json_obj_get(req, "limit");
        if (j) (void)rp_int_of(j, &limit);
        s = json_get_str(req, "s", "");
    }
    return rp_elide(s, limit);
}

/* -------------------------------------------------------------- constants */

json_t *w25_reporter_f01f70_constants(void) {
    json_t *o = json_object();
    json_t *names = json_object();
    json_t *defaults = json_array();
    json_set(names, "1", json_string("ERROR"));
    json_set(names, "2", json_string("WARN"));
    json_set(names, "3", json_string("INFO"));
    json_set(names, "4", json_string("HINT"));
    json_set(o, "severity_names", names);
    json_append(defaults, json_number(1));
    json_set(o, "default_severities", defaults);
    json_set(o, "max_per_file", json_number((double)W25_REPORTER_F01F70_MAX_PER_FILE));
    json_set(o, "max_total_chars", json_number((double)W25_REPORTER_F01F70_MAX_TOTAL_CHARS));
    json_set(o, "max_message_chars", json_number((double)W25_REPORTER_F01F70_MAX_MESSAGE_CHARS));
    json_set(o, "max_code_chars", json_number((double)W25_REPORTER_F01F70_MAX_CODE_CHARS));
    json_set(o, "max_source_chars", json_number((double)W25_REPORTER_F01F70_MAX_SOURCE_CHARS));
    return o;
}
