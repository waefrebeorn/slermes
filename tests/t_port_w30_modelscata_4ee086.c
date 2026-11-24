/* JSON-lines harness for the w30_modelscata_4ee086 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w30_modelscata_4ee086.h"

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
        json_t *in, *out;
        const char *func;
        if (!strchr(line, '{')) continue;
        in = json_parse(line, NULL);
        if (!in) continue;
        func = json_get_str(in, "func", "");
        if (!strcmp(func, "constants")) {
            out = w30_modelscata_4ee086_constants();
        } else if (!strcmp(func, "xai_promote_top")) {
            out = w30_modelscata_4ee086_xai_promote_top(json_obj_get(in, "ids"));
        } else if (!strcmp(func, "xai_merge_curated_extras")) {
            out = w30_modelscata_4ee086_xai_merge_curated_extras(json_obj_get(in, "ids"));
        } else if (!strcmp(func, "xai_finalize_catalog")) {
            out = w30_modelscata_4ee086_xai_finalize_catalog(json_obj_get(in, "ids"));
        } else if (!strcmp(func, "provider_group_for_slug")) {
            out = w30_modelscata_4ee086_provider_group_for_slug(json_obj_get(in, "slug"));
        } else if (!strcmp(func, "group_providers")) {
            out = w30_modelscata_4ee086_group_providers(json_obj_get(in, "slugs"));
        } else if (!strcmp(func, "plugin_provider_enters_picker")) {
            out = w30_modelscata_4ee086_plugin_provider_enters_picker(json_obj_get(in, "name"));
        } else if (!strcmp(func, "provider_models")) {
            out = w30_modelscata_4ee086_provider_models(json_obj_get(in, "slug"));
        } else if (!strcmp(func, "provider_label")) {
            out = w30_modelscata_4ee086_provider_label(json_obj_get(in, "slug"));
        } else if (!strcmp(func, "provider_alias")) {
            out = w30_modelscata_4ee086_provider_alias(json_obj_get(in, "key"));
        } else if (!strcmp(func, "canonical_provider_row")) {
            out = w30_modelscata_4ee086_canonical_provider_row(json_obj_get(in, "slug"));
        } else if (!strcmp(func, "group_members")) {
            out = w30_modelscata_4ee086_group_members(json_obj_get(in, "gid"));
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
