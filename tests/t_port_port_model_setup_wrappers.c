/* AUTO-GENERATED integration oracle harness for port_model_setup_wrappers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_model_setup_wrappers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int msf_bedrock_region_geo_prefix(const char *);
extern int msf_bedrock_model_routable_from_region(const char *);
extern int msf_u_prune_replaced_custom_model_config_credentials(const char *);
extern int msf_u_prompt_auth_credentials_choice(const char *);
extern int msf_u_model_flow_openrouter(const char *);
extern int msf_u_print_moa_preset(const char *);
extern int msf_u_model_flow_moa(const char *);
extern int msf_u_model_flow_nous(const char *);
extern int msf_u_model_flow_openai_codex(const char *);
extern int msf_u_model_flow_xai_oauth(const char *);
extern int msf_u_model_flow_qwen_oauth(const char *);
extern int msf_u_model_flow_minimax_oauth(const char *);
extern int msf_u_model_flow_custom(const char *);
extern int msf_u_model_flow_azure_foundry(const char *);
extern int msf_u_model_flow_named_custom(const char *);
extern int msf_u_model_flow_copilot(const char *);
extern int msf_u_model_flow_copilot_acp(const char *);
extern int msf_u_model_flow_kimi(const char *);
extern int msf_u_model_flow_stepfun(const char *);
extern int msf_u_model_flow_bedrock_api_key(const char *);
extern int msf_u_model_flow_bedrock(const char *);
extern int msf_u_model_flow_vertex(const char *);
extern int msf_u_select_zai_endpoint(const char *);
extern int msf_u_model_flow_anthropic(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_msf_bedrock_region_geo_prefix(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msf_bedrock_region_geo_prefix(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msf_bedrock_region_geo_prefix"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_msf_bedrock_model_routable_from_region(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msf_bedrock_model_routable_from_region(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msf_bedrock_model_routable_from_region"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_msf_u_prune_replaced_custom_model_config_credentials(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msf_u_prune_replaced_custom_model_config_credentials(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msf_u_prune_replaced_custom_model_config_credentials"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_msf_u_prompt_auth_credentials_choice(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msf_u_prompt_auth_credentials_choice(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msf_u_prompt_auth_credentials_choice"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_msf_u_model_flow_openrouter(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msf_u_model_flow_openrouter(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msf_u_model_flow_openrouter"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_msf_u_print_moa_preset(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msf_u_print_moa_preset(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msf_u_print_moa_preset"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_msf_u_model_flow_moa(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msf_u_model_flow_moa(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msf_u_model_flow_moa"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_msf_u_model_flow_nous(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msf_u_model_flow_nous(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msf_u_model_flow_nous"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_msf_u_model_flow_openai_codex(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msf_u_model_flow_openai_codex(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msf_u_model_flow_openai_codex"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_msf_u_model_flow_xai_oauth(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msf_u_model_flow_xai_oauth(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msf_u_model_flow_xai_oauth"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_msf_u_model_flow_qwen_oauth(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msf_u_model_flow_qwen_oauth(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msf_u_model_flow_qwen_oauth"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_msf_u_model_flow_minimax_oauth(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msf_u_model_flow_minimax_oauth(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msf_u_model_flow_minimax_oauth"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_msf_u_model_flow_custom(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msf_u_model_flow_custom(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msf_u_model_flow_custom"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_msf_u_model_flow_azure_foundry(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msf_u_model_flow_azure_foundry(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msf_u_model_flow_azure_foundry"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_msf_u_model_flow_named_custom(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msf_u_model_flow_named_custom(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msf_u_model_flow_named_custom"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_msf_u_model_flow_copilot(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msf_u_model_flow_copilot(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msf_u_model_flow_copilot"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_msf_u_model_flow_copilot_acp(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msf_u_model_flow_copilot_acp(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msf_u_model_flow_copilot_acp"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_msf_u_model_flow_kimi(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msf_u_model_flow_kimi(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msf_u_model_flow_kimi"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_msf_u_model_flow_stepfun(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msf_u_model_flow_stepfun(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msf_u_model_flow_stepfun"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_msf_u_model_flow_bedrock_api_key(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msf_u_model_flow_bedrock_api_key(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msf_u_model_flow_bedrock_api_key"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_msf_u_model_flow_bedrock(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msf_u_model_flow_bedrock(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msf_u_model_flow_bedrock"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_msf_u_model_flow_vertex(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msf_u_model_flow_vertex(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msf_u_model_flow_vertex"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_msf_u_select_zai_endpoint(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msf_u_select_zai_endpoint(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msf_u_select_zai_endpoint"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_msf_u_model_flow_anthropic(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msf_u_model_flow_anthropic(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msf_u_model_flow_anthropic"));
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
        if (strcmp(op, "msf_bedrock_region_geo_prefix") == 0) o = emit_msf_bedrock_region_geo_prefix(c);
        if (strcmp(op, "msf_bedrock_model_routable_from_region") == 0) o = emit_msf_bedrock_model_routable_from_region(c);
        if (strcmp(op, "msf_u_prune_replaced_custom_model_config_credentials") == 0) o = emit_msf_u_prune_replaced_custom_model_config_credentials(c);
        if (strcmp(op, "msf_u_prompt_auth_credentials_choice") == 0) o = emit_msf_u_prompt_auth_credentials_choice(c);
        if (strcmp(op, "msf_u_model_flow_openrouter") == 0) o = emit_msf_u_model_flow_openrouter(c);
        if (strcmp(op, "msf_u_print_moa_preset") == 0) o = emit_msf_u_print_moa_preset(c);
        if (strcmp(op, "msf_u_model_flow_moa") == 0) o = emit_msf_u_model_flow_moa(c);
        if (strcmp(op, "msf_u_model_flow_nous") == 0) o = emit_msf_u_model_flow_nous(c);
        if (strcmp(op, "msf_u_model_flow_openai_codex") == 0) o = emit_msf_u_model_flow_openai_codex(c);
        if (strcmp(op, "msf_u_model_flow_xai_oauth") == 0) o = emit_msf_u_model_flow_xai_oauth(c);
        if (strcmp(op, "msf_u_model_flow_qwen_oauth") == 0) o = emit_msf_u_model_flow_qwen_oauth(c);
        if (strcmp(op, "msf_u_model_flow_minimax_oauth") == 0) o = emit_msf_u_model_flow_minimax_oauth(c);
        if (strcmp(op, "msf_u_model_flow_custom") == 0) o = emit_msf_u_model_flow_custom(c);
        if (strcmp(op, "msf_u_model_flow_azure_foundry") == 0) o = emit_msf_u_model_flow_azure_foundry(c);
        if (strcmp(op, "msf_u_model_flow_named_custom") == 0) o = emit_msf_u_model_flow_named_custom(c);
        if (strcmp(op, "msf_u_model_flow_copilot") == 0) o = emit_msf_u_model_flow_copilot(c);
        if (strcmp(op, "msf_u_model_flow_copilot_acp") == 0) o = emit_msf_u_model_flow_copilot_acp(c);
        if (strcmp(op, "msf_u_model_flow_kimi") == 0) o = emit_msf_u_model_flow_kimi(c);
        if (strcmp(op, "msf_u_model_flow_stepfun") == 0) o = emit_msf_u_model_flow_stepfun(c);
        if (strcmp(op, "msf_u_model_flow_bedrock_api_key") == 0) o = emit_msf_u_model_flow_bedrock_api_key(c);
        if (strcmp(op, "msf_u_model_flow_bedrock") == 0) o = emit_msf_u_model_flow_bedrock(c);
        if (strcmp(op, "msf_u_model_flow_vertex") == 0) o = emit_msf_u_model_flow_vertex(c);
        if (strcmp(op, "msf_u_select_zai_endpoint") == 0) o = emit_msf_u_select_zai_endpoint(c);
        if (strcmp(op, "msf_u_model_flow_anthropic") == 0) o = emit_msf_u_model_flow_anthropic(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
