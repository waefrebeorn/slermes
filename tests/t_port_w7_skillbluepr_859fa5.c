/* t_port_w7_skillbluepr_859fa5.c — differential harness for cron/skill_blueprints.py. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w7_skillbluepr_859fa5.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *opt_str(const json_t *in, const char *key, bool *has) {
    const json_t *v = json_obj_get(in, key);
    if (!v || v->type == JSON_NULL) { *has = false; return NULL; }
    *has = true;
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

        if (!strcmp(func, "blueprint_to_job_spec")) {
            bool hp = false, hm = false, hprov = false, hn = false;
            const char *prompt = opt_str(in, "prompt", &hp);
            const char *model = opt_str(in, "model", &hm);
            const char *provider = opt_str(in, "provider", &hprov);
            const char *name = opt_str(in, "name", &hn);
            out = w7_skillbluepr_blueprint_to_job_spec(
                json_get_str(in, "skill_name", ""),
                json_get_str(in, "schedule", ""),
                json_get_str(in, "deliver", "origin"),
                hp, prompt ? prompt : "",
                json_get_bool(in, "no_agent", false),
                hm, model ? model : "",
                hprov, provider ? provider : "",
                json_obj_get(in, "enabled_toolsets"),
                hn, name ? name : "");
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
