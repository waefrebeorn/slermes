/* Oracle harness for tools/skill_provenance.py port.
 * Verifies the EXISTING port in lib/libskillusage/skill_provenance.c against
 * the live Python module. Reads fixture (argv[1]): {"ops":[...]}.
 *
 * NOTE: the existing C port uses a process-global origin (not _Thread_local),
 * while Python's ContextVar is per-context. For a single sequential op stream
 * the semantics are identical, which is what this oracle exercises.
 *
 * Emits a compact JSON array of results (set/reset -> null, get -> string,
 * is_bg -> bool) byte-diffed against the Python oracle.
 */
#include "skill_provenance.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, (size_t)n, f); buf[n] = '\0'; fclose(f);
    return buf;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture>\n", argv[0]); return 1; }
    char *src = read_file(argv[1]);
    if (!src) return 1;
    json_t *root = json_parse(src, NULL);
    if (!root || root->type != JSON_OBJECT) { free(src); return 1; }

    json_t *results = json_new_array();
    skill_provenance_token_t stack[64];
    int sp = 0;

    json_t *ops = json_object_get(root, "ops");
    if (ops && ops->type == JSON_ARRAY) {
        for (size_t i = 0; i < ops->c.count; i++) {
            json_t *o = ops->c.items[i];
            const char *op = json_string_value(json_object_get(o, "op"));
            const char *origin = json_string_value(json_object_get(o, "origin"));
            if (strcmp(op, "set") == 0) {
                skill_provenance_token_t t = skill_provenance_set(origin);
                if (sp < 64) stack[sp++] = t;
                json_array_append(results, json_new_null());
            } else if (strcmp(op, "reset") == 0) {
                if (sp > 0) skill_provenance_reset(stack[--sp]);
                json_array_append(results, json_new_null());
            } else if (strcmp(op, "get") == 0) {
                json_array_append(results, json_new_string(skill_provenance_get()));
            } else if (strcmp(op, "is_bg") == 0) {
                json_array_append(results, json_new_bool(skill_provenance_is_background_review()));
            }
        }
    }
    char *out = json_serialize(results);
    printf("%s", out ? out : "[]");
    free(out);
    json_free(results);
    json_free(root);
    free(src);
    return 0;
}
