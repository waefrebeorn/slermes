/* Oracle harness: tools/patch_parser.parse_v4a_patch vs LIVE Python.
 * For each argv entry (a path to a patch file), reads the file contents,
 * runs patch_parser_parse_v4a, and prints the canonical JSON. The oracle
 * (sta_oracle_patch_parser.py) produces the same canonical form from LIVE
 * Python; diff the two to verify fidelity. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tools/port_patch_parser.h"

static char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return NULL; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)sz + 1);
    size_t got = fread(buf, 1, (size_t)sz, f);
    buf[got] = '\0';
    fclose(f);
    return buf;
}

int main(int argc, char **argv)
{
    if (argc > 1) {
        for (int i = 1; i < argc; i++) {
            char *patch = read_file(argv[i]);
            if (!patch) continue;
            patch_parse_result_t r = patch_parser_parse_v4a(patch);
            patch_parser_print_canonical(&r);
            patch_parser_result_free(&r);
            free(patch);
        }
        return 0;
    }
    /* stdin: read whole file as one patch */
    char *buf = NULL; size_t cap = 0, len = 0;
    size_t n; char tmp[4096];
    while ((n = fread(tmp, 1, sizeof(tmp), stdin)) > 0) {
        if (len + n + 1 > cap) { cap = (len + n + 1) * 2; buf = realloc(buf, cap); }
        memcpy(buf + len, tmp, n); len += n;
    }
    if (!buf) buf = malloc(1);
    buf[len] = '\0';
    patch_parse_result_t r = patch_parser_parse_v4a(buf);
    patch_parser_print_canonical(&r);
    patch_parser_result_free(&r);
    free(buf);
    return 0;
}
