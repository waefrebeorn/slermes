/* t_port_w17_streamcons_bd185c.c — JSON-lines harness for gateway/stream_consumer_think.py port. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w17_streamcons_bd185c.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *jstr(char *s) { json_t *o = json_string(s ? s : ""); free(s); return o; }

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        const char *acc = json_get_str(in, "accumulated", "");
        if (!strcmp(func, "at_block_boundary"))
            out = json_bool(w17_streamcons_bd185c_at_block_boundary(
                acc, json_get_str(in, "buf", ""), (int)json_get_num(in, "idx", 0)) ? 1 : 0);
        else if (!strcmp(func, "earliest_open_tag")) {
            int bi = -1, bl = 0;
            w17_streamcons_bd185c_earliest_open_tag(acc, json_get_str(in, "buf", ""), &bi, &bl);
            out = json_array();
            json_append(out, json_number(bi));
            json_append(out, json_number(bl));
        }
        else if (!strcmp(func, "strip_orphan_close_tags"))
            out = jstr(w17_streamcons_bd185c_strip_orphan_close_tags(json_get_str(in, "text", "")));
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
