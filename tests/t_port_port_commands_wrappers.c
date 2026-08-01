/* AUTO-GENERATED integration oracle harness for port_commands_wrappers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_commands_wrappers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern bool cmd_is_gateway_known_command(const char *);
extern bool cmd_should_bypass_active_session(const char *);
extern int cmd_telegram_effective_priority(const char *);
extern bool cmd_is_skill_command(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_cmd_is_gateway_known_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    int v = (int)cmd_is_gateway_known_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cmd_is_gateway_known_command"));
    json_set(o, "out", json_bool(v)); return o;
}

static json_t *emit_cmd_should_bypass_active_session(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    int v = (int)cmd_should_bypass_active_session(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cmd_should_bypass_active_session"));
    json_set(o, "out", json_bool(v)); return o;
}

static json_t *emit_cmd_telegram_effective_priority(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)cmd_telegram_effective_priority(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cmd_telegram_effective_priority"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_cmd_is_skill_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    int v = (int)cmd_is_skill_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("cmd_is_skill_command"));
    json_set(o, "out", json_bool(v)); return o;
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
        if (strcmp(op, "cmd_is_gateway_known_command") == 0) o = emit_cmd_is_gateway_known_command(c);
        if (strcmp(op, "cmd_should_bypass_active_session") == 0) o = emit_cmd_should_bypass_active_session(c);
        if (strcmp(op, "cmd_telegram_effective_priority") == 0) o = emit_cmd_telegram_effective_priority(c);
        if (strcmp(op, "cmd_is_skill_command") == 0) o = emit_cmd_is_skill_command(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
