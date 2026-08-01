/* AUTO-GENERATED integration oracle harness for port_nous_sub_wrappers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_nous_sub_wrappers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int nsub_u_uses_gateway(const char *);
extern int nsub_image_gen(const char *);
extern int nsub_video_gen(const char *);
extern int nsub_u_toolset_enabled(const char *);
extern int nsub_u_has_agent_browser(const char *);
extern int nsub_u_local_browser_runnable(const char *);
extern int nsub_u_browser_label(const char *);
extern int nsub_u_tts_label(const char *);
extern int nsub_u_stt_label(const char *);
extern int nsub_u_local_stt_backend_available(const char *);
extern int nsub_u_resolve_browser_feature_state(const char *);
extern int nsub_apply_nous_managed_defaults(const char *);
extern int nsub_u_get_gateway_direct_credentials(const char *);
extern int nsub_get_gateway_eligible_tools(const char *);
extern int nsub_apply_gateway_defaults(const char *);
extern int nsub_prompt_enable_tool_gateway(const char *);
extern int nsub_ensure_nous_portal_access(const char *);
extern int nsub_u_run_nous_portal_login_only(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_nsub_u_uses_gateway(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)nsub_u_uses_gateway(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("nsub_u_uses_gateway"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_nsub_image_gen(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)nsub_image_gen(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("nsub_image_gen"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_nsub_video_gen(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)nsub_video_gen(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("nsub_video_gen"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_nsub_u_toolset_enabled(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)nsub_u_toolset_enabled(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("nsub_u_toolset_enabled"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_nsub_u_has_agent_browser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)nsub_u_has_agent_browser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("nsub_u_has_agent_browser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_nsub_u_local_browser_runnable(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)nsub_u_local_browser_runnable(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("nsub_u_local_browser_runnable"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_nsub_u_browser_label(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)nsub_u_browser_label(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("nsub_u_browser_label"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_nsub_u_tts_label(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)nsub_u_tts_label(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("nsub_u_tts_label"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_nsub_u_stt_label(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)nsub_u_stt_label(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("nsub_u_stt_label"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_nsub_u_local_stt_backend_available(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)nsub_u_local_stt_backend_available(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("nsub_u_local_stt_backend_available"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_nsub_u_resolve_browser_feature_state(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)nsub_u_resolve_browser_feature_state(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("nsub_u_resolve_browser_feature_state"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_nsub_apply_nous_managed_defaults(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)nsub_apply_nous_managed_defaults(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("nsub_apply_nous_managed_defaults"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_nsub_u_get_gateway_direct_credentials(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)nsub_u_get_gateway_direct_credentials(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("nsub_u_get_gateway_direct_credentials"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_nsub_get_gateway_eligible_tools(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)nsub_get_gateway_eligible_tools(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("nsub_get_gateway_eligible_tools"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_nsub_apply_gateway_defaults(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)nsub_apply_gateway_defaults(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("nsub_apply_gateway_defaults"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_nsub_prompt_enable_tool_gateway(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)nsub_prompt_enable_tool_gateway(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("nsub_prompt_enable_tool_gateway"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_nsub_ensure_nous_portal_access(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)nsub_ensure_nous_portal_access(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("nsub_ensure_nous_portal_access"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_nsub_u_run_nous_portal_login_only(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)nsub_u_run_nous_portal_login_only(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("nsub_u_run_nous_portal_login_only"));
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
        if (strcmp(op, "nsub_u_uses_gateway") == 0) o = emit_nsub_u_uses_gateway(c);
        if (strcmp(op, "nsub_image_gen") == 0) o = emit_nsub_image_gen(c);
        if (strcmp(op, "nsub_video_gen") == 0) o = emit_nsub_video_gen(c);
        if (strcmp(op, "nsub_u_toolset_enabled") == 0) o = emit_nsub_u_toolset_enabled(c);
        if (strcmp(op, "nsub_u_has_agent_browser") == 0) o = emit_nsub_u_has_agent_browser(c);
        if (strcmp(op, "nsub_u_local_browser_runnable") == 0) o = emit_nsub_u_local_browser_runnable(c);
        if (strcmp(op, "nsub_u_browser_label") == 0) o = emit_nsub_u_browser_label(c);
        if (strcmp(op, "nsub_u_tts_label") == 0) o = emit_nsub_u_tts_label(c);
        if (strcmp(op, "nsub_u_stt_label") == 0) o = emit_nsub_u_stt_label(c);
        if (strcmp(op, "nsub_u_local_stt_backend_available") == 0) o = emit_nsub_u_local_stt_backend_available(c);
        if (strcmp(op, "nsub_u_resolve_browser_feature_state") == 0) o = emit_nsub_u_resolve_browser_feature_state(c);
        if (strcmp(op, "nsub_apply_nous_managed_defaults") == 0) o = emit_nsub_apply_nous_managed_defaults(c);
        if (strcmp(op, "nsub_u_get_gateway_direct_credentials") == 0) o = emit_nsub_u_get_gateway_direct_credentials(c);
        if (strcmp(op, "nsub_get_gateway_eligible_tools") == 0) o = emit_nsub_get_gateway_eligible_tools(c);
        if (strcmp(op, "nsub_apply_gateway_defaults") == 0) o = emit_nsub_apply_gateway_defaults(c);
        if (strcmp(op, "nsub_prompt_enable_tool_gateway") == 0) o = emit_nsub_prompt_enable_tool_gateway(c);
        if (strcmp(op, "nsub_ensure_nous_portal_access") == 0) o = emit_nsub_ensure_nous_portal_access(c);
        if (strcmp(op, "nsub_u_run_nous_portal_login_only") == 0) o = emit_nsub_u_run_nous_portal_login_only(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
