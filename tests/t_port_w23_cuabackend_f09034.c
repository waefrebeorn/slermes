/* JSON-lines harness for the w23_cuabackend_f09034 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w23_cuabackend_f09034.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
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
        if (!strcmp(func, "outcome_unknown")) {
            out = w23_cuabackend_f09034_outcome_unknown(in);
        } else if (!strcmp(func, "logical_error_text")) {
            char *s = w23_cuabackend_f09034_logical_error_text(json_obj_get(in, "error"));
            out = json_string(s ? s : "");
            free(s);
        } else if (!strcmp(func, "is_ended_session_result")) {
            out = json_bool(w23_cuabackend_f09034_is_ended_session_result(
                                json_obj_get(in, "result")) != 0);
        } else if (!strcmp(func, "is_closed_session_error")) {
            out = json_bool(w23_cuabackend_f09034_is_closed_session_error(in) != 0);
        } else if (!strcmp(func, "is_transient_daemon_error")) {
            out = json_bool(w23_cuabackend_f09034_is_transient_daemon_error(
                                json_get_str(in, "message", "")) != 0);
        } else if (!strcmp(func, "supports_capability")) {
            out = json_bool(w23_cuabackend_f09034_supports_capability(in) != 0);
        } else if (!strcmp(func, "supports_input_property")) {
            out = json_bool(w23_cuabackend_f09034_supports_input_property(in) != 0);
        } else if (!strcmp(func, "mcp_field")) {
            out = w23_cuabackend_f09034_mcp_field(json_obj_get(in, "obj"),
                                                  json_get_str(in, "snake", NULL),
                                                  json_get_str(in, "camel", NULL));
        } else if (!strcmp(func, "tool_field")) {
            out = w23_cuabackend_f09034_tool_field(json_obj_get(in, "obj"),
                                                   json_get_str(in, "snake", NULL),
                                                   json_get_str(in, "camel", NULL));
        } else if (!strcmp(func, "cli_attempts")) {
            out = json_number((double)w23_cuabackend_f09034_cli_attempts());
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
