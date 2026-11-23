#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_cmdsink.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static int streq_nullable(const char *a, const char *b) {
    if (a == b) return 1;
    if (!a || !b) return 0;
    return !strcmp(a, b);
}

static const char *elem_str(json_t *v) {
    return (v && v->type == JSON_STRING && v->str_val) ? v->str_val : "";
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "scenario")) {
            json_t *notices = json_obj_get(in, "notices");
            json_t *pv = json_obj_get(in, "prompt");
            json_t *dv = json_obj_get(in, "display");
            const char *prompt = (pv && pv->type == JSON_STRING) ? pv->str_val : NULL;
            const char *display = (dv && dv->type == JSON_STRING) ? dv->str_val : NULL;

            int outside_none = (cmdsink_depth() == 0);
            cmdsink_capture();
            if (notices && notices->type == JSON_ARRAY) {
                for (size_t i = 0; i < json_len(notices); ++i)
                    cmdsink_add_notice(elem_str(json_get(notices, i)));
            }
            cmdsink_set_prompt(prompt);
            cmdsink_set_display(display);

            cmdsink_sink_t s;
            int active_present = cmdsink_active(&s);
            int inside_depth = cmdsink_depth();
            json_t *inside = json_object();
            json_t *na = json_array();
            for (size_t i = 0; i < s.notices; ++i) {
                const char *n = cmdsink_notice_at(i);
                json_append(na, json_string(n ? n : ""));
            }
            json_set(inside, "notices", na);
            json_set(inside, "prompt", s.prompt ? json_string(s.prompt) : json_null());
            json_set(inside, "display", s.display ? json_string(s.display) : json_null());

            cmdsink_capture();
            cmdsink_sink_t s2;
            cmdsink_active(&s2);
            int nested_fresh = (s2.notices == 0 && s2.prompt == NULL && s2.display == NULL);
            int nested_depth = cmdsink_depth();
            cmdsink_release();
            cmdsink_sink_t s3;
            cmdsink_active(&s3);
            int restored = (s3.notices == s.notices &&
                            streq_nullable(s3.prompt, s.prompt) &&
                            streq_nullable(s3.display, s.display));
            cmdsink_release();
            int after_none = (cmdsink_depth() == 0);

            out = json_object();
            json_set(out, "outside_none", json_bool(outside_none ? 1 : 0));
            json_set(out, "inside", inside);
            json_set(out, "active_present", json_bool(active_present ? 1 : 0));
            json_set(out, "inside_depth", json_int(inside_depth));
            json_set(out, "nested_fresh", json_bool(nested_fresh ? 1 : 0));
            json_set(out, "nested_depth", json_int(nested_depth));
            json_set(out, "restored", json_bool(restored ? 1 : 0));
            json_set(out, "after_none", json_bool(after_none ? 1 : 0));
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
