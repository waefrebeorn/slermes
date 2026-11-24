/* JSON-lines harness for the w20_economics_90c7dd gate.
 * One request object per stdin line; one response per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w20_economics_90c7dd.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *str_or(json_t *in, const char *key, const char *def) {
    return json_get_str(in, key, def);
}

static long long ll_or(json_t *in, const char *key, long long def) {
    const char *s = json_get_str(in, key, NULL);
    if (!s) return def;
    return strtoll(s, NULL, 10);
}

static json_t *finish(json_t *in, const char *func, json_t *out) {
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
    return out;
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
        if (!strcmp(func, "evaluate")) {
            w20_economics_90c7dd_decision d;
            int ok = w20_economics_90c7dd_evaluate(
                json_obj_get(in, "pricing"),
                (int64_t)ll_or(in, "cached_tokens", 0),
                (int64_t)ll_or(in, "uncached_tokens", 0),
                json_get_num(in, "spent_usd", 0.0),
                json_get_num(in, "observed_warm_usd", 0.0),
                (int64_t)ll_or(in, "output_tokens", 1),
                &d);
            out = ok ? w20_economics_90c7dd_decision_json(&d) : json_null();
        } else if (!strcmp(func, "per_token")) {
            char buf[64];
            double v = w20_economics_90c7dd_per_token(json_obj_get(in, "rate"));
            snprintf(buf, sizeof(buf), "%.17g", v);
            out = json_string(buf);
        } else if (!strcmp(func, "stop_reason")) {
            w20_economics_90c7dd_decision d;
            memset(&d, 0, sizeof(d));
            d.economics_available = json_get_bool(in, "economics_available", 0) ? 1 : 0;
            d.within_miss = json_get_bool(in, "within_miss", 1) ? 1 : 0;
            out = json_string(w20_economics_90c7dd_stop_reason(&d));
        } else {
            out = json_string("UNKNOWN_FUNC");
        }
        finish(in, func, out);
        json_free(out);
        json_free(in);
    }
    return 0;
}
