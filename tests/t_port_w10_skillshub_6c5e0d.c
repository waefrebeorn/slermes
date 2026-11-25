/* t_port_w10_skillshub_6c5e0d.c -- JSON-lines harness for src/tools/skills_hub_models.c. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w10_skillshub_6c5e0d.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IOFBF, 1u << 20);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        if (!strcmp(func, "matches")) {
            out = w10_skillshub_6c5e0d_matches_query(json_get_str(in, "query", ""), json_obj_get(in, "fields"));
        } else if (!strcmp(func, "dedupe")) {
            out = w10_skillshub_6c5e0d_dedupe_by_trust(json_obj_get(in, "results"));
        } else if (!strcmp(func, "normpath")) {
            out = w10_skillshub_6c5e0d_normalize_bundle_path(json_get_str(in, "path", ""),
                    json_get_str(in, "field", "bundle file path"), json_get_bool(in, "nested", true) ? 1 : 0);
        } else if (!strcmp(func, "valname")) {
            out = w10_skillshub_6c5e0d_validate_skill_name(json_get_str(in, "name", ""));
        } else if (!strcmp(func, "valparent")) {
            out = w10_skillshub_6c5e0d_validate_install_parent_path(json_get_str(in, "category", ""));
        } else if (!strcmp(func, "valrel")) {
            out = w10_skillshub_6c5e0d_validate_bundle_rel_path(json_get_str(in, "rel_path", ""));
        } else if (!strcmp(func, "hermestags")) {
            out = w10_skillshub_6c5e0d_hermes_tags(json_obj_get(in, "fm"));
        } else if (!strcmp(func, "concrete")) {
            out = w10_skillshub_6c5e0d_query_is_concrete(json_get_str(in, "query", ""));
        } else if (!strcmp(func, "srcurl")) {
            out = w10_skillshub_6c5e0d_source_url_for_bundle(json_obj_get(in, "bundle"));
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
