/* JSON-lines harness for the w34_sessions_67685b gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w34_sessions_67685b.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *dispatch(const json_t *in, const char *func) {
    if (!strcmp(func, "catalog"))
        return w34_sessions_67685b_catalog();
    if (!strcmp(func, "models"))
        return w34_sessions_67685b_models();
    if (!strcmp(func, "fields"))
        return w34_sessions_67685b_fields(json_get_str(in, "model", NULL));
    if (!strcmp(func, "required"))
        return w34_sessions_67685b_required(json_get_str(in, "model", NULL));
    if (!strcmp(func, "validate"))
        return w34_sessions_67685b_validate(json_get_str(in, "method", NULL), json_obj_get(in, "params"));
    if (!strcmp(func, "id_requirement"))
        return w34_sessions_67685b_id_requirement(json_get_str(in, "method", NULL));
    if (!strcmp(func, "enum"))
        return w34_sessions_67685b_enum(json_get_str(in, "name", NULL));
    if (!strcmp(func, "accepts"))
        return w34_sessions_67685b_accepts(json_get_str(in, "method", NULL),
                                          json_get_str(in, "key", NULL));
    return json_string("UNKNOWN_FUNC");
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
        out = dispatch(in, func);
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
