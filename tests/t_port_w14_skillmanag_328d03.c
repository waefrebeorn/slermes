#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w14_skillmanag_328d03.h"

static void emit_key(const char *key, struct json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static int pinned_value(const struct json_t *in) {
    const struct json_t *v = json_obj_get(in, "pinned");
    if (!v || v->type == JSON_NULL) return -1;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0 ? 1 : 0;
    return -1;
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        struct json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        struct json_t *out = NULL;
        if (!strcmp(func, "refusal")) {
            out = w14_skillmanag_328d03_refusal(
                json_get_str(in, "message", ""), json_obj_get(in, "extra"));
        } else if (!strcmp(func, "pinned_guard")) {
            out = w14_skillmanag_328d03_pinned_guard(
                json_get_str(in, "name", ""), json_obj_get(in, "essential"),
                pinned_value(in));
        } else if (!strcmp(func, "read_before_write_guard")) {
            out = w14_skillmanag_328d03_read_before_write_guard(
                json_get_str(in, "name", ""), json_get_str(in, "target", ""),
                json_get_str(in, "action", ""), json_get_str(in, "file_label", ""),
                json_get_bool(in, "is_bg", false) ? 1 : 0,
                json_get_bool(in, "has_read", false) ? 1 : 0);
        } else if (!strcmp(func, "curator_consolidation_delete_guard")) {
            const struct json_t *ab = json_obj_get(in, "absorbed_into");
            out = w14_skillmanag_328d03_curator_consolidation_delete_guard(
                json_get_str(in, "name", ""),
                (ab && ab->type == JSON_STRING) ? ab->str_val : NULL,
                json_get_bool(in, "is_bg", false) ? 1 : 0);
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
