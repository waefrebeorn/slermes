/* Differential harness for the ported fill-script builder. */
#include "hermes_json.h"

#include <stdio.h>
#include <stdlib.h>

extern char *vl_build_fill_js(const char *fills_json, const char *expected_origin, const char *nonce);

static char *read_all(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *b = malloc((size_t)n + 1);
    if (!b) { fclose(f); return NULL; }
    size_t got = fread(b, 1, (size_t)n, f);
    fclose(f);
    b[got] = '\0';
    return b;
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <cases.json> <out.json>\n", argv[0]); return 2; }
    char *text = read_all(argv[1]);
    if (!text) return 2;
    json_t *cases = json_parse(text, NULL);
    free(text);
    if (!cases || cases->type != JSON_ARRAY) return 2;

    json_t *out = json_array();
    for (size_t i = 0; i < json_len(cases); i++) {
        const json_t *c = json_get(cases, i);
        char *fills = json_serialize(json_obj_get(c, "fills"));
        char *js = vl_build_fill_js(fills ? fills : "[]",
                                    json_get_str(c, "origin", ""),
                                    json_get_str(c, "nonce", ""));
        json_t *entry = json_object();
        json_set(entry, "name", json_string(json_get_str(c, "name", "")));
        json_set(entry, "js", js ? json_string(js) : json_null());
        json_append(out, entry);
        free(js);
        free(fills);
    }
    char *ser = json_serialize(out);
    json_free(out);
    json_free(cases);
    if (!ser) return 2;
    FILE *f = fopen(argv[2], "wb");
    if (!f) { free(ser); return 2; }
    fputs(ser, f);
    fclose(f);
    free(ser);
    return 0;
}
