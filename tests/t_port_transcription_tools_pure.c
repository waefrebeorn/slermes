/*
 * t_port_transcription_tools_pure.c — oracle harness for the config-parse
 * helpers of tools/transcription_tools.py ported to C
 * (transcription_tools_pure.c / transcription_tools_pure.h).
 *
 * Reads a JSON fixture (argv[1]) of cases, runs the C functions, and emits one
 * JSON object per case. The Python oracle (sta_oracle_transcription_tools_pure.py)
 * recomputes the same functions from LIVE tools/transcription_tools.py and the
 * runner exact-compares.
 *
 * Tested: ts_get_idle_unload_seconds, ts_cloud_trim_settings.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "hermes_json.h"
#include "libjson/json.h"
#include "transcription_tools_pure.h"

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
    if (argc < 2) { fprintf(stderr, "usage: t_port_transcription_tools_pure <cases.in>\n"); return 1; }
    char *text = read_file(argv[1]);
    if (!text) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }
    char *err = NULL;
    json_t *cases = json_parse(text, &err);
    free(text);
    if (err || !cases || cases->type != JSON_ARRAY) { if (err) free(err); fprintf(stderr, "bad fixture\n"); return 1; }

    for (size_t i = 0; i < cases->c.count; i++) {
        json_t *c = cases->c.items[i];
        const char *op = json_get_str(c, "op", "");
        json_t *arg = json_obj_get(c, "arg");
        const char *s = (arg && arg->type == JSON_STRING) ? arg->str_val : "";

        if (strcmp(op, "idle_unload") == 0) {
            int out = ts_get_idle_unload_seconds(s);
            printf("{\"op\":\"idle_unload\",\"out\":%d}\n", out);
        } else if (strcmp(op, "cloud_trim") == 0) {
            bool enabled = true; int tdb = -40, keep = 300;
            ts_cloud_trim_settings(s, &enabled, &tdb, &keep);
            printf("{\"op\":\"cloud_trim\",\"enabled\":%s,\"threshold_db\":%d,\"keep_ms\":%d}\n",
                   enabled ? "true" : "false", tdb, keep);
        } else {
            printf("{\"op\":\"unknown\"}\n");
        }
    }
    json_free(cases);
    return 0;
}
