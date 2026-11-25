#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_voicemixer_6e87f2.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static int hexval(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Decode a hex string to bytes; caller frees. Empty on any malformed input. */
static unsigned char *hex2bin(const char *hex, size_t *out_len) {
    size_t n, i;
    unsigned char *buf;
    *out_len = 0;
    if (!hex) return NULL;
    n = strlen(hex);
    if (n % 2) return NULL;
    buf = (unsigned char *)malloc(n / 2 + 1);
    if (!buf) return NULL;
    for (i = 0; i < n; i += 2) {
        int hi = hexval((unsigned char)hex[i]), lo = hexval((unsigned char)hex[i + 1]);
        if (hi < 0 || lo < 0) { free(buf); return NULL; }
        buf[i / 2] = (unsigned char)(hi * 16 + lo);
    }
    *out_len = n / 2;
    return buf;
}

static double num(json_t *o, const char *k, double def) {
    json_t *v = o ? json_obj_get(o, k) : NULL;
    if (!v || v->type != JSON_NUMBER) return def;
    return v->num_val;
}

static int has_num(json_t *o, const char *k) {
    json_t *v = o ? json_obj_get(o, k) : NULL;
    return v && v->type == JSON_NUMBER;
}

static json_t *run_sequence(json_t *in) {
    w19_voicemixer_6e87f2_mixer_t m;
    json_t *ops = json_obj_get(in, "ops");
    json_t *results = json_array();
    w19_voicemixer_6e87f2_init(&m, num(in, "ambient_gain", 0.18), num(in, "duck_gain", 0.06),
                               num(in, "speech_gain", 1.0),
                               (long)num(in, "duck_release_ms", 400.0));
    for (size_t i = 0; ops && ops->type == JSON_ARRAY && i < json_len(ops); i++) {
        json_t *op = json_get(ops, i);
        const char *kind = op && op->type == JSON_OBJECT ? json_get_str(op, "op", "") : "";
        json_t *r = NULL;
        if (!strcmp(kind, "ambient")) {
            size_t n = 0;
            unsigned char *pcm = hex2bin(json_get_str(op, "hex", ""), &n);
            w19_voicemixer_6e87f2_set_ambient(&m, pcm, pcm ? n : 0, has_num(op, "gain"),
                                              num(op, "gain", 0.0));
            free(pcm);
        } else if (!strcmp(kind, "ambient_clear")) {
            w19_voicemixer_6e87f2_set_ambient(&m, NULL, 0, 0, 0.0);
        } else if (!strcmp(kind, "speech")) {
            size_t n = 0;
            unsigned char *pcm = hex2bin(json_get_str(op, "hex", ""), &n);
            w19_voicemixer_6e87f2_play_speech(&m, pcm, pcm ? n : 0, has_num(op, "gain"),
                                              num(op, "gain", 0.0),
                                              (long)num(op, "fade_in_ms", 40.0));
            free(pcm);
        } else if (!strcmp(kind, "stop_speech")) {
            w19_voicemixer_6e87f2_stop_speech(&m);
        } else if (!strcmp(kind, "read")) {
            r = w19_voicemixer_6e87f2_read(&m);
        } else if (!strcmp(kind, "status")) {
            r = w19_voicemixer_6e87f2_status(&m);
        } else if (!strcmp(kind, "cleanup")) {
            w19_voicemixer_6e87f2_cleanup(&m);
        }
        json_append(results, r ? r : json_null());
    }
    w19_voicemixer_6e87f2_free(&m);
    return results;
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "sequence")) {
            out = run_sequence(in);
        } else if (!strcmp(func, "child_plan")) {
            out = w19_voicemixer_6e87f2_child_plan((size_t)num(in, "pcm_len", 0.0),
                                                   (int)num(in, "loop", 0.0),
                                                   num(in, "gain", 1.0),
                                                   (long)num(in, "fade_in_ms", 0.0),
                                                   (int)num(in, "frames", 1.0));
        } else if (!strcmp(func, "geometry")) {
            out = w19_voicemixer_6e87f2_geometry();
        } else if (!strcmp(func, "whole_cycle_freq")) {
            out = json_number(w19_voicemixer_6e87f2_whole_cycle_freq(num(in, "target", 0.0),
                                                                     num(in, "seconds", 1.0)));
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout);
            emit_key(k, json_object_get_at(in, i));
        }
        fputs("}\n", stdout);
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
