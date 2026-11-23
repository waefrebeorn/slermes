#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "noc_lazyfwd.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *forwarder_json(const lazyfwd_forwarder_t *f) {
    json_t *o = json_object();
    json_set(o, "name", json_string(f->name));
    json_set(o, "qualname", json_string(f->qualname));
    json_set(o, "doc", json_string(f->doc));
    json_set(o, "static", json_bool(f->is_static));
    return o;
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        const char *module = json_get_str(in, "module", "");
        const char *name = json_get_str(in, "name", "");
        json_t *out = NULL;

        if (!strcmp(func, "forward") || !strcmp(func, "forward_static")) {
            lazyfwd_forwarder_t f;
            if (!strcmp(func, "forward_static"))
                lazyfwd_forward_static(module, name, &f);
            else
                lazyfwd_forward(module, name,
                                json_get_bool(in, "static", false), &f);
            out = forwarder_json(&f);
            lazyfwd_forwarder_free(&f);
        } else if (!strcmp(func, "doc")) {
            char *d = lazyfwd_doc(module, name);
            out = json_string(d ? d : "");
            free(d);
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
