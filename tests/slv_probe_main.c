/*
 * slv_probe_main.c -- minimal host for tests/probe_lua_gaps*.py.
 *
 * Unlike t_slv_ui_demo.c this does NOT dump the cell buffer: it just opens the
 * libraries, runs the chunk, and lets the chunk's own print() output stand
 * alone. That keeps probe results readable (one line per probe).
 *
 * Usage: slv_probe <script.lua>
 */
#include <stdio.h>
#include <stdlib.h>

#include "slv.h"

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long n = ftell(f);
    if (n < 0) { fclose(f); return NULL; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[got] = '\0';
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <script.lua>\n", argv[0]);
        return 2;
    }
    char *src = read_file(argv[1]);
    if (!src) {
        fprintf(stderr, "cannot read %s\n", argv[1]);
        return 2;
    }

    slv *L = slv_new();
    if (!L) { fprintf(stderr, "slv_new failed\n"); free(src); return 2; }

    const char *err = slv_run(L, src, argv[1]);
    free(src);
    slv_free(L);

    if (err) {
        fprintf(stderr, "%s\n", err);
        return 1;
    }
    return 0;
}
