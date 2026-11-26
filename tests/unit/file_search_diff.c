/* Harness: run file_search_run on one args JSON and print the result. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>

extern char *file_search_run(const char *args_json);

static char *read_all(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[got] = '\0';
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: file_search_diff <args.json>\n"); return 2; }
    char *args = read_all(argv[1]);
    if (!args) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
    char *out = file_search_run(args);
    free(args);
    if (!out) { printf("{\"error\":\"null\"}\n"); return 0; }
    printf("%s\n", out);
    free(out);
    return 0;
}
