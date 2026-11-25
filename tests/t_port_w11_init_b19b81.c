#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_init_b19b81.h"

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
        if (!strcmp(func, "normalize_arch")) {
            out = w11_init_b19b81_normalize_arch(json_get_str(in, "raw", ""));
        } else if (!strcmp(func, "windows_native_arch")) {
            json_t *w = json_obj_get(in, "wow64_native");
            const char *wv = (w && w->type == JSON_STRING) ? w->str_val : "";
            out = w11_init_b19b81_windows_native_arch(wv, json_get_str(in, "machine", ""));
        } else if (!strcmp(func, "classify_gpu_vendors")) {
            out = w11_init_b19b81_classify_gpu_vendors(json_obj_get(in, "vendor_ids"));
        } else if (!strcmp(func, "parse_cpuinfo")) {
            out = w11_init_b19b81_parse_cpuinfo(json_get_str(in, "text", ""),
                                                json_get_str(in, "device_tree_model", ""));
        } else if (!strcmp(func, "parse_cpuinfo_vendor")) {
            out = w11_init_b19b81_parse_cpuinfo_vendor(json_get_str(in, "text", ""));
        } else if (!strcmp(func, "parse_meminfo_total")) {
            out = w11_init_b19b81_parse_meminfo_total(json_get_str(in, "text", ""));
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
