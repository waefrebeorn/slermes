/* JSON-lines harness for the w26_resources_caddbd gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_resources_caddbd.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[131072];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "read_int")) {
            out = w26_resources_caddbd_read_int(in);
        } else if (!strcmp(func, "meminfo")) {
            out = w26_resources_caddbd_meminfo(in);
        } else if (!strcmp(func, "stat_value")) {
            out = w26_resources_caddbd_stat_value(in);
        } else if (!strcmp(func, "cgroup")) {
            out = w26_resources_caddbd_cgroup(in);
        } else if (!strcmp(func, "memory_info")) {
            out = w26_resources_caddbd_memory_info(in);
        } else if (!strcmp(func, "min_free_mb")) {
            out = w26_resources_caddbd_min_free_mb(in);
        } else if (!strcmp(func, "tight_headroom_mb")) {
            out = w26_resources_caddbd_tight_headroom_mb(in);
        } else if (!strcmp(func, "memory_blocker")) {
            out = w26_resources_caddbd_memory_blocker(in);
        } else if (!strcmp(func, "constants")) {
            out = w26_resources_caddbd_constants();
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
