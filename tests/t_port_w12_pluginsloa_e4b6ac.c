/* t_port_w12_pluginsloa_e4b6ac.c — JSON-lines oracle harness for hermes_cli/plugins_loader.py */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w12_pluginsloa_e4b6ac.h"

static void emit_key(const char *key, json_t *v)
{
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void)
{
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        char buf[4096];
        json_t *out = NULL;

        if (!strcmp(func, "dist_name")) {
            w12_pluginsloa_e4b6ac_dist_name(json_get_str(in, "req", ""), buf, sizeof(buf));
            out = json_string(buf);
        } else if (!strcmp(func, "load_error_text")) {
            w12_pluginsloa_e4b6ac_load_error_text(json_get_bool(in, "systemexit", false),
                                json_obj_get(in, "code"),
                                json_get_str(in, "text", ""), buf, sizeof(buf));
            out = json_string(buf);
        } else if (!strcmp(func, "platform_name")) {
            w12_pluginsloa_e4b6ac_platform_name(json_get_str(in, "name", ""),
                              json_get_str(in, "path", ""), buf, sizeof(buf));
            out = json_string(buf);
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
