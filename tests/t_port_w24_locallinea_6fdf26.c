/* JSON-lines harness for the w24_locallinea_6fdf26 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w24_locallinea_6fdf26.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
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
        if (!strcmp(func, "constants")) {
            out = w24_locallinea_6fdf26_constants();
        } else if (!strcmp(func, "receipt")) {
            out = w24_locallinea_6fdf26_receipt(json_obj_get(in, "raw"));
        } else if (!strcmp(func, "moved")) {
            out = w24_locallinea_6fdf26_moved(json_obj_get(in, "rows"),
                                              json_obj_get(in, "target_ids"));
        } else if (!strcmp(func, "release_keys")) {
            out = w24_locallinea_6fdf26_release_keys(json_obj_get(in, "owners"));
        } else if (!strcmp(func, "handoff")) {
            out = w24_locallinea_6fdf26_handoff(json_obj_get(in, "moved"),
                                                json_obj_get(in, "receipts"));
        } else if (!strcmp(func, "segment_root")) {
            out = w24_locallinea_6fdf26_segment_root(json_obj_get(in, "segments"),
                                                     json_obj_get(in, "target"),
                                                     json_obj_get(in, "end_reasons"));
        } else if (!strcmp(func, "title_moves")) {
            out = json_bool(w24_locallinea_6fdf26_title_moves(
                                json_obj_get(in, "owner"), json_obj_get(in, "head"),
                                json_obj_get(in, "owner_row"),
                                json_obj_get(in, "head_row")) != 0);
        } else if (!strcmp(func, "title_rank")) {
            out = json_number(w24_locallinea_6fdf26_title_rank(json_obj_get(in, "source")));
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
