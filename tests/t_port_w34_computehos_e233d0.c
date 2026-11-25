/* JSON-lines harness for the w34_computehos_e233d0 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w34_computehos_e233d0.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

/* An absent key or an explicit null both mean the Python None default. */
static const json_t *opt(const json_t *in, const char *key) {
    const json_t *v = json_obj_get(in, key);
    return (v && v->type != JSON_NULL) ? v : NULL;
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "turn_isolation_enabled")) {
            out = json_bool(w34_computehos_e233d0_turn_isolation_enabled(
                json_obj_get(in, "cfg"), (int)json_get_num(in, "child_env_is_1", 0)));
        } else if (!strcmp(func, "session_uses_compute_host")) {
            out = json_bool(w34_computehos_e233d0_session_uses_compute_host(
                json_obj_get(in, "session"), json_obj_get(in, "cfg"),
                (int)json_get_num(in, "child_env_is_1", 0)));
        } else if (!strcmp(func, "active_session_lease_vouch")) {
            out = w34_computehos_e233d0_active_session_lease_vouch(json_obj_get(in, "session"));
            if (!out) out = json_null();
        } else if (!strcmp(func, "metadata_mirror")) {
            out = w34_computehos_e233d0_metadata_mirror(json_obj_get(in, "session"));
        } else if (!strcmp(func, "open_request_matches")) {
            out = json_bool(w34_computehos_e233d0_open_request_matches(
                json_obj_get(in, "session"), opt(in, "request_id")));
        } else if (!strcmp(func, "adopt_frame_meta")) {
            out = w34_computehos_e233d0_adopt_frame_meta(json_obj_get(in, "session"),
                                                         json_obj_get(in, "frame"));
        } else if (!strcmp(func, "control_frame")) {
            out = w34_computehos_e233d0_control_frame(json_obj_get(in, "payload"),
                                                      opt(in, "command"));
        } else if (!strcmp(func, "compress_wait_seconds")) {
            out = json_number(w34_computehos_e233d0_compress_wait_seconds(json_obj_get(in, "cfg")));
        } else if (!strcmp(func, "ack_error")) {
            out = w34_computehos_e233d0_ack_error(opt(in, "rid"), json_obj_get(in, "ack"),
                                                  (int)json_get_num(in, "code", 0),
                                                  json_get_str(in, "default", ""));
            if (!out) out = json_null();
        } else if (!strcmp(func, "turn_frame")) {
            out = w34_computehos_e233d0_turn_frame(
                opt(in, "rid"), opt(in, "sid"), json_obj_get(in, "session"), opt(in, "text"),
                opt(in, "image_paths"), opt(in, "queued_prompt_generation"),
                opt(in, "display_kind"), opt(in, "display_metadata"));
            if (!out) out = json_null();
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
