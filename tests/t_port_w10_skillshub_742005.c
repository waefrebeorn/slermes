/* t_port_w10_skillshub_742005.c -- JSON-lines harness for src/tools/skills_hub_github.c. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w10_skillshub_742005.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
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
        if (!strcmp(func, "provider")) {
            out = w10_skillshub_742005_github_provider_for(json_get_str(in, "repo", ""));
        } else if (!strcmp(func, "provfilter")) {
            out = w10_skillshub_742005_provider_filter_of(json_get_str(in, "source", ""));
        } else if (!strcmp(func, "cachekey")) {
            out = w10_skillshub_742005_tap_cache_key(json_get_str(in, "repo", ""),
                    json_get_str(in, "path", ""), json_get_str(in, "bucket", ""));
        } else if (!strcmp(func, "splitid")) {
            out = w10_skillshub_742005_split_repo_id(json_get_str(in, "identifier", ""));
        } else if (!strcmp(func, "skillpath")) {
            out = w10_skillshub_742005_skill_file_path(json_get_str(in, "skill_path", ""),
                    json_get_str(in, "filename", "SKILL.md"));
        } else if (!strcmp(func, "skipbundle")) {
            out = w10_skillshub_742005_skip_bundle_file(json_get_str(in, "rel_path", ""));
        } else if (!strcmp(func, "ratelimit")) {
            out = w10_skillshub_742005_is_rate_limit_response(
                    (int)json_get_num(in, "status_code", 0.0), json_obj_get(in, "headers"));
        } else if (!strcmp(func, "treemembers")) {
            out = w10_skillshub_742005_tree_members(json_obj_get(in, "entries"),
                    json_get_str(in, "prefix", ""));
        } else if (!strcmp(func, "groupings")) {
            out = w10_skillshub_742005_parse_skillsh_groupings(json_get_str(in, "content", ""));
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
