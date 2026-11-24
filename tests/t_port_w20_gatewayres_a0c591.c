/* JSON-lines harness for the w20_gatewayres_a0c591 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash:
 * the fixture describes the start-time seam, which the live function reads via
 * process_start_time_stdlib; the C port takes it as an argument. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w20_gatewayres_a0c591.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

/* 64-bit integers travel as decimal strings (double would round a pid). */
static long long as_ll(const json_t *in, const char *key, long long def) {
    json_t *v = json_obj_get(in, key);
    if (!v) return def;
    if (v->type == JSON_STRING && v->str_val) return strtoll(v->str_val, NULL, 10);
    if (v->type == JSON_NUMBER) return (long long)v->num_val;
    return def;
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "watcher_source")) {
            /* start_time: the repr() of process_start_time_stdlib(old_pid). */
            json_t *st = json_obj_get(in, "start_time");
            const char *start = (st && st->type == JSON_STRING && st->str_val && st->str_val[0])
                                    ? st->str_val : NULL;
            char *src = w20_gatewayres_a0c591_watcher_source(
                as_ll(in, "old_pid", 0),
                json_get_str(in, "respawn_cwd", ""),
                json_obj_get(in, "respawn_env_overlay"),
                json_get_num(in, "timeout_s", 0.0),
                json_get_str(in, "project_root", ""),
                start);
            out = json_string(src ? src : "");
            free(src);
        } else if (!strcmp(func, "source_template")) {
            char *src = w20_gatewayres_a0c591_source_template();
            out = json_string(src ? src : "");
            free(src);
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
