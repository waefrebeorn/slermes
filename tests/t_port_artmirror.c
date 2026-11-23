#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_artmirror.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *source_json(const artmirror_source_t *s) {
    json_t *o = json_object();
    json_set(o, "url", json_string(s->url));
    json_set(o, "dest", json_string(s->dest));
    json_set(o, "sha256", json_string(s->sha256));
    json_t *fb = json_array();
    for (size_t i = 0; i < s->nfallbacks; i++) json_append(fb, json_string(s->fallbacks[i]));
    json_set(o, "fallbacks", fb);
    json_set(o, "http_mirror", json_string(s->http_mirror));
    return o;
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        const char *sha = json_get_str(in, "sha256", "");
        json_t *out = NULL;

        if (!strcmp(func, "object_key")) {
            char *k = NULL;
            out = artmirror_object_key(sha, &k) ? json_string(k) : json_null();
            free(k);
        } else if (!strcmp(func, "mirror_url")) {
            char *u = NULL;
            out = artmirror_mirror_url(sha, &u) ? json_string(u) : json_null();
            free(u);
        } else if (!strcmp(func, "pinned")) {
            artmirror_source_t s;
            if (artmirror_pinned_source(sha,
                                        json_get_str(in, "dest", ""),
                                        json_get_str(in, "url", ""),
                                        json_get_str(in, "mirrored", ""),
                                        json_get_str(in, "registry", ""), &s)) {
                out = source_json(&s);
                artmirror_source_free(&s);
            } else {
                out = json_null();
            }
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
            emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
