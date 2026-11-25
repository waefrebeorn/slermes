/* Port gate harness for agent/image_replay.py (gate w6_imagereplay_7778b4). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_imagereplay_7778b4.h"

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
        json_t *out = NULL;
        if (!strcmp(func, "resolve")) {
            out = imagereplay_resolve_recent(json_obj_get(in, "cfg"));
        } else if (!strcmp(func, "frontier")) {
            out = imagereplay_frontier(json_get_num(in, "total", 0), json_get_num(in, "keep", 1));
        } else if (!strcmp(func, "split")) {
            out = imagereplay_split_row(json_get_str(in, "content", ""));
        } else if (!strcmp(func, "replay")) {
            out = imagereplay_replay_recent(json_obj_get(in, "messages"),
                                            json_get_num(in, "keep", 3),
                                            json_obj_get(in, "urls"));
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
