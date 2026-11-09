/*
 * t_slv_ui_demo.c -- render games/ui_demo.lua through the Lua-driven terminal
 * host and print the resulting screen.
 *
 * This is the "does it actually draw something" proof for the curses
 * replacement: the script is plain Lua, the host owns the buffer, and the
 * C side only reads the file and dumps what Lua produced.
 *
 * Not part of the pass/fail gate - it exists so a human can look at the
 * rendered frame. Run from the repo root.
 */
#include <stdio.h>
#include <stdlib.h>

#include "slv_ui.h"

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
    const char *script = (argc > 1) ? argv[1] : "games/ui_demo.lua";
    int w = (argc > 2) ? atoi(argv[2]) : 60;
    int h = (argc > 3) ? atoi(argv[3]) : 16;

    char *src = read_file(script);
    if (!src) {
        fprintf(stderr, "cannot read %s (run from the repo root)\n", script);
        return 2;
    }

    slv_ui *ui = slv_ui_new(w, h);
    if (!ui) { fprintf(stderr, "slv_ui_new(%d,%d) failed\n", w, h); free(src); return 2; }

    const char *err = slv_ui_run(ui, src, script);
    if (err) {
        fprintf(stderr, "lua error: %s\n", err);
        free(src);
        slv_ui_free(ui);
        return 1;
    }

    printf("+%s+\n", "------------------------------------------------------------");
    slv_ui_dump(ui, stdout);
    printf("+%s+\n", "------------------------------------------------------------");
    printf("rendered %dx%d from %s\n", slv_ui_width(ui), slv_ui_height(ui), script);

    free(src);
    slv_ui_free(ui);
    return 0;
}
