/* JSON-lines harness for the w26_gatewaymul_37308a gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back.
 *
 * The gateway's live-PID proof and its gateway_state.json reader are the seams:
 * "live_pid" (0/absent/null = no verified live gateway) and "runtime" (the
 * record) stand in for them, exactly as the upstream callers receive them.
 */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w26_gatewaymul_37308a.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

/* live_default_gateway_pid(): a verified pid is truthy; None/0 is not live. */
static int live_of(const json_t *in) {
    const json_t *v = json_obj_get(in, "live_pid");
    if (!v) return 0;
    if (v->type == JSON_NUMBER) return v->num_val != 0.0;
    if (v->type == JSON_BOOL) return v->bool_val ? 1 : 0;
    return 0;
}

static const char *text_of(const json_t *in, const char *key, const char *fallback) {
    const json_t *v = json_obj_get(in, key);
    if (v && v->type == JSON_STRING && v->str_val) return v->str_val;
    return fallback;
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out;
        const char *func;
        int live;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        live = live_of(in);
        if (!strcmp(func, "served_profiles")) {
            out = w26_gatewaymul_37308a_served_profiles(json_obj_get(in, "runtime"), live);
        } else if (!strcmp(func, "secondaries")) {
            out = w26_gatewaymul_37308a_secondaries(json_obj_get(in, "runtime"), live);
        } else if (!strcmp(func, "unserved_platforms")) {
            out = w26_gatewaymul_37308a_unserved_platforms(json_obj_get(in, "runtime"), live,
                                                           text_of(in, "profile", ""));
        } else if (!strcmp(func, "ingress_urls")) {
            out = w26_gatewaymul_37308a_ingress_urls(json_obj_get(in, "runtime"), live,
                                                     text_of(in, "profile", ""));
        } else if (!strcmp(func, "format_lines")) {
            out = w26_gatewaymul_37308a_format_lines(json_obj_get(in, "urls"),
                                                     text_of(in, "indent", "  "));
        } else if (!strcmp(func, "constants")) {
            out = w26_gatewaymul_37308a_constants();
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
