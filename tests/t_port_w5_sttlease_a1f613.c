#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w5_sttlease_a1f613.h"

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
        if (!strcmp(func, "warm")) {
            out = w5_sttlease_a1f613_warm_stt_provider_ex(
                /* provider_arg */ json_has(in, "provider_arg") ? json_get_str(in, "provider_arg", "") : NULL,
                /* config_provider */ json_has(in, "config_provider") ? json_get_str(in, "config_provider", "") : NULL,
                json_get_bool(in, "has_fw", false) ? 1 : 0,
                json_get_bool(in, "lazy_install_ok", false) ? 1 : 0,
                json_get_str(in, "model_name", "base"),
                json_get_bool(in, "hot_before", false) ? 1 : 0,
                json_get_bool(in, "load_ok", false) ? 1 : 0,
                json_has(in, "load_error") ? json_get_str(in, "load_error", "") : NULL,
                (long long)json_get_num(in, "elapsed_ms", 0.0));
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
