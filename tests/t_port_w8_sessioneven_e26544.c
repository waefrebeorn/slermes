#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_sessioneven_e26544.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

/* frames: replay note_progress over a list of frames, then progress_for. */
static json_t *run_frames(json_t *in) {
    json_t *state = NULL;             /* _progress starts as None */
    json_t *frames = json_obj_get(in, "frames");
    for (size_t i = 0; i < (frames && frames->type == JSON_ARRAY ? json_len(frames) : 0); i++) {
        json_t *f = json_get(frames, i);
        if (!f || f->type != JSON_OBJECT) continue;
        state = w8_sessioneven_e26544_note_progress(
            state, json_obj_get(f, "generation"), json_get_str(f, "event_type", ""),
            json_obj_get(f, "payload"), json_get_num(f, "now", 0.0));
    }
    json_t *out = w8_sessioneven_e26544_progress_for(state, json_obj_get(in, "generation"));
    json_free(state);
    return out;
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "note")) {
            out = w8_sessioneven_e26544_note_progress(
                json_obj_get(in, "state"), json_obj_get(in, "generation"),
                json_get_str(in, "event_type", ""), json_obj_get(in, "payload"),
                json_get_num(in, "now", 0.0));
        } else if (!strcmp(func, "progress")) {
            out = w8_sessioneven_e26544_progress_for(
                json_obj_get(in, "state"), json_obj_get(in, "generation"));
        } else if (!strcmp(func, "frames")) {
            out = run_frames(in);
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
