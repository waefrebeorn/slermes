/*
 * t_port_tts_pure.c — oracle harness for the pure TTS helpers of
 * tools/tts_tool.py ported to C (port_tts_tool.c).
 *
 * Reads a JSON fixture (argv[1]) of cases, runs the C functions, emits one JSON
 * object per case. The Python oracle recomputes from LIVE tools/tts_tool.py.
 *
 * Tested: _resolve_audio_delivery_profile, _split_oversized_sentence.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "hermes_json.h"
#include "libjson/json.h"

typedef struct { char *platform; long max_file_bytes; double safety_ratio; } tts_delivery_profile_t;
extern tts_delivery_profile_t tts_tool_resolve_audio_delivery_profile(const char *platform, const json_t *tts_config);
extern char **tts_tool_split_oversized_sentence(const char *sentence, int max_chars, int *out_count);

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

static char *jesc(const char *s) {
    static char buf[4][8192];
    static int cur = 0;
    char *q = buf[cur];
    cur = (cur + 1) % 4;
    char *base = q;
    *q++ = '"';
    if (s) {
        for (const char *p = s; *p; p++) {
            unsigned char c = (unsigned char)*p;
            if (c == '"' || c == '\\') { *q++ = '\\'; *q++ = c; }
            else if (c == '\n') { *q++ = '\\'; *q++ = 'n'; }
            else if (c == '\t') { *q++ = '\\'; *q++ = 't'; }
            else if (c < 32) { *q++ = '\\'; *q++ = 'u'; *q++ = '0'; *q++ = '0';
                *q++ = (c > 15 ? '1' : '0'); *q++ = (c % 16 < 10 ? c % 16 + '0' : c % 16 - 10 + 'a'); }
            else if ((q - base) < 8170) *q++ = c;
        }
    }
    *q++ = '"'; *q = '\0';
    return base;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: t_port_tts_pure <cases.in>\n"); return 1; }
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

        if (strcmp(op, "resolve_profile") == 0) {
            const char *platform = (arg && arg->type == JSON_STRING) ? arg->str_val : NULL;
            json_t *cfg = json_obj_get(c, "cfg");
            json_t *cfg_json = (cfg && cfg->type == JSON_STRING) ? json_parse(cfg->str_val, NULL) : NULL;
            tts_delivery_profile_t p = tts_tool_resolve_audio_delivery_profile(platform, cfg_json);
            printf("{\"op\":\"resolve_profile\",\"platform\":%s,\"max_file_bytes\":%ld,\"safety_ratio\":%.6g}\n",
                   jesc(p.platform), p.max_file_bytes, p.safety_ratio);
            free(p.platform);
            if (cfg_json) json_free(cfg_json);
        } else if (strcmp(op, "split_sentence") == 0) {
            const char *s = (arg && arg->type == JSON_STRING) ? arg->str_val : "";
            int max = (int)json_get_num(c, "max_chars", 10);
            int n = 0;
            char **chunks = tts_tool_split_oversized_sentence(s, max, &n);
            printf("{\"op\":\"split_sentence\",\"out\":[");
            for (int k = 0; k < n; k++) {
                if (k) printf(",");
                printf("%s", jesc(chunks[k]));
            }
            printf("]}\n");
            for (int k = 0; k < n; k++) free(chunks[k]);
            free(chunks);
        } else {
            printf("{\"op\":\"unknown\"}\n");
        }
    }
    json_free(cases);
    return 0;
}
