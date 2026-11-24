/* tests/t_port_w19_voicementi_2db922.c - JSON-lines harness for plugins/platforms/matrix/voice_mention.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w19_voicementi_2db922.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "has_voice_marker")) {
            out = json_bool(w19_voicementi_2db922_has_voice_marker(json_obj_get(in, "content")));
        } else if (!strcmp(func, "is_voice_event")) {
            out = json_bool(w19_voicementi_2db922_is_voice_event(json_obj_get(in, "content")));
        } else if (!strcmp(func, "parked_replay")) {
            out = w19_voicementi_2db922_parked_replay(json_obj_get(in, "ops"));
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
