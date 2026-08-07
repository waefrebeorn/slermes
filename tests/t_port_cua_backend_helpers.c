/*
 * Oracle harness for tools/computer_use/cua_backend.py _wsl_windows_path_to_posix.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libjson/json.h"
#include "cua_backend_helpers.h"

int main(int argc, char **argv) {
    if (argc < 2) return 1;
    char *err = NULL;
    json_t *root = json_parse_file(argv[1], &err);
    if (err) { free(err); return 1; }
    if (!root || root->type != JSON_ARRAY) {
        if (root) json_free(root);
        return 1;
    }
    for (size_t i = 0; i < root->c.count; i++) {
        json_t *c = json_get(root, i);
        json_t *vj = json_obj_get(c, "value");
        const char *path = vj && vj->type == JSON_STRING ? vj->str_val : "";
        json_t *wj = json_obj_get(c, "is_wsl");
        int is_wsl = 0;
        if (wj) {
            if (wj->type == JSON_BOOL) is_wsl = wj->bool_val;
            else if (wj->type == JSON_NUMBER) is_wsl = (int)vj->num_val;
        }
        char *r = cua_wsl_windows_path_to_posix(path, is_wsl);
        printf("%s\n", r ? r : "null");
        free(r);
    }
    json_free(root);
    return 0;
}
