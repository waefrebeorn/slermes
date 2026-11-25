/* t_port_acp_modes.c — harness for src/acp_adapter/modes.c.
 * Reads fixture lines {"func":..., ...} and emits {"func":..., "out":<json>}.
 * Oracle replays the LIVE acp_adapter/modes.py tables and dict.get lookups.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "acp_modes.h"

static void emit_json_value(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[8192];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "constants")) {
            out = json_object();
            json_set(out, "DEFAULT_MODE", json_string(acp_modes_default_mode()));
            json_set(out, "DEFAULT_EDIT_APPROVAL_POLICY",
                     json_string(acp_modes_default_edit_approval_policy()));
            json_set(out, "EDIT_APPROVAL_POLICY_CONFIG_ID",
                     json_string(acp_modes_edit_approval_policy_config_id()));
        } else if (!strcmp(func, "modes")) {
            out = acp_modes_table();
        } else if (!strcmp(func, "mode_to_policy")) {
            out = acp_modes_mode_to_policy();
        } else if (!strcmp(func, "policy_to_mode")) {
            out = acp_modes_policy_to_mode();
        } else if (!strcmp(func, "policy_for_mode")) {
            const char *mode = json_get_str(in, "mode", NULL);
            const char *fallback = json_get_str(in, "fallback", NULL);
            const char *r = acp_modes_policy_for_mode(mode, fallback);
            out = r ? json_string(r) : json_null();
        } else if (!strcmp(func, "mode_for_policy")) {
            const char *policy = json_get_str(in, "policy", NULL);
            const char *r = acp_modes_mode_for_policy(policy);
            out = r ? json_string(r) : json_null();
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        printf("{");
        emit_json_value("func", json_string(func));
        fputs(",", stdout);
        emit_json_value("out", out);
        for (int i = 0; i < 3; i++) {
            const char *key = (i == 0) ? "mode" : (i == 1) ? "fallback" : "policy";
            json_t *v = json_obj_get(in, key);
            if (v) {
                fputs(",", stdout);
                emit_json_value(key, json_copy(v));
            }
        }
        fputs("}\n", stdout);
        fflush(stdout);
        json_free(out);
        json_free(in);
    }
    return 0;
}
