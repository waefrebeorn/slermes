/* JSON-lines harness for the w20_provider_8219de gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w20_provider_8219de.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "search")) {
            out = w20_provider_8219de_search(json_obj_get(in, "results"));
        } else if (!strcmp(func, "extract")) {
            out = w20_provider_8219de_extract(json_obj_get(in, "results"),
                                              json_obj_get(in, "statuses"));
        } else if (!strcmp(func, "crawl_status")) {
            out = w20_provider_8219de_crawl_status(json_obj_get(in, "error"));
        } else if (!strcmp(func, "vendor_error_entry")) {
            out = w20_provider_8219de_vendor_error_entry(json_obj_get(in, "url"),
                                                         json_obj_get(in, "status"),
                                                         json_obj_get(in, "reason"),
                                                         json_obj_get(in, "body"));
        } else if (!strcmp(func, "document")) {
            out = w20_provider_8219de_document(json_get_str(in, "url", ""),
                                               json_get_str(in, "title", ""),
                                               json_get_str(in, "content", ""));
        } else if (!strcmp(func, "setup_schema")) {
            out = w20_provider_8219de_setup_schema();
        } else if (!strcmp(func, "use_keyless")) {
            out = json_bool(w20_provider_8219de_use_keyless(json_obj_get(in, "tier"),
                                                            json_get_str(in, "api_key", ""),
                                                            json_get_bool(in, "keyless_enabled", false)) ? 1 : 0);
        } else if (!strcmp(func, "missing_key")) {
            out = json_string(w20_provider_8219de_missing_key());
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
