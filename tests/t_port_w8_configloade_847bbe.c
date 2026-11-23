/* t_port_w8_configloade_847bbe.c — harness for src/gateway/config_loader.c
 * Reads {"func":..., ...input keys...}, emits {"func", "out", ...echoed input keys...}.
 * Gate: run_one_oracle.sh w8_configloade_847bbe oracle_w8_configloade_847bbe.py "" "<objs> lib/libjson/json.o"
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w8_configloade_847bbe.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
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
        if (!strcmp(func, "bridge_lookup")) {
            out = w8_configloade_847bbe_bridge_lookup(
                json_obj_get(in, "yaml_cfg"), json_obj_get(in, "gateway_section"),
                json_obj_get(in, "gw_data"), json_get_str(in, "key", ""),
                json_get_str(in, "mode", "presence"));
        } else if (!strcmp(func, "quick_commands_ok")) {
            out = w8_configloade_847bbe_quick_commands_ok(json_obj_get(in, "value"));
        } else if (!strcmp(func, "str_keyed")) {
            out = w8_configloade_847bbe_str_keyed(json_obj_get(in, "value"));
        } else if (!strcmp(func, "platform_section")) {
            out = w8_configloade_847bbe_platform_section(
                json_obj_get(in, "yaml_cfg"), json_get_str(in, "name", ""),
                json_obj_get(in, "gateway_platforms"));
        } else if (!strcmp(func, "snapshot_authored_extra")) {
            out = w8_configloade_847bbe_snapshot_authored_extra(json_obj_get(in, "platforms_data"));
        }
        printf("{");
        json_t *fv = json_string(func);
        emit_key("func", fv);
        json_free(fv);
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
