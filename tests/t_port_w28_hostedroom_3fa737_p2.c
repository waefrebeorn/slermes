/* JSON-lines harness for the w28_hostedroom_3fa737_p2 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w28_hostedroom_3fa737_p2.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *jstr(const json_t *in, const char *key, const char *def) {
    const json_t *v = json_obj_get(in, key);
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : def;
}

static json_t *jval(const json_t *in, const char *key) { return json_obj_get(in, key); }

static double jnum(const json_t *in, const char *key, double def) {
    const json_t *v = json_obj_get(in, key);
    return (v && v->type == JSON_NUMBER) ? v->num_val : def;
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
        func = jstr(in, "func", "");
        if (!strcmp(func, "info_active")) {
            out = w28_hostedroom_3fa737_p2_info_active(jval(in, "info"));
        } else if (!strcmp(func, "info_is_active_for")) {
            out = w28_hostedroom_3fa737_p2_info_is_active_for(jval(in, "info"),
                                                              jstr(in, "task_id", ""),
                                                              jbool(in, "require_exact"));
        } else if (!strcmp(func, "peer_stop_acknowledged")) {
            out = w28_hostedroom_3fa737_p2_peer_stop_acknowledged(jval(in, "info"),
                                                                  jval(in, "task"),
                                                                  jbool(in, "has_session"));
        } else if (!strcmp(func, "interrupt_ack")) {
            out = w28_hostedroom_3fa737_p2_interrupt_ack(jval(in, "result"));
        } else if (!strcmp(func, "discard_guard")) {
            out = w28_hostedroom_3fa737_p2_discard_guard(jval(in, "info"), jval(in, "task"));
        } else if (!strcmp(func, "discard_outcome")) {
            out = w28_hostedroom_3fa737_p2_discard_outcome(jval(in, "result"), jval(in, "task"));
        } else if (!strcmp(func, "stop_outcome")) {
            out = w28_hostedroom_3fa737_p2_stop_outcome(jval(in, "spec"));
        } else if (!strcmp(func, "past_execution_deadline")) {
            out = w28_hostedroom_3fa737_p2_past_execution_deadline(jval(in, "task"),
                                                                   jnum(in, "now", 0),
                                                                   jnum(in, "timeout", 0));
        } else if (!strcmp(func, "defer_unconfirmed_guard")) {
            out = w28_hostedroom_3fa737_p2_defer_unconfirmed_guard(jval(in, "task"),
                                                                   jnum(in, "now", 0),
                                                                   jnum(in, "defer_seconds", 0),
                                                                   jbool(in, "served_elsewhere"));
        } else if (!strcmp(func, "deadline_id")) {
            out = w28_hostedroom_3fa737_p2_deadline_id(jval(in, "task"));
        } else if (!strcmp(func, "reconcile_deadline")) {
            out = w28_hostedroom_3fa737_p2_reconcile_deadline(jval(in, "task"),
                                                              jnum(in, "now", 0),
                                                              jnum(in, "defer_seconds", 0));
        } else if (!strcmp(func, "retry_route")) {
            out = w28_hostedroom_3fa737_p2_retry_route(jval(in, "task"),
                                                       jval(in, "requested_generation"),
                                                       jbool(in, "has_binding"));
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
