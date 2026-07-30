/*
 * t_port_fuzzy_utils.c — faithful verification harness for
 * count_lines() and trim_right() in lib/libfuzzymatch/fuzzy_match.c.
 *
 * The fixture (argv[1]) is one case per line: "<op>\t<input>" where op is
 * "lines" or "trim". For "lines" we emit the line count; for "trim" we emit
 * the trailing-whitespace-stripped result. The Python oracle
 * (tests/sta_oracle_fuzzy_utils.py) recomputes with a reference
 * implementation; the runner diffs them.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Forward declarations — defined in lib/libfuzzymatch/fuzzy_match.c. */
int count_lines(const char *s);
char *trim_right(const char *s);

static void emit_json_string(const char *s) {
    putchar('"');
    for (const char *p = s; *p; p++) {
        if (*p == '"') fputs("\\\"", stdout);
        else if (*p == '\\') fputs("\\\\", stdout);
        else if (*p == '\n') fputs("\\n", stdout);
        else if (*p == '\r') fputs("\\r", stdout);
        else if (*p == '\t') fputs("\\t", stdout);
        else putchar(*p);
    }
    putchar('"');
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "usage: %s <cases.tsv>\n", argv[0]); return 2; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }

    char line[8192];
    while (fgets(line, sizeof(line), f)) {
        size_t n = strlen(line);
        while (n > 0 && (line[n-1] == '\n' || line[n-1] == '\r'))
            line[--n] = '\0';
        if (n == 0) continue;

        char *op = line;
        char *input = strchr(op, '\t');
        if (!input) continue;
        *input++ = '\0';

        if (strcmp(op, "lines") == 0) {
            printf("{\"op\":\"lines\",\"out\":%d}\n", count_lines(input));
        } else if (strcmp(op, "trim") == 0) {
            char *t = trim_right(input);
            printf("{\"op\":\"trim\",\"out\":");
            emit_json_string(t ? t : "");
            printf("}\n");
            free(t);
        }
    }
    fclose(f);
    return 0;
}
