/* Port gate harness for hermes_cli/fs_utils.py (gate w7_fsutils_903462). */
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "noc_w7_fsutils_903462.h"
#include "hermes_cli/fs_utils.c"

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
static void ensure_parent(const char *path) {
    char buf[1024];
    strncpy(buf, path, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    for (char *p = buf + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char save = *p; *p = '\0';
            if (*buf) mkdir(buf, 0700);
            *p = save;
        }
    }
}
static int in_list(const json_t *list, const char *s) {
    if (!list || list->type != JSON_ARRAY) return 0;
    for (size_t i = 0; i < list->c.count; i++) {
        json_t *v = list->c.items[i];
        if (v && v->type == JSON_STRING && v->str_val && !strcmp(v->str_val, s)) return 1;
    }
    return 0;
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
        if (!strcmp(func, "rmtree_force")) {
            const char *path = jstr(in, "path");
            json_t *files = json_obj_get(in, "files");
            json_t *ro = json_obj_get(in, "readonly");
            /* Start from a clean slate (mirrors the oracle's rmtree(ignore_errors)). */
            w7_fsutils_rmtree_force(path, NULL);
            if (files && files->type == JSON_ARRAY) {
                for (size_t i = 0; i < files->c.count; i++) {
                    json_t *f = files->c.items[i];
                    if (!f || f->type != JSON_STRING || !f->str_val) continue;
                    char full[1024];
                    snprintf(full, sizeof(full), "%s/%s", path, f->str_val);
                    ensure_parent(full);
                    FILE *fp = fopen(full, "wb");
                    if (fp) { fputc('x', fp); fclose(fp); }
                    if (in_list(ro, f->str_val)) chmod(full, 0400);
                }
            } else {
                mkdir(path, 0700);
            }
            int gone = w7_fsutils_rmtree_force(path, ro);
            out = json_bool(gone ? true : false);
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
