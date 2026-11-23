/* t_port_w5_files_19ae74.c — harness for utils/files.py port.
 * Reads {"func":<name>, ...args...}, emits
 * {"func":<name>,"out":<value>, ...echoed input keys...}.
 * 64-bit quantities arrive as decimal strings.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_files_19ae74.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static long long h_i64(json_t *in, const char *k) {
    json_t *v = json_obj_get(in, k);
    if (v && v->type == JSON_STRING) return strtoll(v->str_val, NULL, 10);
    if (v && v->type == JSON_NUMBER) return (long long)v->num_val;
    return 0;
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "w5_files_19ae74_yaml11_ambiguous")) {
            out = json_bool(w5_files_19ae74_yaml11_ambiguous(json_get_str(in, "word", "")) != 0);
        } else if (!strcmp(func, "w5_files_19ae74_contended_replace_error")) {
            int r = w5_files_19ae74_contended_replace_error(
                (int)json_get_bool(in, "is_windows", 0), h_i64(in, "winerror"));
            out = json_bool(r != 0);
        } else if (!strcmp(func, "w5_files_19ae74_mtime_is_racy")) {
            int r = w5_files_19ae74_mtime_is_racy(json_get_str(in, "mtime_ns", "0"),
                                                  json_get_str(in, "now_ns", "0"));
            out = json_bool(r != 0);
        } else if (!strcmp(func, "w5_files_19ae74_file_signature")) {
            out = w5_files_19ae74_file_signature(json_get_str(in, "mtime_ns", "0"),
                json_get_str(in, "size", "0"), json_get_str(in, "ino", "0"),
                json_get_str(in, "ctime_ns", "0"));
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
