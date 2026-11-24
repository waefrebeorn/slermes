/* JSON-lines harness for the w25_telegramin_d02e02 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w25_telegramin_d02e02.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
    if (v) json_free(v);
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
        if (!strcmp(func, "contact_text")) {
            out = w25_telegramin_d02e02_contact_text(json_obj_get(in, "contact"));
        } else if (!strcmp(func, "drop_matching")) {
            out = w25_telegramin_d02e02_drop_matching(json_obj_get(in, "pending"),
                                                      json_obj_get(in, "session_key"));
        } else if (!strcmp(func, "discard_media_buffers")) {
            out = w25_telegramin_d02e02_discard_media_buffers(json_obj_get(in, "session_key"),
                                                              json_obj_get(in, "photo_batches"),
                                                              json_obj_get(in, "media_group_events"));
        } else if (!strcmp(func, "constants")) {
            out = w25_telegramin_d02e02_constants();
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
        json_free(in);
    }
    return 0;
}
