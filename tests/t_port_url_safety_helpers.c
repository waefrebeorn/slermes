/* Curated integration oracle harness for src/tools/port_url_safety_helpers.c
 * (tools/url_safety.py). Reads a JSON array fixture from argv[1]; each element
 * is {"op":<cfunc>, "value":<input-string>}. Recomputes from the LIVE Python
 * source; run_oracle.sh diffs byte-for-byte.
 * Curated mapping (verified by reading both sources):
 *   tools_url_safety_has_sensitive_query_params(const char*) -> has_sensitive_query_params(url:str)->bool
 */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_url_safety_helpers.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern bool tools_url_safety_has_sensitive_query_params(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_has_sensitive(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    int v = (int)tools_url_safety_has_sensitive_query_params(value);
    json_t *o = json_new_object();
    json_set(o, "fn", json_string("tools_url_safety_has_sensitive_query_params"));
    json_set(o, "out", json_bool(v));
    return o;
}

int main(int argc, char **argv){
    if (argc < 2) { fprintf(stderr, "usage: %s <cases.json>\n", argv[0]); return 2; }
    char *input = read_all(argv[1]);
    if (!input) { fprintf(stderr, "cannot read %s\n", argv[1]); return 2; }
    char *err = NULL; json_t *root = json_parse(input, &err);
    if (err) { fprintf(stderr, "parse error: %s\n", err); free(err); free(input); return 2; }
    if (root->type != JSON_ARRAY) { fprintf(stderr, "fixture must be a JSON array\n"); free(input); return 2; }
    int n = json_array_size(root);
    for (int i = 0; i < n; i++){
        json_t *c = json_get(root, i);
        const char *op = json_get_str(c, "op", "");
        json_t *o = NULL;
        if (strcmp(op, "tools_url_safety_has_sensitive_query_params") == 0) o = emit_has_sensitive(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
