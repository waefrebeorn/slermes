#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_media_dc1e86.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *str_json(char *owned) {
    json_t *v = owned ? json_string(owned) : json_null();
    free(owned);
    return v;
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        const char *base_url = json_get_str(in, "base_url", "");
        const char *url = json_get_str(in, "url", "");
        json_t *out = NULL;
        if (!strcmp(func, "media_base_url")) {
            out = str_json(w19_media_dc1e86_media_base_url(url));
        } else if (!strcmp(func, "is_relay_media_url")) {
            out = json_bool(w19_media_dc1e86_is_relay_media_url(base_url, url) ? true : false);
        } else if (!strcmp(func, "enabled")) {
            out = json_bool(w19_media_dc1e86_enabled(json_get_str(in, "base_url", NULL),
                                                     json_get_str(in, "gateway_id", NULL),
                                                     json_get_str(in, "secret", NULL)) ? true : false);
        } else if (!strcmp(func, "clean_filename")) {
            out = str_json(w19_media_dc1e86_clean_attachment_filename(json_get_str(in, "name", "")));
        } else if (!strcmp(func, "cd_filename")) {
            out = str_json(w19_media_dc1e86_content_disposition_filename(json_get_str(in, "header", "")));
        } else if (!strcmp(func, "warn_foreign_rehost")) {
            out = json_bool(w19_media_dc1e86_warn_foreign_rehost(base_url, url, json_obj_get(in, "seen")) ? true : false);
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
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
