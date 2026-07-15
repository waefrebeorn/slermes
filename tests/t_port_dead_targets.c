/* Oracle harness for gateway/dead_targets.py port.
 * Reads fixture (argv[1]): {"path":"/tmp/...","ops":[...]}
 * Each op: {"op":"mark"/"clear"/"is_dead"/"is_dead_error_kind",
 *           "platform":..., "chat_id":..., "reason":...}
 * Prints a compact JSON object:
 *   {"results":[bool...], "final":{ <all_dead without marked_at> }}
 * The matched C/Python are byte-diffed. marked_at is stripped (nondeterministic).
 */
#include "gateway/dead_targets.h"
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

/* Build a json_t pointing at `node`, stripping its "marked_at" children so the
 * snapshot is deterministic. Returns a fresh object (caller frees). */
static json_t *strip_marked_at(json_t *node) {
    json_t *out = json_new_object();
    if (!node || node->type != JSON_OBJECT) return out;
    for (size_t i = 0; i < node->c.count; i++) {
        const char *k = node->c.keys[i];
        json_t *v = node->c.items[i];
        if (v && v->type == JSON_OBJECT) {
            json_t *entry = json_new_object();
            for (size_t j = 0; j < v->c.count; j++) {
                const char *ek = v->c.keys[j];
                if (strcmp(ek, "marked_at") == 0) continue;
                json_object_set(entry, ek, json_copy(v->c.items[j]));
            }
            json_object_set(out, k, entry);
        }
    }
    return out;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture>\n", argv[0]); return 1; }
    char *src = read_file(argv[1]);
    if (!src) return 1;
    json_t *root = json_parse(src, NULL);
    if (!root || root->type != JSON_OBJECT) { free(src); return 1; }

    const char *path = json_string_value(json_object_get(root, "c_path"));
    if (!path) path = json_string_value(json_object_get(root, "path"));
    dead_target_registry_t *r = dead_target_registry_create(path);

    json_t *results = json_new_array();
    json_t *ops = json_object_get(root, "ops");
    if (ops && ops->type == JSON_ARRAY) {
        for (size_t i = 0; i < ops->c.count; i++) {
            json_t *o = ops->c.items[i];
            const char *op = json_string_value(json_object_get(o, "op"));
            const char *plat = json_string_value(json_object_get(o, "platform"));
            const char *cid = json_string_value(json_object_get(o, "chat_id"));
            const char *reason = json_string_value(json_object_get(o, "reason"));
            bool res = false;
            if (strcmp(op, "is_dead_error_kind") == 0) {
                res = dead_target_is_dead_error_kind(cid);  /* reuse: kind in chat_id slot */
            } else if (strcmp(op, "mark") == 0) {
                res = dead_target_mark_dead(r, plat, cid, reason);
            } else if (strcmp(op, "clear") == 0) {
                res = dead_target_clear(r, plat, cid);
            } else if (strcmp(op, "is_dead") == 0) {
                res = dead_target_is_dead(r, plat, cid);
            }
            json_array_append(results, json_new_bool(res));
        }
    }

    json_t *all = dead_target_all_dead(r);
    json_t *clean = strip_marked_at(all);
    json_free(all);

    char *results_str = json_serialize(results);
    char *final_str = json_serialize(clean);
    printf("{\"results\":%s,\"final\":%s}", results_str ? results_str : "[]",
           final_str ? final_str : "{}");

    free(results_str); free(final_str);
    json_free(results); json_free(clean); json_free(root);
    dead_target_registry_free(r);
    free(src);
    return 0;
}
