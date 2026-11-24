/* t_port_w17_sessiondef_feb26c.c — JSON-lines harness for the faithful C11
 * port of gateway/session_deferred_pick.py (gate w17_sessiondef_feb26c). */
#include "hermes_json.h"
#include "gateway/session_deferred_pick.c"

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
        json_t *io = json_obj_get(in, "io");
        const char *sid = json_get_str(in, "session_id", "");
        json_t *state = json_object();
        json_t *ret = NULL;
        json_t *live = NULL;

        if (!strcmp(func, "defer_pick")) {
            ret = w17_sessiondef_feb26c_defer_pick(authority, json_obj_get(in, "actor"), sid,
                    json_obj_get(in, "params"), io, state);
        } else if (!strcmp(func, "reask_on_attach")) {
            live = json_copy(json_obj_get(in, "live"));
            ret = w17_sessiondef_feb26c_reask_on_attach(authority, sid, live, io, state);
        } else if (!strcmp(func, "commit")) {
            ret = w17_sessiondef_feb26c_commit(authority, sid, json_obj_get(in, "entry"),
                    json_get_str(in, "operation", "model"), io, state);
        } else if (!strcmp(func, "commit_level")) {
            ret = w17_sessiondef_feb26c_commit_level(authority, sid, json_obj_get(in, "raw"),
                    json_obj_get(in, "pick"), io, state);
        } else if (!strcmp(func, "apply_deferred_pick")) {
            live = json_copy(json_obj_get(in, "live"));
            ret = w17_sessiondef_feb26c_apply_deferred_pick(authority, sid, live, io, state);
        } else {
            ret = json_string("UNKNOWN_FUNC");
        }

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
        if (live) json_free(live);
        json_free(in);
    }
    return 0;
}
