/*
 * t_port_file_ops_pure.c — oracle harness for the PURE, deterministic helpers
 * ported from tools/file_operations.py across:
 *   file_text_ops.c   (strip_terminal_fence_leaks, detect_line_ending,
 *                      normalize_line_endings, strip_bom, has_bom)
 *   file_pagination_ops.c (coerce_int, pattern_has_regex_newline,
 *                      normalize_read_pagination, normalize_search_pagination)
 *   file_lint.c       (_lint_json/yaml/toml/python_inproc)
 *
 * Network/IO-free. Exercises the REAL C functions and emits stable JSON
 * (one object per line) that the Python oracle reproduces.
 */

#include "tools/file_text_ops.h"
#include "tools/file_pagination_ops.h"
#include "tools/file_lint.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void emit_json_string(const char *s) {
    if (!s) { printf("null"); return; }
    putchar('"');
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        switch (c) {
            case '"':  printf("\\\""); break;
            case '\\': printf("\\\\"); break;
            case '\n': printf("\\n"); break;
            case '\t': printf("\\t"); break;
            case '\r': printf("\\r"); break;
            default:
                if (c < 0x20) printf("\\u%04x", c);
                else putchar((int)c);
        }
    }
    putchar('"');
}

/* Decode fixture tokens into real characters:
 *   @LF@ -> \n   @CRLF@ -> \r\n   @BSN@ -> backslash+n (regex \n)
 *   @BSBSN@ -> backslash+backslash+n   @BOM@ -> U+FEFF */
static char *decode_tokens(const char *in) {
    if (!in) return NULL;
    size_t n = strlen(in);
    char *out = malloc(n + 1);
    if (!out) return NULL;
    size_t oi = 0;
    for (size_t i = 0; i < n; ) {
        if (strncmp(in + i, "@LF@", 4) == 0) { out[oi++] = '\n'; i += 4; }
        else if (strncmp(in + i, "@CRLF@", 6) == 0) { out[oi++] = '\r'; out[oi++] = '\n'; i += 6; }
        else if (strncmp(in + i, "@BSBSN@", 7) == 0) { out[oi++] = '\\'; out[oi++] = '\\'; out[oi++] = 'n'; i += 7; }
        else if (strncmp(in + i, "@BSN@", 5) == 0) { out[oi++] = '\\'; out[oi++] = 'n'; i += 5; }
        else if (strncmp(in + i, "@BOM@", 5) == 0) {
            /* U+FEFF UTF-8: EF BB BF */
            out[oi++] = (char)0xEF; out[oi++] = (char)0xBB; out[oi++] = (char)0xBF; i += 5;
        } else {
            out[oi++] = in[i++];
        }
    }
    out[oi] = '\0';
    return out;
}

/* parse "key value" with value possibly empty -> rest points after first space */
static void split_kv(const char *line, char *key, size_t ksz, const char **val) {
    size_t i = 0;
    while (*line && *line != ' ' && i + 1 < ksz) key[i++] = *line++;
    key[i] = '\0';
    if (*line == ' ') line++;
    *val = line;
}

int main(void) {
    char line[32768];
    file_lint_t *lint = file_lint_init("python3");

    while (fgets(line, sizeof(line), stdin)) {
        size_t L = strlen(line);
        while (L > 0 && (line[L-1] == '\n' || line[L-1] == '\r')) line[--L] = '\0';
        if (!*line || line[0] == '#') continue;

        char op[40];
        const char *rest;
        split_kv(line, op, sizeof(op), &rest);
        char *dv = decode_tokens(rest[0] ? rest : "");   /* decoded value */
        const char *v = dv ? dv : "";

        if (strcmp(op, "fence") == 0) {
            char *out = file_text_ops_strip_terminal_fence_leaks(v);
            printf("{\"op\":\"fence\",\"in\":");
            emit_json_string(v); printf(",\"out\":");
            emit_json_string(out ? out : ""); printf("}\n");
            free(out);

        } else if (strcmp(op, "detect_le") == 0) {
            char *out = file_text_ops_detect_line_ending(v);
            printf("{\"op\":\"detect_le\",\"in\":");
            emit_json_string(v); printf(",\"out\":");
            emit_json_string(out ? out : "");
            printf("}\n");
            free(out);

        } else if (strcmp(op, "norm_le") == 0) {
            /* norm_le <target>|<text> ; target uses @LF@/@CRLF@ tokens */
            const char *bar = strchr(v, '|');
            char *target = decode_tokens(bar ? "x" : "");  /* placeholder */
            char *text = NULL;
            if (bar) {
                /* target = v up to bar (decode it), text = after bar */
                size_t tl = (size_t)(bar - v);
                char *tbuf = malloc(tl + 1);
                memcpy(tbuf, v, tl); tbuf[tl] = '\0';
                free(target);
                target = decode_tokens(tbuf);
                free(tbuf);
                text = decode_tokens(bar + 1);
            } else {
                target = decode_tokens("@LF@");
                text = decode_tokens("");
            }
            char *out = file_text_ops_normalize_line_endings(text ? text : "", target ? target : "\n");
            printf("{\"op\":\"norm_le\",\"target\":");
            emit_json_string(target ? target : "\n"); printf(",\"text\":");
            emit_json_string(text ? text : ""); printf(",\"out\":");
            emit_json_string(out ? out : "");
            printf("}\n");
            free(out); free(target); free(text);

        } else if (strcmp(op, "strip_bom") == 0) {
            char *out = file_text_ops_strip_bom(v);
            bool had = file_text_ops_has_bom(v);
            printf("{\"op\":\"strip_bom\",\"in\":");
            emit_json_string(v); printf(",\"out\":");
            emit_json_string(out ? out : "");
            printf(",\"had_bom\":%s}\n", had ? "true" : "false");
            free(out);

        } else if (strcmp(op, "pat_newline") == 0) {
            bool r = file_pagination_ops_pattern_has_regex_newline(v);
            printf("{\"op\":\"pat_newline\",\"pattern\":");
            emit_json_string(v); printf(",\"has\":%s}\n", r ? "true" : "false");

        } else if (strcmp(op, "norm_read") == 0) {
            /* norm_read <default_limit>|<offset>|<limit> */
            int dl = 2000, off = 1, lim = 500;
            sscanf(rest, "%d|%d|%d", &dl, &off, &lim);
            char *out = file_pagination_ops_normalize_read_pagination(off, lim, dl);
            printf("{\"op\":\"norm_read\",\"default_limit\":%d,\"offset\":%d,\"limit\":%d,\"out\":", dl, off, lim);
            emit_json_string(out ? out : "");
            printf("}\n");
            free(out);

        } else if (strcmp(op, "norm_search") == 0) {
            int dl = 50, off = 0, lim = 50;
            sscanf(rest, "%d|%d|%d", &dl, &off, &lim);
            char *out = file_pagination_ops_normalize_search_pagination(off, lim, dl);
            printf("{\"op\":\"norm_search\",\"default_limit\":%d,\"offset\":%d,\"limit\":%d,\"out\":", dl, off, lim);
            emit_json_string(out ? out : "");
            printf("}\n");
            free(out);

        } else if (strcmp(op, "lint") == 0) {
            /* lint <kind>|<content> ; kind in json|yaml|toml|python */
            const char *bar = strchr(v, '|');
            const char *kind = v;
            char *content = NULL;
            if (bar) {
                size_t kl = (size_t)(bar - v);
                char *kbuf = malloc(kl + 1);
                memcpy(kbuf, v, kl); kbuf[kl] = '\0';
                kind = kbuf;
                content = decode_tokens(bar + 1);
                free(kbuf);
            } else {
                content = decode_tokens("");
            }
            char *out = NULL;
            if (strcmp(kind, "json") == 0) out = file_lint_json(content);
            else if (strcmp(kind, "yaml") == 0) out = file_lint_yaml(content);
            else if (strcmp(kind, "toml") == 0) out = file_lint_toml(content);
            else if (strcmp(kind, "python") == 0) out = file_lint_python(lint, content);
            printf("{\"op\":\"lint\",\"kind\":");
            emit_json_string(kind); printf(",\"content\":");
            emit_json_string(content ? content : ""); printf(",\"result\":");
            emit_json_string(out ? out : "");
            printf("}\n");
            free(content); free(out);

        } else {
            printf("{\"op\":\"unknown\",\"raw\":");
            emit_json_string(line);
            printf("}\n");
        }
        free(dv);
    }
    if (lint) file_lint_free(lint);
    return 0;
}
