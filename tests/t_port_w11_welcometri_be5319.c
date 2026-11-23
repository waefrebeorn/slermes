#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_welcometri_be5319.h"
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
        int af = json_get_bool(in, "assume_fresh", false) ? 1 : 0;
        int local = json_get_bool(in, "local_backend", false) ? 1 : 0;
        int guest = json_get_bool(in, "guest_allowed", false) ? 1 : 0;
        int nous = json_get_bool(in, "has_nous_identity", false) ? 1 : 0;
        int prov = json_get_bool(in, "provider_configured", false) ? 1 : 0;
        int machine = json_get_bool(in, "machine_existing", false) ? 1 : 0;
        int reserved = json_get_bool(in, "trial_reserved", false) ? 1 : 0;
        json_t *out = NULL;
        if (!strcmp(func, "claim_blocker")) {
            const char *b = w11_welcometri_be5319_claim_blocker(af, json_obj_get(in, "configured"),
                        local, guest, nous, prov, machine, reserved);
            out = b ? json_string(b) : NULL;
        } else if (!strcmp(func, "inference_available")) {
            int free = json_get_bool(in, "free_inference_active", false) ? 1 : 0;
            out = json_bool(w11_welcometri_be5319_inference_available(af, free,
                        local, guest, nous, prov, machine, reserved) ? true : false);
        }
        printf("{"); emit_key("func", json_string(func)); fputs(",", stdout); emit_key("out", out);
        for (size_t i = 0; i < json_object_size(in); i++) {
            const char *k = json_object_get_key_at(in, i);
            if (!k || !strcmp(k, "func")) continue;
            fputs(",", stdout); emit_key(k, json_copy(json_object_get_at(in, i)));
        }
        fputs("}\n", stdout);
        json_free(out); json_free(in);
    }
    return 0;
}
