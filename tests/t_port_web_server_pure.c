/*
 * t_port_web_server_pure.c — Oracle harness for the pure-helpers ported in
 * port_web_server_schema_path.c (and the corresponding hermes_web_server_pure.h
 * API). Reads fixture from argv[1], emits one JSON line per case that the
 * Python oracle (tests/sta_oracle_web_server_pure.py) re-computes and diffs.
 *
 * Conventions (run_oracle.sh):
 *   - argv[1]  = fixture path
 *   - one row per line; '|' separates fields; '#'/blank lines are skipped
 *   - output is one JSON envelope per case, exactly the same shape the Python
 *     oracle produces, so `diff -q` is a clean pass/fail signal.
 */
#include "hermes_web_server_pure.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* ── output helpers ───────────────────────────────────────────────────────
 * The runner does `diff -q` on the two side-by-side files, so we emit
 * single-line JSON envelopes with consistent key order. */

/* escape a JSON string value per RFC8259; writes to `out` and returns the
 * new write pointer. */
static char *json_str(char *w, const char *end, const char *s) {
    if (!s) s = "";
    if (w >= end) return w;
    *w++ = '"';
    for (; *s && w + 7 < end; s++) {
        unsigned char c = (unsigned char)*s;
        switch (c) {
            case '"':  *w++ = '\\'; *w++ = '"';  break;
            case '\\': *w++ = '\\'; *w++ = '\\'; break;
            case '\n': *w++ = '\\'; *w++ = 'n';  break;
            case '\r': *w++ = '\\'; *w++ = 'r';  break;
            case '\t': *w++ = '\\'; *w++ = 't';  break;
            case '\b': *w++ = '\\'; *w++ = 'b';  break;
            case '\f': *w++ = '\\'; *w++ = 'f';  break;
            default:
                if (c < 0x20) {
                    int n = snprintf(w, (size_t)(end - w), "\\u%04x", c);
                    if (n > 0) w += n;
                } else {
                    *w++ = (char)c;
                }
        }
    }
    if (w < end) *w++ = '"';
    return w;
}

/* hex-decode a string into a malloc'd buffer; malloc only if hex_len > 0 */
static unsigned char *hex_decode(const char *hex, size_t *out_len) {
    size_t hl = strlen(hex);
    if (hl == 0) {
        if (out_len) *out_len = 0;
        return NULL;
    }
    if (hl % 2 != 0) { if (out_len) *out_len = 0; return NULL; }
    size_t n = hl / 2;
    unsigned char *buf = malloc(n + 1);
    if (!buf) { if (out_len) *out_len = 0; return NULL; }
    for (size_t i = 0; i < n; i++) {
        int hi = -1, lo = -1;
        char a = hex[2*i], b = hex[2*i + 1];
        if      (a >= '0' && a <= '9') hi = a - '0';
        else if (a >= 'a' && a <= 'f') hi = a - 'a' + 10;
        else if (a >= 'A' && a <= 'F') hi = a - 'A' + 10;
        if      (b >= '0' && b <= '9') lo = b - '0';
        else if (b >= 'a' && b <= 'f') lo = b - 'a' + 10;
        else if (b >= 'A' && b <= 'F') lo = b - 'A' + 10;
        if (hi < 0 || lo < 0) { free(buf); if (out_len) *out_len = 0; return NULL; }
        buf[i] = (unsigned char)((hi << 4) | lo);
    }
    buf[n] = '\0';
    if (out_len) *out_len = n;
    return buf;
}

static char *emit_line(char *w, const char *end,
                       const char *op, const char *arg,
                       const char *result) {
    if (w >= end) return w;
    *w++ = '{';
    w += snprintf(w, (size_t)(end - w), "\"op\":\"%s\",\"arg\":", op);
    w = json_str(w, end, arg);
    if (w + 12 < end) {
        *w++ = ',';
        *w++ = '"';
        const char *rk = "result";
        for (const char *p = rk; *p && w < end; p++) *w++ = *p;
        *w++ = '"';
        *w++ = ':';
        w = json_str(w, end, result);
    }
    if (w < end) *w++ = '}';
    if (w < end) *w++ = '\n';
    return w;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <fixture>\n", argv[0]);
        return 2;
    }
    FILE *f = fopen(argv[1], "r");
    if (!f) { perror("fopen"); return 2; }

    char line[8192];
    /* The runner redirects stdout to a file, so a single buffered write
     * is fine. */
    char out_buf[65536];
    char *w = out_buf;
    char *end = out_buf + sizeof out_buf;

    while (fgets(line, sizeof line, f)) {
        /* trim leading whitespace */
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (!*p || *p == '#') continue;
        /* strip trailing newline */
        size_t L = strlen(p);
        while (L > 0 && (p[L-1] == '\n' || p[L-1] == '\r')) L--;
        p[L] = '\0';

        /* parse op */
        char *op = strtok(p, "|");
        char *a  = strtok(NULL, "|");
        if (!op) continue;
        if (!a) a = (char *)"";

        if (strcmp(op, "infer_type_bool") == 0) {
            bool v = (strcmp(a, "true") == 0);
            const char *r = ws_infer_type_bool(v);
            w = emit_line(w, end, op, a, r);
        }
        else if (strcmp(op, "infer_type_int") == 0) {
            long v = strtol(a, NULL, 10);
            const char *r = ws_infer_type_int(v);
            w = emit_line(w, end, op, a, r);
        }
        else if (strcmp(op, "infer_type_f64") == 0) {
            double v = strtod(a, NULL);
            const char *r = ws_infer_type_f64(v);
            w = emit_line(w, end, op, a, r);
        }
        else if (strcmp(op, "infer_type_str") == 0) {
            const char *r = ws_infer_type_str(a);
            w = emit_line(w, end, op, a, r);
        }
        else if (strcmp(op, "path_text") == 0) {
            char out[1024];
            /* Unescape literal \0, \n, \t, \r from fixture to test the byte path.
             * Python strings can carry embedded NUL bytes — to faithfully
             * mirror that, we pass the true byte length to ws_path_text_n.
             * (ws_path_text is a strlen-based wrapper that cannot see NUL.) */
            char buf[2048]; size_t bn = 0;
            for (const char *s = a; *s && bn < sizeof buf - 1; ) {
                if (s[0] == '\\' && s[1] == '0') { buf[bn++] = '\0'; s += 2; }
                else if (s[0] == '\\' && s[1] == 'n') { buf[bn++] = '\n'; s += 2; }
                else if (s[0] == '\\' && s[1] == 't') { buf[bn++] = '\t'; s += 2; }
                else if (s[0] == '\\' && s[1] == 'r') { buf[bn++] = '\r'; s += 2; }
                else { buf[bn++] = *s++; }
            }
            buf[bn] = '\0';
            ws_path_status_t s = ws_path_text_n(buf, bn, out, sizeof out);
            char res[1100];
            /* Strip embedded NUL from the printed result (it's invisible to diff
             * anyway) — Python inspecting the str would show nothing. */
            size_t rlen = strnlen(out, sizeof out);
            char quoted[1100]; char *q = quoted; const char *qe = quoted + sizeof quoted - 4;
            for (size_t i = 0; i < rlen && q < qe; i++) {
                if ((unsigned char)out[i] < 0x20) { int n = snprintf(q, (size_t)(qe - q), "\\u%04x", (unsigned char)out[i]); if (n > 0) q += n; }
                else *q++ = out[i];
            }
            *q = '\0';
            snprintf(res, sizeof res, "%s|%s", ws_path_status_str(s), quoted);
            w = emit_line(w, end, op, a, res);
        }
        else if (strcmp(op, "path_is_under") == 0) {
            char *b = strtok(NULL, "|");
            if (!b) continue;
            bool ok = ws_path_is_under(a, b);
            w = emit_line(w, end, op, a, ok ? "true" : "false");
        }
        else if (strcmp(op, "fs_mime") == 0) {
            const char *r = ws_fs_mime_type(a);
            w = emit_line(w, end, op, a, r);
        }
        else if (strcmp(op, "audio_mime") == 0) {
            const char *r = ws_audio_extension_for_mime(a);
            w = emit_line(w, end, op, a, r);
        }
        else if (strcmp(op, "looks_binary") == 0) {
            size_t bl = 0;
            unsigned char *buf = hex_decode(a, &bl);
            bool r = ws_fs_looks_binary(buf, bl);
            free(buf);
            w = emit_line(w, end, op, a, r ? "true" : "false");
        }
        else {
            /* unknown op — skip */
        }
    }

    fflush(stdout);
    if (w > out_buf) fwrite(out_buf, 1, (size_t)(w - out_buf), stdout);
    fclose(f);
    return 0;
}
