/* Timing probe: is the cost in json_parse or in json_dumps?
 * Build: see scripts/bench_json_probe.sh. Not part of the product build. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "hermes_json.h"

static double now_s(void) {
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : NULL;
    FILE *f;
    long size;
    char *buf;
    double t0, t1, t2;
    json_t *node;
    char *err = NULL;
    char *out;

    if (!path) { fprintf(stderr, "usage: %s <json-file>\n", argv[0]); return 2; }
    f = fopen(path, "rb");
    if (!f) { perror(path); return 2; }
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = (char *)malloc((size_t)size + 1);
    if (!buf) { fclose(f); return 2; }
    if (fread(buf, 1, (size_t)size, f) != (size_t)size) { fclose(f); free(buf); return 2; }
    buf[size] = '\0';
    fclose(f);

    t0 = now_s();
    node = json_parse(buf, &err);
    t1 = now_s();
    if (!node) { fprintf(stderr, "parse error: %s\n", err ? err : "?"); free(err); free(buf); return 1; }
    out = json_dumps(node, 0);
    t2 = now_s();
    {
        double t3, t4;
        json_t *cp;
        t3 = now_s();
        cp = json_copy(node);
        t4 = now_s();
        printf("in=%ld out=%zu parse=%.3fs dump=%.3fs copy=%.3fs\n",
               size, out ? strlen(out) : 0, t1 - t0, t2 - t1, t4 - t3);
        json_free(cp);
    }
    free(out);
    json_free(node);
    free(buf);
    return 0;
}
