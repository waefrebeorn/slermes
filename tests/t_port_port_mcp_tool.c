/* AUTO-GENERATED integration oracle harness for port_mcp_tool (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_mcp_tool.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern bool mcp_tool_check_message_handler_support(const char *);
extern bool mcp_tool_is_method_not_found_error(const char *);
extern bool mcp_tool_validate_remote_mcp_url(const char *);
extern bool mcp_tool_is_http(const char *);
extern bool mcp_tool_advertises_tools(const char *);
extern bool mcp_tool_refresh_tools(const char *);
extern bool mcp_tool_keepalive_probe(const char *);
extern bool mcp_tool_preflight_content_type(const char *);
extern bool mcp_tool_is_session_expired_error(const char *);
extern bool mcp_tool_parse_boolish(const char *);
extern bool mcp_tool_validate_server_config(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_mcp_tool_check_message_handler_support(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    int v = (int)mcp_tool_check_message_handler_support(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcp_tool_check_message_handler_support"));
    json_set(o, "out", json_bool(v)); return o;
}

static json_t *emit_mcp_tool_is_method_not_found_error(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    int v = (int)mcp_tool_is_method_not_found_error(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcp_tool_is_method_not_found_error"));
    json_set(o, "out", json_bool(v)); return o;
}

static json_t *emit_mcp_tool_validate_remote_mcp_url(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    int v = (int)mcp_tool_validate_remote_mcp_url(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcp_tool_validate_remote_mcp_url"));
    json_set(o, "out", json_bool(v)); return o;
}

static json_t *emit_mcp_tool_is_http(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    int v = (int)mcp_tool_is_http(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcp_tool_is_http"));
    json_set(o, "out", json_bool(v)); return o;
}

static json_t *emit_mcp_tool_advertises_tools(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    int v = (int)mcp_tool_advertises_tools(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcp_tool_advertises_tools"));
    json_set(o, "out", json_bool(v)); return o;
}

static json_t *emit_mcp_tool_refresh_tools(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    int v = (int)mcp_tool_refresh_tools(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcp_tool_refresh_tools"));
    json_set(o, "out", json_bool(v)); return o;
}

static json_t *emit_mcp_tool_keepalive_probe(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    int v = (int)mcp_tool_keepalive_probe(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcp_tool_keepalive_probe"));
    json_set(o, "out", json_bool(v)); return o;
}

static json_t *emit_mcp_tool_preflight_content_type(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    int v = (int)mcp_tool_preflight_content_type(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcp_tool_preflight_content_type"));
    json_set(o, "out", json_bool(v)); return o;
}

static json_t *emit_mcp_tool_is_session_expired_error(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    int v = (int)mcp_tool_is_session_expired_error(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcp_tool_is_session_expired_error"));
    json_set(o, "out", json_bool(v)); return o;
}

static json_t *emit_mcp_tool_parse_boolish(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    int v = (int)mcp_tool_parse_boolish(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcp_tool_parse_boolish"));
    json_set(o, "out", json_bool(v)); return o;
}

static json_t *emit_mcp_tool_validate_server_config(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    int v = (int)mcp_tool_validate_server_config(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcp_tool_validate_server_config"));
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
        if (strcmp(op, "mcp_tool_check_message_handler_support") == 0) o = emit_mcp_tool_check_message_handler_support(c);
        if (strcmp(op, "mcp_tool_is_method_not_found_error") == 0) o = emit_mcp_tool_is_method_not_found_error(c);
        if (strcmp(op, "mcp_tool_validate_remote_mcp_url") == 0) o = emit_mcp_tool_validate_remote_mcp_url(c);
        if (strcmp(op, "mcp_tool_is_http") == 0) o = emit_mcp_tool_is_http(c);
        if (strcmp(op, "mcp_tool_advertises_tools") == 0) o = emit_mcp_tool_advertises_tools(c);
        if (strcmp(op, "mcp_tool_refresh_tools") == 0) o = emit_mcp_tool_refresh_tools(c);
        if (strcmp(op, "mcp_tool_keepalive_probe") == 0) o = emit_mcp_tool_keepalive_probe(c);
        if (strcmp(op, "mcp_tool_preflight_content_type") == 0) o = emit_mcp_tool_preflight_content_type(c);
        if (strcmp(op, "mcp_tool_is_session_expired_error") == 0) o = emit_mcp_tool_is_session_expired_error(c);
        if (strcmp(op, "mcp_tool_parse_boolish") == 0) o = emit_mcp_tool_parse_boolish(c);
        if (strcmp(op, "mcp_tool_validate_server_config") == 0) o = emit_mcp_tool_validate_server_config(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
