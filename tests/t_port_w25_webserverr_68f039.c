/* JSON-lines harness for the w25_webserverr_68f039 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_webserverr_68f039.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static long long as_ll(const json_t *v) {
    if (!v) return 0;
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    return 0;
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "position")) {
            const json_t *d = json_obj_get(in, "digits");
            out = w25_webserverr_68f039_position(
                d && d->type == JSON_STRING ? d->str_val : NULL);
        } else if (!strcmp(func, "byte_range")) {
            out = w25_webserverr_68f039_byte_range(json_obj_get(in, "header"),
                                                   as_ll(json_obj_get(in, "size")));
        } else if (!strcmp(func, "response")) {
            const json_t *t = json_obj_get(in, "target");
            const json_t *mt = json_obj_get(in, "media_type");
            out = w25_webserverr_68f039_response(json_obj_get(in, "request"),
                                                 t && t->type == JSON_STRING ? t->str_val : NULL,
                                                 as_ll(json_obj_get(in, "size")),
                                                 mt && mt->type == JSON_STRING ? mt->str_val : NULL);
        } else if (!strcmp(func, "constants")) {
            out = w25_webserverr_68f039_constants();
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
