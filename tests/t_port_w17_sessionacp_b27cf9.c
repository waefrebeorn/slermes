/* t_port_w17_sessionacp_b27cf9.c — JSON-lines harness for the faithful C11
 * port of gateway/session_acp_lifecycle.py (gate w17_sessionacp_b27cf9). */
#include "hermes_json.h"
#include "gateway/session_acp_lifecycle.c"

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
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
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

        if (!strcmp(func, "end_idle_acp_session"))
            ret = w17_sessionacp_b27cf9_end_idle_acp_session(authority, sid, io, state);
        else if (!strcmp(func, "end_idle_viewer_session"))
            ret = w17_sessionacp_b27cf9_end_idle_viewer_session(authority, sid,
                    json_get_str(in, "reason", ""), io, state);
        else if (!strcmp(func, "reopen_if_reattached"))
            ret = w17_sessionacp_b27cf9_reopen_if_reattached(authority, sid,
                    json_get_bool(in, "ended", false), io, state);
        else if (!strcmp(func, "end_idle_acp_sessions"))
            ret = w17_sessionacp_b27cf9_end_idle_acp_sessions(authority, io, state);
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
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
