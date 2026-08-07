/*
 * t_port_tools_tts_text_normalize.c — faithful verification harness for
 * src/tools/port_tools_tts_text_normalize.c (port of tools/tts_text_normalize.py).
 *
 * Reads a JSON array of {func, in} cases from argv[1]; runs the matching C
 * function on each input; emits one JSON line per case:
 *   {"func":"...","in":"...","out":"..."}
 *
 * The Python oracle (tests/sta_oracle_tools_tts_text_normalize.py) imports the
 * LIVE tools/tts_text_normalize module and recomputes the same function on
 * each input; the runner diffs the two JSONL streams byte-for-byte.
 *
 * The harness links the SAME object closure as the real `slermes` binary
 * (via run_oracle.sh's make -n extraction), so PCRE2, libhtml are present.
 * The JSON reader/emitter below is intentionally tiny: the fixture is a flat
 * array of {"func":"...","in":"..."} objects, and "out" may contain the
 * heading sentinel byte \x00 (emitted as \u0000).
 */

#include "port_tools_tts_text_normalize.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* File slurp                                                          */
/* ------------------------------------------------------------------ */

static char *read_all(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f);
    buf[r] = '\0';
    fclose(f);
    return buf;
}

/* ------------------------------------------------------------------ */
/* Tiny dynabuffer for JSON emission                                   */
/* ------------------------------------------------------------------ */

typedef struct {
    char  *buf;
    size_t len, cap;
} jbuf_t;

static void jb_init(jbuf_t *b)
{
    b->cap = 256; b->len = 0;
    b->buf = (char *)malloc(b->cap);
    if (!b->buf) abort();
    b->buf[0] = '\0';
}

static void jb_reserve(jbuf_t *b, size_t need)
{
    if (b->len + need + 1 <= b->cap) return;
    size_t n = b->cap * 2;
    while (n < b->len + need + 1) n *= 2;
    char *nb = (char *)realloc(b->buf, n);
    if (!nb) abort();
    b->buf = nb; b->cap = n;
}

static void jb_byte(jbuf_t *b, char c)
{
    jb_reserve(b, 1);
    b->buf[b->len++] = c;
    b->buf[b->len] = '\0';
}

static void jb_str_n(jbuf_t *b, const char *s, size_t n)
{
    jb_reserve(b, n);
    memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = '\0';
}

static void jb_str(jbuf_t *b, const char *s)
{
    jb_str_n(b, s, strlen(s));
}

/* Append a JSON string literal (with quoting) for bytes [s, s+n). NUL
 * becomes \u0000; UTF-8 passes through verbatim. */
static void jb_jsonstr_n(jbuf_t *b, const char *s, size_t n)
{
    size_t i;
    jb_byte(b, '"');
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '"')       jb_str(b, "\\\"");
        else if (c == '\\') jb_str(b, "\\\\");
        else if (c == '\n') jb_str(b, "\\n");
        else if (c == '\r') jb_str(b, "\\r");
        else if (c == '\t') jb_str(b, "\\t");
        else if (c == '\0') jb_str(b, "\\u0000");
        else if (c < 0x20) {
            char hex[8];
            snprintf(hex, sizeof(hex), "\\u%04x", c);
            jb_str(b, hex);
        } else {
            jb_byte(b, (char)c);
        }
    }
    jb_byte(b, '"');
}

/* Convenience: NUL-terminated string with no embedded NUL. */
static void jb_jsonstr(jbuf_t *b, const char *s)
{
    jb_jsonstr_n(b, s, s ? strlen(s) : 0);
}

/* ------------------------------------------------------------------ */
/* Minimal JSON array scanner for [{"func":"..","in":".."}, ...]       */
/* ------------------------------------------------------------------ */

typedef struct { char *func; char *in; } tts_case_t;

static char *json_unescape(const char *start, const char *close)
{
    size_t n = (size_t)(close - start);
    char *out = (char *)malloc(n + 1);
    if (!out) abort();
    size_t w = 0;
    for (size_t i = 0; i < n; i++) {
        char c = start[i];
        if (c == '\\' && i + 1 < n) {
            char e = start[++i];
            switch (e) {
                case '"':  out[w++] = '"';  break;
                case '\\': out[w++] = '\\'; break;
                case '/':  out[w++] = '/';  break;
                case 'n':  out[w++] = '\n'; break;
                case 'r':  out[w++] = '\r'; break;
                case 't':  out[w++] = '\t'; break;
                case 'b':  out[w++] = '\b'; break;
                case 'f':  out[w++] = '\f'; break;
                case 'u': {
                    if (i + 5 < n) {
                        unsigned int cp = 0;
                        sscanf(start + i + 1, "%4x", &cp);
                        i += 4;
                        if (cp < 0x80) out[w++] = (char)cp;
                        else if (cp < 0x800) {
                            out[w++] = (char)(0xC0 | (cp >> 6));
                            out[w++] = (char)(0x80 | (cp & 0x3F));
                        } else {
                            out[w++] = (char)(0xE0 | (cp >> 12));
                            out[w++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                            out[w++] = (char)(0x80 | (cp & 0x3F));
                        }
                    }
                    break;
                }
                default: out[w++] = e; break;
            }
        } else {
            out[w++] = c;
        }
    }
    out[w] = '\0';
    return out;
}

static int find_str_field(const char *obj, const char *objend,
                          const char *key, char **val)
{
    const char *p = obj;
    size_t kl = strlen(key);
    while (p < objend) {
        const char *q = memchr(p, '"', (size_t)(objend - p));
        if (!q) return 1;
        const char *kq = memchr(q + 1, '"', (size_t)(objend - q - 1));
        if (!kq) return 1;
        size_t klen = (size_t)(kq - q - 1);
        if (klen == kl && strncmp(q + 1, key, kl) == 0) {
            const char *v = kq + 1;
            while (v < objend && (*v == ' ' || *v == '\t' || *v == ':')) v++;
            if (v < objend && *v == '"') {
                const char *vs = v + 1;
                const char *ve = memchr(vs, '"', (size_t)(objend - vs));
                if (!ve) return 1;
                /* memchr stops at the first '"', but escaped quotes ("\"")
                 * exist in the fixture text? The fixture uses raw strings with
                 * \n escapes only — no escaped quotes — so the first '"' is the
                 * real close. */
                *val = json_unescape(vs, ve);
                return 0;
            }
            return 1;
        }
        p = kq + 1;
    }
    return 1;
}

static tts_case_t *parse_cases(const char *json, size_t *count)
{
    size_t n = strlen(json);
    const char *p = json;
    const char *end = json + n;

    while (p < end && *p != '[') p++;
    if (p >= end) { *count = 0; return NULL; }
    p++;

    size_t cap = 64;
    tts_case_t *cases = (tts_case_t *)malloc(cap * sizeof(tts_case_t));
    if (!cases) abort();
    size_t nc = 0;

    while (p < end) {
        while (p < end && (*p == ',' || *p == ' ' || *p == '\t' ||
                            *p == '\n' || *p == '\r')) p++;
        if (p >= end || *p == ']') break;
        if (*p != '{') { p++; continue; }
        const char *obj = p;
        const char *ob = memchr(p + 1, '}', end - p - 1);
        if (!ob) break;
        if (nc == cap) {
            cap *= 2;
            cases = (tts_case_t *)realloc(cases, cap * sizeof(tts_case_t));
            if (!cases) abort();
        }
        cases[nc].func = NULL; cases[nc].in = NULL;
        find_str_field(obj, ob, "func", &cases[nc].func);
        find_str_field(obj, ob, "in", &cases[nc].in);
        if (!cases[nc].func) cases[nc].func = strdup("");
        if (!cases[nc].in) cases[nc].in = strdup("");
        nc++;
        p = ob + 1;
    }
    *count = nc;
    return cases;
}

/* ------------------------------------------------------------------ */
/* Dispatch + emit                                                     */
/* ------------------------------------------------------------------ */

static void emit_case(const char *func, const char *in,
                      const char *out, size_t out_len)
{
    jbuf_t b; jb_init(&b);
    jb_str(&b, "{\"func\":");
    jb_jsonstr(&b, func);
    jb_str(&b, ",\"in\":");
    jb_jsonstr(&b, in);
    jb_str(&b, ",\"out\":");
    jb_jsonstr_n(&b, out ? out : "", out ? out_len : 0);
    jb_str(&b, "}");
    printf("%s\n", b.buf);
    free(b.buf);
}

static void run_one(const char *func, const char *in)
{
    char *out = NULL; size_t out_len = 0;

    if (strcmp(func, "strip_markdown_for_tts") == 0)
        out = tts_strip_markdown(in, &out_len);
    else if (strcmp(func, "normalize_symbols_for_tts") == 0)
        out = tts_normalize_symbols(in, &out_len);
    else if (strcmp(func, "smooth_whitespace_for_tts") == 0)
        out = tts_smooth_whitespace(in, &out_len);
    else if (strcmp(func, "strip_nonspoken_blocks") == 0)
        out = tts_strip_nonspoken_blocks(in, &out_len);
    else if (strcmp(func, "flatten_newlines_for_payload") == 0)
        out = tts_flatten_newlines(in, &out_len);
    else if (strcmp(func, "prepare_spoken_text") == 0)
        out = tts_prepare_spoken_text(in, 4000, &out_len);

    emit_case(func ? func : "", in ? in : "", out, out_len);
    free(out);
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc < 2) {
        fprintf(stderr, "usage: %s <cases.in>\n", argv[0]);
        return 2;
    }
    char *json = read_all(argv[1]);
    if (!json) {
        fprintf(stderr, "cannot read %s\n", argv[1]);
        return 2;
    }

    size_t nc;
    tts_case_t *cases = parse_cases(json, &nc);
    free(json);

    for (size_t i = 0; i < nc; i++)
        run_one(cases[i].func, cases[i].in);

    for (size_t i = 0; i < nc; i++) {
        free(cases[i].func);
        free(cases[i].in);
    }
    free(cases);
    return 0;
}
