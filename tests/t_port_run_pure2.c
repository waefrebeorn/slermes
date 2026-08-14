/*
 * t_port_run_pure2.c — oracle harness for the pure helpers of gateway/run.py
 * ported in run_pure2.c.
 *
 * Reads a JSON fixture (argv[1]) of cases, runs the C functions, emits one JSON
 * object per case. The Python oracle recomputes from LIVE gateway/run.py.
 *
 * Tested: hygiene_compaction_recovered, compression_made_progress.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "hermes_json.h"
#include "libjson/json.h"
#include "gateway_run_pure2.h"

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, sz, f);
    buf[rd] = '\0';
    fclose(f);
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: t_port_run_pure2 <cases.in>\n"); return 1; }
    char *text = read_file(argv[1]);
    if (!text) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }
    char *err = NULL;
    json_t *cases = json_parse(text, &err);
    free(text);
    if (err || !cases || cases->type != JSON_ARRAY) { if (err) free(err); fprintf(stderr, "bad fixture\n"); return 1; }

    for (size_t i = 0; i < cases->c.count; i++) {
        json_t *c = cases->c.items[i];
        const char *op = json_get_str(c, "op", "");

        if (strcmp(op, "hygiene_compaction_recovered") == 0) {
            bool aborted = json_get_bool(c, "aborted", false);
            bool rotated = json_get_bool(c, "rotated", false);
            bool in_place = json_get_bool(c, "in_place", false);
            long mc = (long)json_get_num(c, "msg_count", 100);
            long nc = (long)json_get_num(c, "new_count", 50);
            long at = (long)json_get_num(c, "approx_tokens", 1000);
            long nt = (long)json_get_num(c, "new_tokens", 600);
            bool r = gateway_run_pure2_hygiene_compaction_recovered(
                aborted, rotated, in_place, mc, nc, at, nt);
            printf("{\"op\":\"hygiene_compaction_recovered\",\"out\":%s}\n", r ? "true" : "false");
        } else if (strcmp(op, "compression_made_progress") == 0) {
            int ol = (int)json_get_num(c, "orig_len", 0);
            int nl = (int)json_get_num(c, "new_len", 0);
            int ot = (int)json_get_num(c, "orig_tokens", 0);
            int nt = (int)json_get_num(c, "new_tokens", 0);
            bool r = gw_compression_made_progress(ol, nl, ot, nt);
            printf("{\"op\":\"compression_made_progress\",\"out\":%s}\n", r ? "true" : "false");
        } else {
            printf("{\"op\":\"unknown\"}\n");
        }
    }
    json_free(cases);
    return 0;
}
