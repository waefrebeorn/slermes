#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_downloader_69e11a.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
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
        if (!strcmp(func, "pinned")) {
            out = w5_downloader_69e11a_pinned(json_get_str(in, "sha256", ""));
        } else if (!strcmp(func, "host")) {
            out = w5_downloader_69e11a_host(json_get_str(in, "url", ""));
        } else if (!strcmp(func, "loopback")) {
            out = w5_downloader_69e11a_loopback(json_get_str(in, "url", ""));
        } else if (!strcmp(func, "coalesce")) {
            out = w5_downloader_69e11a_coalesce(json_obj_get(in, "ranges"));
        } else if (!strcmp(func, "missing")) {
            out = w5_downloader_69e11a_missing(json_obj_get(in, "total"),
                                               json_obj_get(in, "covered"));
        } else if (!strcmp(func, "source_plain_http")) {
            out = w5_downloader_69e11a_source_plain_http(json_get_str(in, "url", ""),
                        json_get_str(in, "sha256", ""), json_get_str(in, "http_mirror", ""));
        } else if (!strcmp(func, "source_byte_limit")) {
            out = w5_downloader_69e11a_source_byte_limit(json_get_str(in, "url", ""),
                        json_get_str(in, "sha256", ""), json_get_str(in, "http_mirror", ""));
        } else if (!strcmp(func, "validate_range")) {
            out = w5_downloader_69e11a_validate_range(json_obj_get(in, "status"),
                        json_get_str(in, "content_range", ""), json_obj_get(in, "content_length"),
                        json_get_str(in, "content_encoding", "identity"),
                        json_obj_get(in, "start"), json_obj_get(in, "end"), json_obj_get(in, "total"));
        } else if (!strcmp(func, "strong_etag")) {
            out = w5_downloader_69e11a_strong_etag(json_get_str(in, "etag", ""));
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
