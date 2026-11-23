/* Port gate harness for tui_gateway/mcp_rpc_helpers.py (gate w15_mcprpchel_bbb1c7). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_mcprpchel_bbb1c7.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *jstr_or_null(json_t *in, const char *k) {
    json_t *v = json_obj_get(in, k);
    if (!v || v->type == JSON_NULL) return NULL;
    return v->type == JSON_STRING ? v->str_val : "";
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "summary")) {
            out = w15_mcprpchel_bbb1c7_summarize_server(
                json_get_str(in, "name", ""), json_obj_get(in, "cfg"),
                jstr_or_null(in, "plugin"),
                json_get_bool(in, "oauth", 0) ? 1 : 0,
                json_get_bool(in, "enabled", 0) ? 1 : 0);
        } else if (!strcmp(func, "sources")) {
            out = w15_mcprpchel_bbb1c7_server_configs_with_sources(
                json_obj_get(in, "config_servers"), json_obj_get(in, "portable"),
                json_obj_get(in, "owners"));
        } else if (!strcmp(func, "record")) {
            out = w15_mcprpchel_bbb1c7_record_mcp_add(
                json_get_bool(in, "entry_present", 0) ? 1 : 0,
                json_get_str(in, "entry_name", ""), json_obj_get(in, "server_config"),
                json_get_bool(in, "saved", 0) ? 1 : 0);
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
