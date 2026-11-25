/* JSON-lines harness for the w26_datacleanu_bcef92 gate
 * (hermes_cli/data_cleanup_holders.py).  One request object per stdin line;
 * one response line carrying the computed "out" plus every input key echoed
 * back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_datacleanu_bcef92.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

/* Python truthiness of a JSON value. */
static int truthy(const json_t *v) {
    if (!v) return 0;
    switch (v->type) {
    case JSON_NULL:   return 0;
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    }
    return 0;
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
        if (!strcmp(func, "pause_acked")) {
            out = w26_datacleanu_bcef92_pause_acked(json_obj_get(in, "response"), json_obj_get(in, "identity"));
        } else if (!strcmp(func, "owner_stop_hint")) {
            out = w26_datacleanu_bcef92_owner_stop_hint(json_obj_get(in, "owner"));
        } else if (!strcmp(func, "drain_timeout")) {
            out = w26_datacleanu_bcef92_drain_timeout(json_obj_get(in, "response"));
        } else if (!strcmp(func, "backend_owner_state")) {
            out = w26_datacleanu_bcef92_backend_owner_state(json_obj_get(in, "row"),
                                                            truthy(json_obj_get(in, "running")));
        } else if (!strcmp(func, "cron_owner_state")) {
            out = w26_datacleanu_bcef92_cron_owner_state(json_obj_get(in, "row"),
                                                         truthy(json_obj_get(in, "pid_exists")),
                                                         json_obj_get(in, "current_start"));
        } else if (!strcmp(func, "constants")) {
            out = w26_datacleanu_bcef92_constants();
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
