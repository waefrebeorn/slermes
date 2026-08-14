/*
 * t_port_profiles_tree.c — oracle harness for the pure project-tree merge
 * helpers of hermes_cli/web_routers/profiles.py ported to C.
 *
 * Reads a JSON fixture (argv[1]) of cases, runs the C functions, emits one JSON
 * object per case. The Python oracle recomputes from LIVE profiles.py.
 *
 * Tested: _merge_by_id, _merge_profile_tree, _pr_url_from_tool_output.
 */
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "hermes_json.h"
#include "libjson/json.h"

extern void ws_profiles_merge_by_id(json_t *into, json_t *entries, const char *child_key);
extern void ws_profiles_merge_profile_tree(json_t *merged, json_t *projects,
                                           const char *profile, int preview_limit);
extern json_t *ws_profiles_pr_url_from_tool_output(const char *content);

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t rd = fread(buf, 1, sz, f);
    buf[rd] = '\0';
    fclose(f);
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: t_port_profiles_tree <cases.in>\n"); return 1; }
    char *text = read_file(argv[1]);
    if (!text) { fprintf(stderr, "cannot read %s\n", argv[1]); return 1; }
    char *err = NULL;
    json_t *cases = json_parse(text, &err);
    free(text);
    if (err || !cases || cases->type != JSON_ARRAY) { if (err) free(err); fprintf(stderr, "bad fixture\n"); return 1; }

    for (size_t i = 0; i < cases->c.count; i++) {
        json_t *c = cases->c.items[i];
        const char *op = json_get_str(c, "op", "");

        if (strcmp(op, "merge_by_id") == 0) {
            json_t *into = json_parse(json_get_str(c, "into", "{}"), NULL);
            json_t *entries = json_parse(json_get_str(c, "entries", "[]"), NULL);
            const char *child = json_get_str(c, "child_key", "groups");
            ws_profiles_merge_by_id(into, entries, child);
            char *out = json_serialize(into);
            printf("{\"op\":\"merge_by_id\",\"out\":%s}\n", out ? out : "null");
            free(out);
            json_free(into); json_free(entries);
        } else if (strcmp(op, "merge_profile_tree") == 0) {
            json_t *merged = json_parse(json_get_str(c, "merged", "{}"), NULL);
            json_t *projects = json_parse(json_get_str(c, "projects", "[]"), NULL);
            const char *profile = json_get_str(c, "profile", "default");
            int limit = (int)json_get_num(c, "preview_limit", 3);
            ws_profiles_merge_profile_tree(merged, projects, profile, limit);
            char *out = json_serialize(merged);
            printf("{\"op\":\"merge_profile_tree\",\"out\":%s}\n", out ? out : "null");
            free(out);
            json_free(merged); json_free(projects);
        } else if (strcmp(op, "pr_url") == 0) {
            const char *content = json_get_str(c, "arg", "");
            json_t *r = ws_profiles_pr_url_from_tool_output(content);
            if (r) {
                char *out = json_serialize(r);
                printf("{\"op\":\"pr_url\",\"out\":%s}\n", out ? out : "null");
                free(out);
                json_free(r);
            } else {
                printf("{\"op\":\"pr_url\",\"out\":null}\n");
            }
        } else {
            printf("{\"op\":\"unknown\"}\n");
        }
    }
    json_free(cases);
    return 0;
}
