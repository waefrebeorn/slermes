/* Port gate harness for agent/auxiliary_local_runtime.py (gate auxlocal). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_auxlocal.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

/* string-or-NULL fetch; missing and JSON null both mean Python None. */
static const char *jstr_or_null(const json_t *in, const char *k) {
    json_t *v = json_obj_get(in, k);
    if (!v || v->type == JSON_NULL) return NULL;
    return v->type == JSON_STRING ? v->str_val : "";
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
        if (!strcmp(func, "bare")) {
            out = auxlocal_bare_llamacpp_endpoint(
                jstr_or_null(in, "provider"),
                jstr_or_null(in, "base_url_in"),
                jstr_or_null(in, "api_key"),
                json_get_bool(in, "named", 0) ? 1 : 0,
                json_get_bool(in, "ep_present", 0) ? 1 : 0,
                jstr_or_null(in, "ep_base_url"),
                jstr_or_null(in, "ep_api_key"));
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
