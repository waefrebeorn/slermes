/* AUTO-GENERATED integration oracle harness for port_model_switch_wrappers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_model_switch_wrappers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int msw_u_declared_model_ids(const char *);
extern int msw_u_save_discovered_models_to_config(const char *);
extern int msw_u_bare_custom_provider_def(const char *);
extern int msw_format_model_for_display(const char *);
extern int msw_is_nous_hermes_non_agentic(const char *);
extern int msw_u_check_hermes_model_warning(const char *);
extern int msw_u_load_direct_aliases(const char *);
extern int msw_u_ensure_direct_aliases(const char *);
extern int msw_parse_model_flags_detailed(const char *);
extern int msw_u_model_sort_key(const char *);
extern int msw_get_authenticated_provider_slugs(const char *);
extern int msw_u_resolve_alias_fallback(const char *);
extern int msw_resolve_display_context_length(const char *);
extern int msw_u_configured_provider_matches(const char *);
extern int msw_u_resolve_named_custom_model_id(const char *);
extern int msw_u_credential_pool_is_usable(const char *);
extern int msw_u_extra_headers_from_config(const char *);
extern int msw_prewarm_picker_cache_async(const char *);
extern int msw_u_prepend_moa_picker_provider(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_msw_u_declared_model_ids(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msw_u_declared_model_ids(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msw_u_declared_model_ids"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_msw_u_save_discovered_models_to_config(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msw_u_save_discovered_models_to_config(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msw_u_save_discovered_models_to_config"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_msw_u_bare_custom_provider_def(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msw_u_bare_custom_provider_def(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msw_u_bare_custom_provider_def"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_msw_format_model_for_display(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msw_format_model_for_display(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msw_format_model_for_display"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_msw_is_nous_hermes_non_agentic(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msw_is_nous_hermes_non_agentic(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msw_is_nous_hermes_non_agentic"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_msw_u_check_hermes_model_warning(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msw_u_check_hermes_model_warning(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msw_u_check_hermes_model_warning"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_msw_u_load_direct_aliases(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msw_u_load_direct_aliases(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msw_u_load_direct_aliases"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_msw_u_ensure_direct_aliases(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msw_u_ensure_direct_aliases(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msw_u_ensure_direct_aliases"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_msw_parse_model_flags_detailed(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msw_parse_model_flags_detailed(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msw_parse_model_flags_detailed"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_msw_u_model_sort_key(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msw_u_model_sort_key(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msw_u_model_sort_key"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_msw_get_authenticated_provider_slugs(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msw_get_authenticated_provider_slugs(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msw_get_authenticated_provider_slugs"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_msw_u_resolve_alias_fallback(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msw_u_resolve_alias_fallback(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msw_u_resolve_alias_fallback"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_msw_resolve_display_context_length(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msw_resolve_display_context_length(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msw_resolve_display_context_length"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_msw_u_configured_provider_matches(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msw_u_configured_provider_matches(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msw_u_configured_provider_matches"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_msw_u_resolve_named_custom_model_id(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msw_u_resolve_named_custom_model_id(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msw_u_resolve_named_custom_model_id"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_msw_u_credential_pool_is_usable(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msw_u_credential_pool_is_usable(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msw_u_credential_pool_is_usable"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_msw_u_extra_headers_from_config(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msw_u_extra_headers_from_config(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msw_u_extra_headers_from_config"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_msw_prewarm_picker_cache_async(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msw_prewarm_picker_cache_async(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msw_prewarm_picker_cache_async"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_msw_u_prepend_moa_picker_provider(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)msw_u_prepend_moa_picker_provider(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("msw_u_prepend_moa_picker_provider"));
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
        if (strcmp(op, "msw_u_declared_model_ids") == 0) o = emit_msw_u_declared_model_ids(c);
        if (strcmp(op, "msw_u_save_discovered_models_to_config") == 0) o = emit_msw_u_save_discovered_models_to_config(c);
        if (strcmp(op, "msw_u_bare_custom_provider_def") == 0) o = emit_msw_u_bare_custom_provider_def(c);
        if (strcmp(op, "msw_format_model_for_display") == 0) o = emit_msw_format_model_for_display(c);
        if (strcmp(op, "msw_is_nous_hermes_non_agentic") == 0) o = emit_msw_is_nous_hermes_non_agentic(c);
        if (strcmp(op, "msw_u_check_hermes_model_warning") == 0) o = emit_msw_u_check_hermes_model_warning(c);
        if (strcmp(op, "msw_u_load_direct_aliases") == 0) o = emit_msw_u_load_direct_aliases(c);
        if (strcmp(op, "msw_u_ensure_direct_aliases") == 0) o = emit_msw_u_ensure_direct_aliases(c);
        if (strcmp(op, "msw_parse_model_flags_detailed") == 0) o = emit_msw_parse_model_flags_detailed(c);
        if (strcmp(op, "msw_u_model_sort_key") == 0) o = emit_msw_u_model_sort_key(c);
        if (strcmp(op, "msw_get_authenticated_provider_slugs") == 0) o = emit_msw_get_authenticated_provider_slugs(c);
        if (strcmp(op, "msw_u_resolve_alias_fallback") == 0) o = emit_msw_u_resolve_alias_fallback(c);
        if (strcmp(op, "msw_resolve_display_context_length") == 0) o = emit_msw_resolve_display_context_length(c);
        if (strcmp(op, "msw_u_configured_provider_matches") == 0) o = emit_msw_u_configured_provider_matches(c);
        if (strcmp(op, "msw_u_resolve_named_custom_model_id") == 0) o = emit_msw_u_resolve_named_custom_model_id(c);
        if (strcmp(op, "msw_u_credential_pool_is_usable") == 0) o = emit_msw_u_credential_pool_is_usable(c);
        if (strcmp(op, "msw_u_extra_headers_from_config") == 0) o = emit_msw_u_extra_headers_from_config(c);
        if (strcmp(op, "msw_prewarm_picker_cache_async") == 0) o = emit_msw_prewarm_picker_cache_async(c);
        if (strcmp(op, "msw_u_prepend_moa_picker_provider") == 0) o = emit_msw_u_prepend_moa_picker_provider(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
