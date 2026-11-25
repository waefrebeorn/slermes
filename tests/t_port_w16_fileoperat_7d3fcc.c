#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_fileoperat_7d3fcc.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
/* Seed the same deterministic fixture file both sides read. */
static void seed_file(const char *path, const char *hex) {
    if (!hex) { remove(path); return; }
    size_t L = strlen(hex);
    if (L % 2) return;
    FILE *f = fopen(path, "wb");
    if (!f) return;
    for (size_t i = 0; i < L; i += 2) {
        unsigned int b = 0;
        if (sscanf(hex + i, "%2x", &b) == 1) fputc((int)b, f);
    }
    fclose(f);
}
int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *a = json_obj_get(in, "in");
        const char *path = json_get_str(a, "path", "");
        int want = json_get_bool(a, "want_text", false) ? 1 : 0;
        const json_t *sj = json_obj_get(a, "seed_hex");
        seed_file(path, (sj && sj->type == JSON_STRING) ? sj->str_val : NULL);
        json_t *out = NULL;
        if (!strcmp(func, "probe")) out = w16_fileoperat_7d3fcc_probe(path, want);
        else if (!strcmp(func, "sha256_of")) out = w16_fileoperat_7d3fcc_sha256_of(path);
        else out = json_string("UNKNOWN_FUNC");
        printf("{");
        emit_key("func", json_string(func));
        fputs(",", stdout);
        emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout);
            emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
