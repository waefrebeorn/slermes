#include "hermes_json.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "noc_w11_anthropice_3d1193.h"

static void emit_key(const char *key, json_t *v) {
    char *s = v ? json_dumps(v, 0) : NULL;
    printf("\"%s\":", key);
    if (s) { fputs(s, stdout); free(s); }
    else fputs("null", stdout);
}
static const char *jstr(json_t *v){ return (v && v->type==JSON_STRING) ? v->str_val : NULL; }
static int jbool(json_t *v){ return (v && v->type==JSON_BOOL) ? v->bool_val : 0; }
int main(void) {
    char line[32768];
    setvbuf(stdout, NULL, _IONBF, 0);
    while (fgets(line, sizeof(line), stdin)) {
        if (!strchr(line, '{')) continue;
        json_t *in = json_parse(line, NULL);
        if (!in) continue;
        const char *func = json_get_str(in, "func", "");
        json_t *out = NULL;
        char *tmp = NULL;
        const char *b = jstr(json_obj_get(in,"base_url"));
        const char *m = jstr(json_obj_get(in,"model"));
        int nous = jbool(json_obj_get(in,"nous_override"));
        if (!strcmp(func,"normalize_base_url_text")) {
            tmp = w11_anthropice_3d1193_normalize_base_url_text(b);
            out = json_string(tmp);
        } else if (!strcmp(func,"normalized_lower")) {
            tmp = w11_anthropice_3d1193_normalized_lower(b);
            out = json_string(tmp);
        } else if (!strcmp(func,"is_third_party_anthropic_endpoint"))
            out = json_bool(w11_anthropice_3d1193_is_third_party_anthropic_endpoint(b));
        else if (!strcmp(func,"is_kimi_coding_endpoint"))
            out = json_bool(w11_anthropice_3d1193_is_kimi_coding_endpoint(b));
        else if (!strcmp(func,"is_opencode_endpoint"))
            out = json_bool(w11_anthropice_3d1193_is_opencode_endpoint(b));
        else if (!strcmp(func,"model_name_is_kimi_family"))
            out = json_bool(w11_anthropice_3d1193_model_name_is_kimi_family(m));
        else if (!strcmp(func,"is_kimi_family_endpoint"))
            out = json_bool(w11_anthropice_3d1193_is_kimi_family_endpoint(b,m));
        else if (!strcmp(func,"model_name_is_deepseek_thinking"))
            out = json_bool(w11_anthropice_3d1193_model_name_is_deepseek_thinking(m));
        else if (!strcmp(func,"is_deepseek_anthropic_endpoint"))
            out = json_bool(w11_anthropice_3d1193_is_deepseek_anthropic_endpoint(b));
        else if (!strcmp(func,"is_minimax_anthropic_endpoint"))
            out = json_bool(w11_anthropice_3d1193_is_minimax_anthropic_endpoint(b));
        else if (!strcmp(func,"is_azure_anthropic_endpoint"))
            out = json_bool(w11_anthropice_3d1193_is_azure_anthropic_endpoint(b));
        else if (!strcmp(func,"is_bedrock_runtime_endpoint"))
            out = json_bool(w11_anthropice_3d1193_is_bedrock_runtime_endpoint(b));
        else if (!strcmp(func,"is_vertex_ai_endpoint"))
            out = json_bool(w11_anthropice_3d1193_is_vertex_ai_endpoint(b));
        else if (!strcmp(func,"is_claude_platform_endpoint"))
            out = json_bool(w11_anthropice_3d1193_is_claude_platform_endpoint(b));
        else if (!strcmp(func,"base_url_needs_context_1m_beta"))
            out = json_bool(w11_anthropice_3d1193_base_url_needs_context_1m_beta(b));
        else if (!strcmp(func,"requires_bearer_auth"))
            out = json_bool(w11_anthropice_3d1193_requires_bearer_auth(b,nous));
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
        free(tmp);
        json_free(out);
        json_free(in);
    }
    return 0;
}
