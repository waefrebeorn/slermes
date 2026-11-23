/* t_port_w11_projecttre_cfc189.c — harness for the project_tree C port. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_projecttre_cfc189.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static json_t *own_str(char *s) {
    json_t *r = json_string(s ? s : "");
    free(s);
    return r;
}
static const char *sval(json_t *o, const char *k) {
    json_t *v = json_obj_get(o, k);
    return (v && v->type == JSON_STRING) ? v->str_val : NULL;
}
static bool bval(json_t *o, const char *k, bool d) {
    json_t *v = json_obj_get(o, k);
    return (v && v->type == JSON_BOOL) ? v->bool_val : d;
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
        if (!strcmp(func, "constants")) {
            out = w11_projecttre_cfc189_constants();
        } else if (!strcmp(func, "segments")) {
            out = w11_projecttre_cfc189_segments(sval(in, "path"));
        } else if (!strcmp(func, "base_name")) {
            out = own_str((char *)w11_projecttre_cfc189_base_name(sval(in, "path")));
        } else if (!strcmp(func, "path_key")) {
            out = own_str((char *)w11_projecttre_cfc189_path_key(sval(in, "path")));
        } else if (!strcmp(func, "lane_key")) {
            out = own_str((char *)w11_projecttre_cfc189_lane_key(sval(in, "path")));
        } else if (!strcmp(func, "branch_lane_id")) {
            out = own_str((char *)w11_projecttre_cfc189_branch_lane_id(
                sval(in, "root"), sval(in, "branch")));
        } else if (!strcmp(func, "kanban_lane_id")) {
            out = own_str((char *)w11_projecttre_cfc189_kanban_lane_id(sval(in, "root")));
        } else if (!strcmp(func, "is_windows_path")) {
            out = json_bool(w11_projecttre_cfc189_is_windows_path(sval(in, "path")));
        } else if (!strcmp(func, "kanban_worktree_dir")) {
            out = w11_projecttre_cfc189_kanban_worktree_dir(sval(in, "path"));
        } else if (!strcmp(func, "with_base_name")) {
            out = own_str((char *)w11_projecttre_cfc189_with_base_name(
                sval(in, "path"), sval(in, "name")));
        } else if (!strcmp(func, "field")) {
            out = own_str((char *)w11_projecttre_cfc189_field(
                json_obj_get(in, "row"), sval(in, "key")));
        } else if (!strcmp(func, "session_time")) {
            out = json_number(w11_projecttre_cfc189_session_time(json_obj_get(in, "session")));
        } else if (!strcmp(func, "last_active")) {
            out = json_number(w11_projecttre_cfc189_last_active(json_obj_get(in, "sessions")));
        } else if (!strcmp(func, "placement")) {
            out = w11_projecttre_cfc189_placement(
                sval(in, "repo_root"), sval(in, "lane_key"), sval(in, "lane_label"),
                sval(in, "lane_path"), bval(in, "is_main", false),
                bval(in, "is_kanban", false), bval(in, "is_git", true));
        } else if (!strcmp(func, "trunk_placement")) {
            out = w11_projecttre_cfc189_trunk_placement(sval(in, "root"), sval(in, "branch"));
        } else if (!strcmp(func, "kanban_placement")) {
            out = w11_projecttre_cfc189_kanban_placement(sval(in, "root"), sval(in, "kanban_dir"));
        } else if (!strcmp(func, "repo_node")) {
            out = w11_projecttre_cfc189_repo_node(sval(in, "root"), sval(in, "label"));
        } else if (!strcmp(func, "project_node")) {
            out = w11_projecttre_cfc189_project_node(
                sval(in, "pid"), sval(in, "label"), json_obj_get(in, "path"),
                json_obj_get(in, "repos"), json_get_num(in, "session_count", 0),
                json_get_num(in, "last_active", 0), json_obj_get(in, "previews"),
                json_obj_get(in, "rows"), json_obj_get(in, "flags"));
        } else if (!strcmp(func, "lane_sort_key")) {
            out = w11_projecttre_cfc189_lane_sort_key(json_obj_get(in, "group"));
        } else if (!strcmp(func, "disambiguate_labels")) {
            out = w11_projecttre_cfc189_disambiguate_labels(json_obj_get(in, "items"));
        } else if (!strcmp(func, "stamp_profile")) {
            out = w11_projecttre_cfc189_stamp_profile(json_obj_get(in, "projects"),
                                                      sval(in, "profile"));
        } else if (!strcmp(func, "place_by_heuristic")) {
            out = w11_projecttre_cfc189_place_by_heuristic(sval(in, "path"));
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
