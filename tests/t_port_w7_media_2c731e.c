/* Port gate harness for session_rules/media.py (gate w7_media_2c731e). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w7_media_2c731e.h"
#include "session_rules/media.c"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static const char *jstr(const json_t *in, const char *k) {
    json_t *v = json_obj_get(in, k);
    return (v && v->type == JSON_STRING) ? v->str_val : "";
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
        json_t *texts = json_obj_get(in, "texts");
        if (!strcmp(func, "hosted")) {
            out = w7_media_hosted_document_references(jstr(in, "request_id"),
                        json_obj_get(in, "payload"), jstr(in, "media_root"));
        } else if (!strcmp(func, "admission")) {
            out = w7_media_admission_media_references(json_obj_get(in, "payload"));
        } else if (!strcmp(func, "archive")) {
            out = w7_media_archive_refs_in_text(texts);
        } else if (!strcmp(func, "retirement")) {
            out = w7_media_retirement_candidates_in_text(texts);
        } else if (!strcmp(func, "spillover")) {
            out = w7_media_spillover_names_in_text(texts);
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
