#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_auxiliarye_905344.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        const json_t *v = json_obj_get(in, "verdict");
        const json_t *f = json_obj_get(in, "failure");
        int rung = json_get_bool(in, "rung_spent", 0) ? 1 : 0;
        json_t *out = NULL;
        if (!strcmp(func, "same_provider_retry"))
            out = json_bool(w15_auxiliarye_905344_same_provider_retry(v) ? 1 : 0);
        else if (!strcmp(func, "auth_refresh"))
            out = json_bool(w15_auxiliarye_905344_auth_refresh(v) ? 1 : 0);
        else if (!strcmp(func, "same_key_retry"))
            out = json_bool(w15_auxiliarye_905344_same_key_retry(v) ? 1 : 0);
        else if (!strcmp(func, "resend_cannot_help"))
            out = json_bool(w15_auxiliarye_905344_resend_cannot_help(v) ? 1 : 0);
        else if (!strcmp(func, "benches_provider"))
            out = json_bool(w15_auxiliarye_905344_benches_provider(v) ? 1 : 0);
        else if (!strcmp(func, "stream_negotiation_reraises"))
            out = json_bool(w15_auxiliarye_905344_stream_negotiation_reraises(v) ? 1 : 0);
        else if (!strcmp(func, "fallback_label"))
            out = w15_auxiliarye_905344_fallback_label(f, rung);
        else if (!strcmp(func, "ladder_accepts"))
            out = json_bool(w15_auxiliarye_905344_ladder_accepts(f, rung) ? 1 : 0);
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
