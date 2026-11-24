/* JSON-lines harness for the w28_hostedroom_3fa737_p3 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w28_hostedroom_3fa737_p3.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *jval(const json_t *in, const char *key) { return json_obj_get(in, key); }

static double jnum(const json_t *in, const char *key, double def) {
    const json_t *v = json_obj_get(in, key);
    return (v && v->type == JSON_NUMBER) ? v->num_val : def;
}

static long long jll(const json_t *in, const char *key, long long def) {
    const json_t *v = json_obj_get(in, key);
    return (v && v->type == JSON_NUMBER) ? (long long)v->num_val : def;
}

static int jbool(const json_t *in, const char *key) {
    const json_t *v = json_obj_get(in, key);
    if (!v) return 0;
    switch (v->type) {
    case JSON_BOOL:   return v->bool_val ? 1 : 0;
    case JSON_NUMBER: return v->num_val != 0.0;
    case JSON_STRING: return v->str_val && v->str_val[0] != '\0';
    case JSON_ARRAY:  return json_len(v) > 0;
    case JSON_OBJECT: return json_object_size(v) > 0;
    default:          return 0;
    }
}

int main(void) {
    char line[262144];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "retry_delay")) {
            out = w28_hostedroom_3fa737_p3_retry_delay(jval(in, "previous"), jnum(in, "lo", 0),
                                                       jnum(in, "hi", 0), jnum(in, "now", 0));
        } else if (!strcmp(func, "renew_decision")) {
            out = w28_hostedroom_3fa737_p3_renew_decision(jval(in, "lease"), jval(in, "known"),
                                                          jnum(in, "now", 0), jnum(in, "ttl", 0),
                                                          jbool(in, "force"));
        } else if (!strcmp(func, "cache_lease_keep")) {
            out = w28_hostedroom_3fa737_p3_cache_lease_keep(jval(in, "known"),
                                                            jval(in, "incoming"));
        } else if (!strcmp(func, "ambiguous_expiry")) {
            out = w28_hostedroom_3fa737_p3_ambiguous_expiry(jnum(in, "attempt_expires_at", 0),
                                                            jval(in, "known"),
                                                            (int)jll(in, "attempt_generation", 0));
        } else if (!strcmp(func, "deadline_settlement")) {
            out = w28_hostedroom_3fa737_p3_deadline_settlement(jval(in, "task"),
                                                               jnum(in, "timeout", 0));
        } else if (!strcmp(func, "cycle_plan")) {
            out = w28_hostedroom_3fa737_p3_cycle_plan(jll(in, "cursor", 0), jval(in, "rooms"),
                                                      jval(in, "active"),
                                                      jll(in, "max_concurrent", 0),
                                                      jbool(in, "stop"));
        } else if (!strcmp(func, "approval_action")) {
            out = w28_hostedroom_3fa737_p3_approval_action(jval(in, "spec"));
        } else if (!strcmp(func, "validate_config")) {
            out = w28_hostedroom_3fa737_p3_validate_config(jval(in, "cfg"));
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
