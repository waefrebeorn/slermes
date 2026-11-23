#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_planlimits.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static json_t *cstr(char *s) { json_t *o = json_string(s ? s : ""); free(s); return o; }

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "model_label")) {
            out = cstr(planlimits_model_label(json_get_str(in, "model", ""), json_obj_get(in, "pricing")));
        } else if (!strcmp(func, "credits_action")) {
            int keep_false = 0;
            json_t *kg = json_obj_get(in, "kg");
            if (kg && kg->type == JSON_BOOL && !kg->bool_val) keep_false = 1;
            char pmbuf[64] = "0";
            json_t *pm = json_obj_get(in, "pm");
            if (pm && pm->type == JSON_NUMBER) snprintf(pmbuf, sizeof pmbuf, "%.0f", pm->num_val);
            out = json_string(planlimits_credits_action(keep_false, pmbuf));
        } else if (!strcmp(func, "portal_wording")) {
            out = cstr(planlimits_portal_wording(json_get_str(in, "text", "")));
        } else if (!strcmp(func, "next_steps")) {
            json_t *tcj = json_obj_get(in, "topup_command");
            int tnull = tcj && tcj->type == JSON_NULL;
            out = cstr(planlimits_next_steps(json_obj_get(in, "limit"),
                        json_get_str(in, "model_command", "/model"),
                        tnull ? NULL : json_get_str(in, "topup_command", "/topup"),
                        tnull, json_get_bool(in, "portal", false) ? 1 : 0,
                        json_get_bool(in, "markup", false) ? 1 : 0));
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
