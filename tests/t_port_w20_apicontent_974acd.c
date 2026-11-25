/* JSON-lines harness for the w20_apicontent_974acd gate.
 * One request object per stdin line; one response object per line carrying the
 * computed "out" plus every input key echoed back. Missing keys never crash. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w20_apicontent_974acd.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

/* Optional text: absent or non-string reads as None (NULL). */
static const char *opt_str(json_t *in, const char *key) {
    json_t *v = json_obj_get(in, key);
    if (!v || v->type != JSON_STRING) return NULL;
    return v->str_val;
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        json_t *in, *out = NULL;
        const char *func;
        int rowcount;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        rowcount = (int)json_get_num(in, "rowcount", 1.0);
        if (!strcmp(func, "plan_latest")) {
            out = w20_apicontent_974acd_plan_latest(opt_str(in, "session_id"),
                                                    json_obj_get(in, "content"),
                                                    opt_str(in, "api_content"), rowcount);
        } else if (!strcmp(func, "plan_row")) {
            out = w20_apicontent_974acd_plan_row(opt_str(in, "session_id"),
                                                 json_obj_get(in, "row_id"),
                                                 json_obj_get(in, "content"),
                                                 opt_str(in, "api_content"), rowcount);
        } else if (!strcmp(func, "encode_content")) {
            out = w20_apicontent_974acd_encode_content(json_obj_get(in, "content"));
        } else if (!strcmp(func, "latest_sql")) {
            out = json_string(w20_apicontent_974acd_latest_sql());
        } else if (!strcmp(func, "row_sql")) {
            out = json_string(w20_apicontent_974acd_row_sql());
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
