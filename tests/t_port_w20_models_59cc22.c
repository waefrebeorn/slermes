/* JSON-lines harness for the w20_models_59cc22 gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash.
 *
 * The port source is included by bare name (the repo's harness convention) so
 * the gate links against libjson alone. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w20_models_59cc22.h"
#include "plugins/teams_pipeline/models.c"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *str_or(json_t *in, const char *key, const char *def) {
    return json_get_str(in, key, def);
}

/* Build the string-key vector for pick() from a JSON array of strings. */
static json_t *do_pick(json_t *in) {
    json_t *keys = json_obj_get(in, "keys");
    const char *vec[16];
    size_t n = 0, i;
    if (!keys || keys->type != JSON_ARRAY) return json_null();
    for (i = 0; i < keys->c.count && n < 16; i++) {
        json_t *k = keys->c.items[i];
        if (k && k->type == JSON_STRING) vec[n++] = k->str_val;
    }
    return w20_models_59cc22_pick(json_obj_get(in, "payload"), vec, n);
}

int main(void) {
    char line[65536];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "camel")) {
            char *s = w20_models_59cc22_camel(str_or(in, "name", ""));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "str")) {
            char *s = w20_models_59cc22_str(json_obj_get(in, "value"));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "pick")) {
            out = do_pick(in);
        } else if (!strcmp(func, "parse")) {
            json_t *v = json_obj_get(in, "value");
            out = w20_models_59cc22_parse_datetime(v && v->type == JSON_STRING ? v->str_val : NULL);
        } else if (!strcmp(func, "from_dict")) {
            out = w20_models_59cc22_from_dict(str_or(in, "model", ""), json_obj_get(in, "payload"));
        } else if (!strcmp(func, "roundtrip")) {
            out = w20_models_59cc22_roundtrip(str_or(in, "model", ""), json_obj_get(in, "payload"));
        } else if (!strcmp(func, "to_dict")) {
            out = w20_models_59cc22_to_dict(str_or(in, "model", ""), json_obj_get(in, "object"));
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
