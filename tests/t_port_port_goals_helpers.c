/* AUTO-GENERATED integration oracle harness for port_goals_helpers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_goals_helpers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int goals_is_empty(const char *);
extern int goals_has_contract(const char *);
extern int goals_is_active(const char *);
extern int goals_has_goal(const char *);
extern int goals_is_waiting(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_goals_is_empty(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)goals_is_empty(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("goals_is_empty"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_goals_has_contract(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)goals_has_contract(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("goals_has_contract"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_goals_is_active(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)goals_is_active(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("goals_is_active"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_goals_has_goal(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)goals_has_goal(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("goals_has_goal"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_goals_is_waiting(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)goals_is_waiting(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("goals_is_waiting"));
    json_set(o, "out", json_int(v)); return o;
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
        if (strcmp(op, "goals_is_empty") == 0) o = emit_goals_is_empty(c);
        if (strcmp(op, "goals_has_contract") == 0) o = emit_goals_has_contract(c);
        if (strcmp(op, "goals_is_active") == 0) o = emit_goals_is_active(c);
        if (strcmp(op, "goals_has_goal") == 0) o = emit_goals_has_goal(c);
        if (strcmp(op, "goals_is_waiting") == 0) o = emit_goals_is_waiting(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
