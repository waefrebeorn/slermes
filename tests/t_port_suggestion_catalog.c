/* Oracle harness for cron/suggestion_catalog.py port.
 * Reads fixture (argv[1]): {"op":"seed","keys":[...] | null} or {"op":"classify_path"}.
 * For "seed": uses a mock add_fn that echoes each call as a JSON object; prints
 * the array of echoed calls (== what Python's mock add_fn would receive).
 * For "classify_path": prints the classify_items script path (HERMES_ROOT pinned).
 * Emits a compact JSON array/string for byte-diff against the Python oracle.
 */
#include "cron/suggestion_catalog.h"
#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    fread(buf, 1, (size_t)n, f); buf[n] = '\0'; fclose(f);
    return buf;
}

/* Mock add_fn: echoes its arguments into a global array; returns the same
 * record so seed_catalog_suggestions appends it to `created`. Mirrors a Python
 * test double that returns a dict of the kwargs it was called with. */
static json_t *g_calls = NULL;

static json_t *mock_add(const char *title, const char *description,
                        const char *source, json_t *job_spec,
                        const char *dedup_key) {
    json_t *rec = json_new_object();
    json_object_set(rec, "title", json_new_string(title ? title : ""));
    json_object_set(rec, "description", json_new_string(description ? description : ""));
    json_object_set(rec, "source", json_new_string(source ? source : ""));
    json_object_set(rec, "job_spec", job_spec ? json_copy(job_spec) : json_new_object());
    json_object_set(rec, "dedup_key", json_new_string(dedup_key ? dedup_key : ""));
    json_array_append(g_calls, json_copy(rec));
    return rec; /* seed frees nothing here; it appends this to `created` */
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "usage: %s <fixture>\n", argv[0]); return 1; }
    setenv("HERMES_ROOT", "/opt/hermes", 1);
    char *src = read_file(argv[1]);
    if (!src) return 1;
    json_t *root = json_parse(src, NULL);
    if (!root || root->type != JSON_OBJECT) { free(src); return 1; }

    const char *op = json_string_value(json_object_get(root, "op"));

    if (op && strcmp(op, "classify_path") == 0) {
        char buf[1024];
        classify_items_script_path(buf, sizeof(buf));
        printf("\"%s\"", buf);
    } else { /* seed */
        g_calls = json_new_array();
        const char *keys_c[16]; int nk = 0;
        json_t *keys = json_object_get(root, "keys");
        const char *const *keys_arg = NULL;
        if (keys && keys->type == JSON_ARRAY) {
            for (size_t i = 0; i < keys->c.count && nk < 15; i++) {
                const char *k = json_string_value(keys->c.items[i]);
                if (k) keys_c[nk++] = k;
            }
            keys_c[nk] = NULL;
            keys_arg = keys_c;
        }
        json_t *created = seed_catalog_suggestions(mock_add, keys_arg);
        char *out = json_serialize(g_calls);
        printf("%s", out ? out : "[]");
        free(out);
        json_free(created);
        json_free(g_calls);
    }

    json_free(root);
    free(src);
    return 0;
}
