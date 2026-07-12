/* Oracle harness: tools/schema_sanitizer.strip_pattern_and_format /
 * strip_slash_enum vs LIVE Python.
 * Usage: ./t_port_schema_sanitizer <mode:pf|se> <tools.json>
 * Prints "COUNT=<n>\n<serialized tools json>". The oracle emits the same
 * from LIVE Python; diff for fidelity. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hermes_json.h"
#include "cli/port_tools_schema_sanitizer.h"

static char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return NULL; }
    fseek(f, 0, SEEK_END); long sz = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)sz + 1);
    size_t got = fread(buf, 1, (size_t)sz, f);
    buf[got] = '\0'; fclose(f);
    return buf;
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: %s <tools.json> <pf|se>\n", argv[0]); return 2; }
    char *in = read_file(argv[1]);
    if (!in) return 2;
    int stripped = 0;
    char *out = NULL;
    if (strcmp(argv[2], "pf") == 0)
        out = cli_tools_schema_sanitizer__strip_pattern_and_format(in, &stripped);
    else if (strcmp(argv[2], "se") == 0)
        out = cli_tools_schema_sanitizer__strip_slash_enum(in, &stripped);
    else { fprintf(stderr, "unknown mode %s\n", argv[2]); free(in); return 2; }
    printf("COUNT=%d\n%s\n", stripped, out ? out : "[]");
    free(out); free(in);
    return 0;
}
