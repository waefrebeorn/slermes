/* JSON-lines harness for the w23_webservers_36f0c2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w23_webservers_36f0c2.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "poll_s")) {
            out = json_number(w23_webservers_36f0c2_poll_s());
        } else if (!strcmp(func, "confirmations")) {
            out = json_number((double)w23_webservers_36f0c2_confirmations());
        } else if (!strcmp(func, "should_retire")) {
            out = json_bool(w23_webservers_36f0c2_should_retire(
                json_obj_get(in, "skew"),
                json_get_bool(in, "update", false) ? 1 : 0) ? true : false);
        } else if (!strcmp(func, "reason")) {
            json_t *reading = json_object();
            char *r;
            json_set(reading, "skew", json_copy(json_obj_get(in, "skew")));
            json_set(reading, "update", json_bool(json_get_bool(in, "update", false)));
            json_set(reading, "update_raises",
                     json_bool(json_get_bool(in, "update_raises", false)));
            r = w23_webservers_36f0c2_observe(reading);
            out = r ? json_string(r) : json_null();
            free(r);
            json_free(reading);
        } else if (!strcmp(func, "run")) {
            out = w23_webservers_36f0c2_run(json_obj_get(in, "world"));
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
