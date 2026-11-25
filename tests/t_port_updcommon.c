#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_updcommon.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *call_obj(const char *method, json_t *args) {
    json_t *o = json_object();
    json_set(o, "method", json_string(method));
    json_set(o, "args", args);
    return o;
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

        if (!strcmp(func, "record")) {
            json_t *wr = json_obj_get(in, "without_receipt");
            int has_without = (wr && wr->type == JSON_STRING) ? 1 : 0;
            int hr = json_get_bool(in, "has_receipt", false) ? 1 : 0;
            int hrr = json_get_bool(in, "has_record_reason", false) ? 1 : 0;
            int hrw = json_get_bool(in, "has_record_without", false) ? 1 : 0;
            int code = updcommon_record_stop(hr, hrr, hrw, has_without);
            out = json_array();
            if (code == 1) {
                json_t *args = json_array();
                json_append(args, json_string(json_get_str(in, "reason", "")));
                json_append(out, call_obj("record_stop_reason", args));
            } else if (code == 2) {
                json_t *args = json_array();
                json_append(args, json_string(json_get_str(in, "reason", "")));
                json_append(args, json_string(has_without ? wr->str_val : ""));
                json_append(out, call_obj("record_stop_without_receipt", args));
            }
        } else if (!strcmp(func, "best")) {
            int raises = json_get_bool(in, "raises", false) ? 1 : 0;
            int code = updcommon_best_effort(raises);
            out = json_array();
            if (code) {
                json_t *call = json_array();
                json_append(call, json_string(json_get_str(in, "message", "")));
                json_append(call, json_string(json_get_str(in, "exc", "")));
                json_append(out, call);
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
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
