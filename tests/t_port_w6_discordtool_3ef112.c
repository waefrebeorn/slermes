/* Port gate harness for tools/discord_tool.py (gate w6_discordtool_3ef112). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w6_discordtool_3ef112.h"

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
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "validate")) {
            out = discordtool_validate_snowflake(json_get_str(in, "value", ""),
                                                 json_get_str(in, "param_name", "guild_id"));
        } else if (!strcmp(func, "channel_type")) {
            out = discordtool_channel_type_name(json_get_str(in, "type_id", "0"));
        } else if (!strcmp(func, "limit")) {
            out = discordtool_limit_param(json_get_str(in, "limit", ""),
                                          json_get_str(in, "default_value", "50"));
        } else if (!strcmp(func, "enrich_403")) {
            out = discordtool_enrich_403(json_get_str(in, "action", ""), json_get_str(in, "body", ""));
        } else if (!strcmp(func, "token_key")) {
            out = discordtool_token_cache_key(json_get_str(in, "token", ""));
        } else if (!strcmp(func, "available")) {
            out = discordtool_available_actions(json_obj_get(in, "caps"), json_obj_get(in, "allowlist"));
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
