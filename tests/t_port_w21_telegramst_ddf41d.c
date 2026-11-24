/* JSON-lines harness for the w21_telegramst_ddf41d gate.
 * One request object per stdin line; one response per line carrying "out" plus
 * every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w21_telegramst_ddf41d.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        w21_telegramst_ddf41d_env e;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        memset(&e, 0, sizeof(e));
        e.message_ids = json_obj_get(in, "message_ids");
        e.max_ids = json_object_get(in, "max") ? (long long)json_get_num(in, "max", 0.0) : -1;
        e.lock_exists = json_get_bool(in, "lock_exists", 0) ? 1 : 0;
        e.edit = json_obj_get(in, "edit");
        e.send = json_obj_get(in, "send");
        {
            json_t *d = json_obj_get(in, "during_edit");
            e.during_edit = (d && d->type == JSON_STRING) ? d->str_val : NULL;
        }
        if (!strcmp(func, "update")) {
            out = w21_telegramst_ddf41d_send_or_update(
                json_obj_get(in, "chat_id"), json_obj_get(in, "status_key"),
                json_get_str(in, "content", ""), &e);
        } else if (!strcmp(func, "str")) {
            char *s = w21_telegramst_ddf41d_py_str(json_obj_get(in, "value"));
            out = json_string(s ? s : "");
            free(s);
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
