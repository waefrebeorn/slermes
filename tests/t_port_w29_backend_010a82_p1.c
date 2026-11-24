/* JSON-lines harness for the w29_backend_010a82_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w29_backend_010a82_p1.h"

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
        if (!strcmp(func, "constants")) {
            out = w29_backend_010a82_constants();
        } else if (!strcmp(func, "poll_interval")) {
            out = w29_backend_010a82_poll_interval();
        } else if (!strcmp(func, "latest_config_version")) {
            out = w29_backend_010a82_latest_config_version(in);
        } else if (!strcmp(func, "effective_ok")) {
            out = w29_backend_010a82_effective_ok(json_obj_get(in, "body"));
        } else if (!strcmp(func, "profile_writer")) {
            out = w29_backend_010a82_profile_writer(json_obj_get(in, "body"));
        } else if (!strcmp(func, "value_refusal")) {
            out = w29_backend_010a82_value_refusal(in);
        } else if (!strcmp(func, "write_error")) {
            out = w29_backend_010a82_write_error(in);
        } else if (!strcmp(func, "note_level")) {
            out = w29_backend_010a82_note_level(in);
        } else if (!strcmp(func, "capabilities")) {
            out = w29_backend_010a82_capabilities();
        } else if (!strcmp(func, "doctor_warnings")) {
            out = w29_backend_010a82_doctor_warnings(in);
        } else if (!strcmp(func, "destination")) {
            out = w29_backend_010a82_destination(in);
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
