/*
 * t_port_media_policy.c — parity harness for src/gateway/media_policy.c
 * (gateway/media_policy.py). Fixture ops:
 *   {"kind":"apply",
 *    "config": {...} | null,
 *    "preset": {"HERMES_MEDIA_...": "value", ...}}
 *
 * Each op clears the three policy env vars, applies `preset`, runs
 * media_policy_apply_env(config), then reports the three env values
 * (null when unset). The Python twin does exactly the same.
 */
#include "hermes_json.h"
#include "media_policy.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *ENVS[3] = {
    "HERMES_MEDIA_DELIVERY_STRICT",
    "HERMES_MEDIA_ALLOW_DIRS",
    "HERMES_MEDIA_TRUST_RECENT_FILES",
};

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot read %s\n", path); exit(1); }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc(n + 1);
    size_t r = fread(buf, 1, n, f); buf[r] = '\0';
    fclose(f);
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture>\n", argv[0]); return 1; }
    char *rawf = read_file(argv[1]);
    json_t *root = json_parse(rawf, NULL);
    if (!root) { fprintf(stderr, "bad fixture\n"); free(rawf); return 1; }
    json_t *ops = json_object_get(root, "ops");
    if (ops && ops->type == JSON_ARRAY) {
        for (size_t i = 0; i < json_len(ops); i++) {
            json_t *c = json_get(ops, i);
            /* reset the three env vars */
            for (int e = 0; e < 3; e++) unsetenv(ENVS[e]);
            /* apply presets */
            json_t *preset = json_object_get(c, "preset");
            if (preset && preset->type == JSON_OBJECT) {
                for (size_t p = 0; p < json_len(preset); p++) {
                    json_t *kv = json_get(preset, p);
                    /* iterate object members */
                }
                /* libjson object iteration fallback: use known keys only */
                for (int e = 0; e < 3; e++) {
                    json_t *v = json_object_get(preset, ENVS[e]);
                    if (v && v->type == JSON_STRING) setenv(ENVS[e], v->str_val, 1);
                }
            }
            json_t *cfg = json_object_get(c, "config");
            media_policy_apply_env(cfg && cfg->type == JSON_OBJECT ? cfg : NULL);
            json_t *o = json_new_object();
            json_t *out = json_new_object();
            for (int e = 0; e < 3; e++) {
                const char *v = getenv(ENVS[e]);
                json_object_set(out, ENVS[e],
                                v ? json_new_string(v) : json_new_null());
            }
            json_object_set(o, "out", out);
            char *line = json_dumps(o, 0);
            printf("%s\n", line);
            free(line);
            json_free(o);
        }
    }
    json_free(root);
    free(rawf);
    return 0;
}
