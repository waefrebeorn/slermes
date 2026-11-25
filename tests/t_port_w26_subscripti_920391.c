/* JSON-lines harness for the w26_subscripti_920391 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_subscripti_920391.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *err_named(const char *name) {
    json_t *o = json_object();
    json_set(o, "error", json_string(name));
    return o;
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "expected_client_state")) {
            out = w26_subscripti_920391_expected_client_state(json_obj_get(in, "raw"));
        } else if (!strcmp(func, "utc_timestamp")) {
            json_t *h = json_obj_get(in, "hours_from_now");
            int bad = h && h->type != JSON_NUMBER && h->type != JSON_BOOL && h->type != JSON_NULL;
            out = bad ? err_named("TypeError")
                      : w26_subscripti_920391_utc_timestamp(h, json_obj_get(in, "base"));
        } else if (!strcmp(func, "is_managed_subscription")) {
            json_t *payload = json_obj_get(in, "payload");
            if (!payload || payload->type != JSON_OBJECT) {
                out = err_named("AttributeError");
            } else {
                out = w26_subscripti_920391_is_managed_subscription(
                    json_obj_get(in, "known_ids"), payload,
                    json_obj_get(in, "expected_client_state_value"));
            }
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
