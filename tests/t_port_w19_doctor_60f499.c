/* tests/t_port_w19_doctor_60f499.c - JSON-lines harness for persistence/sqlite/admin/_doctor.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_doctor_60f499.h"

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
        if (!strcmp(func, "header_mode")) {
            json_t *hex = json_obj_get(in, "header_hex");
            const char *hex_s = (hex && hex->type == JSON_STRING) ? hex->str_val : "";
            out = w19_doctor_60f499_header_mode(hex_s);
        } else if (!strcmp(func, "refusal")) {
            json_t *cur = json_obj_get(in, "current");
            const char *cur_s = (cur && cur->type == JSON_STRING) ? cur->str_val : NULL;
            out = w19_doctor_60f499_refusal(json_get_str(in, "target", "wal"), cur_s,
                                            json_get_bool(in, "on_cross_vm_fs", 0) ? 1 : 0);
        } else if (!strcmp(func, "newest_mtime_ns")) {
            out = w19_doctor_60f499_newest_mtime_ns(json_obj_get(in, "values"));
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
