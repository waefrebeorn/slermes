/*
 * t_port_msgraph_error.c — faithful verification harness for
 * msgraph_extract_error() in lib/libmsgraph/ms_graph.c.
 *
 * The fixture (argv[1]) is a sequence of JSON error bodies separated by a
 * line containing only "===". For each body we call msgraph_extract_error()
 * and emit one JSON line: {"body":<json>,"out":<json>} where out is the
 * extracted message. The Python oracle (tests/sta_oracle_msgraph_error.py)
 * recomputes with a reference implementation; the runner diffs them.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Forward declaration — defined in lib/libmsgraph/ms_graph.c. Avoid pulling
 * ms_graph.h (drags libdb/token chain). */
const char *msgraph_extract_error(const char *body);

/* Minimal JSON string escaper for the out field. */
static void emit_json_string(const char *s) {
    putchar('"');
    for (const char *p = s; *p; p++) {
        switch (*p) {
            case '"':  fputs("\\\"", stdout); break;
            case '\\': fputs("\\\\", stdout); break;
            case '\n': fputs("\\n", stdout); break;
            case '\r': fputs("\\r", stdout); break;
            case '\t': fputs("\\t", stdout); break;
            default:   putchar(*p); break;
        }
    }
    putchar('"');
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <bodies.txt>\n", argv[0]); return 2; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return 2; }
    size_t rn = fread(buf, 1, (size_t)sz, f); buf[rn] = '\0';
    fclose(f);

    /* split on lines that are exactly "===" */
    char *cur = buf;
    char *seg_start = buf;
    size_t i = 0;
    while (i <= rn) {
        if (i == rn || buf[i] == '\n') {
            size_t line_len = (size_t)(&buf[i] - cur);
            while (line_len > 0 && cur[line_len-1] == '\r') line_len--;
            int is_sep = (line_len == 3 && strncmp(cur, "===", 3) == 0);
            if (is_sep || i == rn) {
                size_t doc_len = (size_t)(cur - seg_start);
                const char *d = seg_start; const char *e = seg_start + doc_len;
                while (d < e && (*d == '\n' || *d == ' ' || *d == '\t' || *d == '\r')) d++;
                while (e > d && (e[-1] == '\n' || e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r')) e--;
                size_t dlen = (size_t)(e - d);
                if (dlen > 0) {
                    char *body = (char *)malloc(dlen + 1);
                    memcpy(body, d, dlen); body[dlen] = '\0';
                    const char *out = msgraph_extract_error(body);
                    printf("{\"body\":");
                    emit_json_string(body);
                    printf(",\"out\":");
                    emit_json_string(out ? out : "");
                    printf("}\n");
                    free(body);
                }
                seg_start = &buf[i+1];
            }
            if (i == rn) break;
            cur = &buf[i+1];
        }
        i++;
    }
    free(buf);
    return 0;
}
