/* JSON-lines harness for the w28_manager_87eef2_p1 gate.
 * One request object per stdin line; one response line carrying the computed
 * "out" plus every input key echoed back. */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w28_manager_87eef2_p1.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}

static json_t *dispatch(const json_t *in, const char *func) {
    if (!strcmp(func, "validate_commit_hash"))
        return w28_manager_87eef2_p1_validate_commit_hash(json_get_str(in, "commit_hash", ""));
    if (!strcmp(func, "ref_name"))
        return w28_manager_87eef2_p1_ref_name(json_get_str(in, "dir_hash", ""));
    if (!strcmp(func, "gitlink_paths"))
        return w28_manager_87eef2_p1_gitlink_paths(json_obj_get(in, "records"));
    if (!strcmp(func, "uncaptured_note"))
        return w28_manager_87eef2_p1_uncaptured_note(json_obj_get(in, "nested_repos"));
    if (!strcmp(func, "project_hash"))
        return w28_manager_87eef2_p1_project_hash(json_get_str(in, "abs_path", ""));
    if (!strcmp(func, "store_path"))
        return w28_manager_87eef2_p1_store_path(json_get_str(in, "base", ""));
    if (!strcmp(func, "index_path"))
        return w28_manager_87eef2_p1_index_path(json_get_str(in, "store", ""),
                                                json_get_str(in, "dir_hash", ""));
    if (!strcmp(func, "ledger_path"))
        return w28_manager_87eef2_p1_ledger_path(json_get_str(in, "store", ""),
                                                 json_get_str(in, "dir_hash", ""));
    if (!strcmp(func, "meta_path"))
        return w28_manager_87eef2_p1_meta_path(json_get_str(in, "store", ""),
                                               json_get_str(in, "dir_hash", ""));
    if (!strcmp(func, "store_pathspecs"))
        return w28_manager_87eef2_p1_store_pathspecs(json_get_str(in, "base", ""),
                                                     json_get_str(in, "working_dir", ""));
    if (!strcmp(func, "exclude_store_args"))
        return w28_manager_87eef2_p1_exclude_store_args(json_get_str(in, "base", ""),
                                                        json_get_str(in, "working_dir", ""));
    if (!strcmp(func, "stage_all_args"))
        return w28_manager_87eef2_p1_stage_all_args(json_get_str(in, "base", ""),
                                                    json_get_str(in, "working_dir", ""));
    return json_string("UNKNOWN_FUNC");
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
        out = dispatch(in, func);
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
