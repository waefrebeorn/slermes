/* JSON-lines harness for the w20_webhookcoa_e2fcee gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w20_webhookcoa_e2fcee.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *take_str(char *s) {
    json_t *out = s ? json_string(s) : json_null();
    free(s);
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
        if (!strcmp(func, "validate")) {
            out = take_str(w20_webhookcoa_e2fcee_validate(json_get_str(in, "route_name", ""),
                                                          json_obj_get(in, "route")));
        } else if (!strcmp(func, "prompt_with_note")) {
            out = take_str(w20_webhookcoa_e2fcee_prompt_with_note(json_obj_get(in, "event")));
        } else if (!strcmp(func, "placeholder_present")) {
            out = json_bool(w20_webhookcoa_e2fcee_placeholder_present(
                                json_get_str(in, "text", "")) ? 1 : 0);
        } else if (!strcmp(func, "group_key")) {
            out = take_str(w20_webhookcoa_e2fcee_group_key(json_get_str(in, "route_name", ""),
                                                           json_get_str(in, "rendered", "")));
        } else if (!strcmp(func, "normalize_template")) {
            out = take_str(w20_webhookcoa_e2fcee_normalize_template(
                               json_get_str(in, "key_template", "")));
        } else if (!strcmp(func, "enqueue_plan")) {
            out = w20_webhookcoa_e2fcee_enqueue_plan(json_obj_get(in, "spec"));
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
