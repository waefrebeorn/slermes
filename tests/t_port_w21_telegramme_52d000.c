/* JSON-lines harness for the w21_telegramme_52d000 gate.
 * One request object per stdin line; one response per line carrying "out" plus
 * every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w21_telegramme_52d000.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *finish(json_t *in, const char *func, json_t *out) {
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
    return NULL;
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
        if (!strcmp(func, "sources")) {
            out = w21_telegramme_52d000_media_sources(json_obj_get(in, "media"));
        } else if (!strcmp(func, "seconds")) {
            out = w21_telegramme_52d000_upload_seconds(json_obj_get(in, "sizes"));
        } else if (!strcmp(func, "deadline")) {
            out = w21_telegramme_52d000_scaled_deadline(
                json_get_num(in, "deadline", 0.0), json_get_num(in, "seconds", 0.0));
        } else if (!strcmp(func, "budgets")) {
            json_t *rt = json_obj_get(in, "read_timeout");
            double read_timeout = (rt && rt->type == JSON_NUMBER)
                                      ? rt->num_val
                                      : W21_TELEGRAMME_52D000_MEDIA_READ_TIMEOUT;
            out = w21_telegramme_52d000_scaled_budgets(
                json_obj_get(in, "sizes"), json_get_num(in, "deadline", 0.0), read_timeout);
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        finish(in, func, out);
        json_free(in);
    }
    return 0;
}
