/* t_port_w17_sessionhis_4326ac.c — JSON-lines harness for the faithful C11
 * port of gateway/session_history_mount.py (gate w17_sessionhis_4326ac). */
#include "hermes_json.h"
#include "gateway/session_history_mount.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *authority = json_obj_get(in, "authority");
        json_t *actor = json_obj_get(in, "actor");
        const char *sid = json_get_str(in, "session_id", "");
        json_t *io = json_obj_get(in, "io");
        json_t *state = json_object();
        json_t *ret = NULL;

        if (!strcmp(func, "reader"))
            ret = w17_sessionhis_4326ac_reader(authority, actor, sid, io, state);
        else if (!strcmp(func, "mount_history"))
            ret = w17_sessionhis_4326ac_mount_history(authority, actor, sid, io, state);
        else if (!strcmp(func, "is_history_mount"))
            ret = w17_sessionhis_4326ac_is_history_mount(json_obj_get(in, "live"));
        else if (!strcmp(func, "require_history_leaf"))
            ret = w17_sessionhis_4326ac_require_history_leaf(authority, sid, io, state);
        else if (!strcmp(func, "read_only"))
            ret = w17_sessionhis_4326ac_read_only();
        else if (!strcmp(func, "require_history_access"))
            ret = w17_sessionhis_4326ac_require_history_access(authority, actor, sid,
                    json_get_str(in, "capability", ""), io, state);
        else
            ret = json_string("UNKNOWN_FUNC");

        json_t *out = json_object();
        json_set(out, "ret", ret ? ret : json_null());
        json_set(out, "state", state);
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
