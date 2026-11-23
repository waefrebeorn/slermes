#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w15_updatecmd_05d181.h"

/* Build a NUL-joined porcelain -z buffer from a JSON string array. */
static char *join_z(const json_t *arr, size_t *len_out) {
    *len_out = 0;
    if (!arr || arr->type != JSON_ARRAY) return NULL;
    size_t total = 0;
    for (size_t i = 0; i < arr->c.count; i++) {
        json_t *v = json_get(arr, i);
        if (v && v->type == JSON_STRING && v->str_val) total += strlen(v->str_val);
        if (i + 1 < arr->c.count) total++;
    }
    char *buf = (char *)malloc(total + 1);
    if (!buf) return NULL;
    size_t p = 0;
    for (size_t i = 0; i < arr->c.count; i++) {
        if (i) buf[p++] = '\0';
        json_t *v = json_get(arr, i);
        if (v && v->type == JSON_STRING && v->str_val) {
            size_t n = strlen(v->str_val);
            memcpy(buf + p, v->str_val, n);
            p += n;
        }
    }
    buf[p] = '\0';
    *len_out = p;
    return buf;
}
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
        char *z = NULL;
        size_t zlen = 0;
        if (!strcmp(func, "intent_to_add_paths")) {
            z = join_z(json_obj_get(in, "fields"), &zlen);
            out = w15_updatecmd_05d181_intent_to_add_paths(z ? z : "", zlen);
        } else if (!strcmp(func, "porcelain_records")) {
            z = join_z(json_obj_get(in, "fields"), &zlen);
            out = w15_updatecmd_05d181_porcelain_records(z ? z : "", zlen);
        } else if (!strcmp(func, "untracked_collides")) {
            out = w15_updatecmd_05d181_untracked_collides(
                json_get_str(in, "entry", ""),
                json_obj_get(in, "files"), json_obj_get(in, "dirs"));
        } else if (!strcmp(func, "stash_apply_failed_only_on_existing_untracked")) {
            out = w15_updatecmd_05d181_stash_apply_failed_only_on_existing_untracked(
                json_get_str(in, "stderr", ""));
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
        free(z);
        json_free(out);
        json_free(in);
    }
    return 0;
}
