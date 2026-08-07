/* Oracle harness for src/agent/context_compressor_pure.c pure helpers ported
 * from agent/context_compressor.py module-level + @staticmethod functions.
 * Reads a JSON array fixture from argv[1]; each element is
 * {"op":<cfunc>, ...args...}. Emits one JSON result per line.
 *
 * Verified C -> Python mapping:
 *   cc_template_visible_role      -> context_compressor._template_visible_role
 *   cc_reasoning_details_text_chars -> context_compressor._reasoning_details_text_chars
 *   cc_rolling_summary_from_marker -> context_compressor.ContextCompressor._rolling_summary_from_marker
 *   cc_render_micro_marker_content -> context_compressor.ContextCompressor._render_micro_marker_content
 *   cc_merge_adjacent_user_turns  -> context_compressor.ContextCompressor._merge_adjacent_user_turns
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hermes_json.h"
#include "context_compressor_pure.h"

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

int main(int argc, char **argv){
    if (argc < 2) { fprintf(stderr, "usage: %s <cases.json>\n", argv[0]); return 2; }
    char *input = read_all(argv[1]);
    if (!input) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
    char *err = NULL;
    json_t *root = json_parse(input, &err);
    if (err) { fprintf(stderr, "parse error: %s\n", err); free(err); free(input); return 2; }
    free(input);
    if (root->type != JSON_ARRAY) { fprintf(stderr, "fixture must be a JSON array\n"); json_free(root); return 2; }

    for (size_t i = 0; i < root->c.count; i++){
        json_t *c = json_get(root, i);
        const char *op = json_get_str(c, "op", "");
        json_t *out = json_object();
        json_set(out, "op", json_string(op));

        if (strcmp(op, "template_visible_role") == 0) {
            const char *r = cc_template_visible_role(json_obj_get(c, "msg"));
            json_set(out, "value", r ? json_string(r) : json_null());
        } else if (strcmp(op, "reasoning_details_text_chars") == 0) {
            long r = cc_reasoning_details_text_chars(json_obj_get(c, "value"));
            json_set(out, "value", json_number((double)r));
        } else if (strcmp(op, "rolling_summary_from_marker") == 0) {
            char *r = cc_rolling_summary_from_marker(json_get_str(c, "content", ""));
            json_set(out, "value", r ? json_string(r) : json_null());
            free(r);
        } else if (strcmp(op, "render_micro_marker_content") == 0) {
            char *r = cc_render_micro_marker_content(json_get_str(c, "summary", ""));
            json_set(out, "value", r ? json_string(r) : json_null());
            free(r);
        } else if (strcmp(op, "merge_adjacent_user_turns") == 0) {
            json_t *merged = NULL;
            int n = cc_merge_adjacent_user_turns(json_obj_get(c, "messages"), &merged);
            (void)n;
            json_set(out, "value", merged ? json_copy(merged) : json_null());
            json_free(merged);
        } else {
            json_set(out, "error", json_string("unknown op"));
        }

        char *ser = json_serialize(out);
        printf("%s\n", ser);
        free(ser);
        json_free(out);
    }
    json_free(root);
    return 0;
}
