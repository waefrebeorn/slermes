#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w16_terminalto_f0d0d6.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static const char *sget(const json_t *o, const char *k) { return json_get_str(o, k, ""); }
int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *a = json_obj_get(in, "in");
        json_t *cfg = json_obj_get(a, "config");
        json_t *out = NULL;
        if (!strcmp(func, "ssh_config")) out = w16_terminalto_f0d0d6_ssh_config(cfg);
        else if (!strcmp(func, "local_config")) out = w16_terminalto_f0d0d6_local_config(cfg);
        else if (!strcmp(func, "container_config")) out = w16_terminalto_f0d0d6_container_config(cfg);
        else if (!strcmp(func, "resources")) out = w16_terminalto_f0d0d6_resources(cfg);
        else if (!strcmp(func, "is_supported"))
            out = w16_terminalto_f0d0d6_is_supported_vercel_runtime(sget(a, "runtime"));
        else if (!strcmp(func, "check_vercel"))
            out = w16_terminalto_f0d0d6_check_vercel(
                cfg, json_get_bool(a, "vercel_installed", false) ? 1 : 0,
                json_obj_get(a, "secrets"));
        else if (!strcmp(func, "ssh_pre")) out = w16_terminalto_f0d0d6_ssh_pre(cfg);
        else out = json_string("UNKNOWN_FUNC");
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
