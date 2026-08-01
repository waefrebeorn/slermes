/* AUTO-GENERATED integration oracle harness for port_mcp_oauth_wrappers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_mcp_oauth_wrappers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int mcpo_u_get_token_dir(const char *);
extern int mcpo_u_safe_filename(const char *);
extern int mcpo_u_find_free_port(const char *);
extern int mcpo_u_reserve_callback_port(const char *);
extern int mcpo_u_cached_redirect_port(const char *);
extern int mcpo_u_cached_redirect_uri(const char *);
extern int mcpo_u_is_interactive(const char *);
extern int mcpo_u_raise_if_non_interactive(const char *);
extern int mcpo_force_interactive_oauth(const char *);
extern int mcpo_suppress_interactive_oauth(const char *);
extern int mcpo_u_can_open_browser(const char *);
extern int mcpo_u_read_json(const char *);
extern int mcpo_u_write_json(const char *);
extern int mcpo_u_tokens_path(const char *);
extern int mcpo_u_client_info_path(const char *);
extern int mcpo_u_meta_path(const char *);
extern int mcpo_get_tokens(const char *);
extern int mcpo_set_tokens(const char *);
extern int mcpo_get_client_info(const char *);
extern int mcpo_set_client_info(const char *);
extern int mcpo_save_oauth_metadata(const char *);
extern int mcpo_load_oauth_metadata(const char *);
extern int mcpo_poison_client_registration(const char *);
extern int mcpo_has_cached_tokens(const char *);
extern int mcpo_u_make_callback_handler(const char *);
extern int mcpo_u_make_redirect_handler(const char *);
extern int mcpo_u_wait_for_callback(const char *);
extern int mcpo_u_make_callback_waiter(const char *);
extern int mcpo_u_paste_callback_reader(const char *);
extern int mcpo_remove_oauth_tokens(const char *);
extern int mcpo_u_configure_callback_port(const char *);
extern int mcpo_u_resolve_redirect_uri(const char *);
extern int mcpo_u_build_client_metadata(const char *);
extern int mcpo_u_maybe_preregister_client(const char *);
extern int mcpo_build_oauth_auth(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_mcpo_u_get_token_dir(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mcpo_u_get_token_dir(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcpo_u_get_token_dir"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mcpo_u_safe_filename(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mcpo_u_safe_filename(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcpo_u_safe_filename"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mcpo_u_find_free_port(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mcpo_u_find_free_port(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcpo_u_find_free_port"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mcpo_u_reserve_callback_port(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mcpo_u_reserve_callback_port(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcpo_u_reserve_callback_port"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mcpo_u_cached_redirect_port(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mcpo_u_cached_redirect_port(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcpo_u_cached_redirect_port"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mcpo_u_cached_redirect_uri(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mcpo_u_cached_redirect_uri(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcpo_u_cached_redirect_uri"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mcpo_u_is_interactive(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mcpo_u_is_interactive(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcpo_u_is_interactive"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mcpo_u_raise_if_non_interactive(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mcpo_u_raise_if_non_interactive(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcpo_u_raise_if_non_interactive"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mcpo_force_interactive_oauth(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mcpo_force_interactive_oauth(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcpo_force_interactive_oauth"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mcpo_suppress_interactive_oauth(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mcpo_suppress_interactive_oauth(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcpo_suppress_interactive_oauth"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mcpo_u_can_open_browser(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mcpo_u_can_open_browser(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcpo_u_can_open_browser"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mcpo_u_read_json(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mcpo_u_read_json(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcpo_u_read_json"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mcpo_u_write_json(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mcpo_u_write_json(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcpo_u_write_json"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mcpo_u_tokens_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mcpo_u_tokens_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcpo_u_tokens_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mcpo_u_client_info_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mcpo_u_client_info_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcpo_u_client_info_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mcpo_u_meta_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mcpo_u_meta_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcpo_u_meta_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mcpo_get_tokens(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mcpo_get_tokens(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcpo_get_tokens"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mcpo_set_tokens(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mcpo_set_tokens(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcpo_set_tokens"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mcpo_get_client_info(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mcpo_get_client_info(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcpo_get_client_info"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mcpo_set_client_info(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mcpo_set_client_info(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcpo_set_client_info"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mcpo_save_oauth_metadata(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mcpo_save_oauth_metadata(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcpo_save_oauth_metadata"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mcpo_load_oauth_metadata(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mcpo_load_oauth_metadata(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcpo_load_oauth_metadata"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mcpo_poison_client_registration(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mcpo_poison_client_registration(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcpo_poison_client_registration"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mcpo_has_cached_tokens(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mcpo_has_cached_tokens(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcpo_has_cached_tokens"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mcpo_u_make_callback_handler(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mcpo_u_make_callback_handler(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcpo_u_make_callback_handler"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mcpo_u_make_redirect_handler(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mcpo_u_make_redirect_handler(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcpo_u_make_redirect_handler"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mcpo_u_wait_for_callback(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mcpo_u_wait_for_callback(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcpo_u_wait_for_callback"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mcpo_u_make_callback_waiter(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mcpo_u_make_callback_waiter(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcpo_u_make_callback_waiter"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mcpo_u_paste_callback_reader(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mcpo_u_paste_callback_reader(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcpo_u_paste_callback_reader"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mcpo_remove_oauth_tokens(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mcpo_remove_oauth_tokens(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcpo_remove_oauth_tokens"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mcpo_u_configure_callback_port(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mcpo_u_configure_callback_port(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcpo_u_configure_callback_port"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mcpo_u_resolve_redirect_uri(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mcpo_u_resolve_redirect_uri(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcpo_u_resolve_redirect_uri"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mcpo_u_build_client_metadata(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mcpo_u_build_client_metadata(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcpo_u_build_client_metadata"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mcpo_u_maybe_preregister_client(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mcpo_u_maybe_preregister_client(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcpo_u_maybe_preregister_client"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_mcpo_build_oauth_auth(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)mcpo_build_oauth_auth(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("mcpo_build_oauth_auth"));
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
        if (strcmp(op, "mcpo_u_get_token_dir") == 0) o = emit_mcpo_u_get_token_dir(c);
        if (strcmp(op, "mcpo_u_safe_filename") == 0) o = emit_mcpo_u_safe_filename(c);
        if (strcmp(op, "mcpo_u_find_free_port") == 0) o = emit_mcpo_u_find_free_port(c);
        if (strcmp(op, "mcpo_u_reserve_callback_port") == 0) o = emit_mcpo_u_reserve_callback_port(c);
        if (strcmp(op, "mcpo_u_cached_redirect_port") == 0) o = emit_mcpo_u_cached_redirect_port(c);
        if (strcmp(op, "mcpo_u_cached_redirect_uri") == 0) o = emit_mcpo_u_cached_redirect_uri(c);
        if (strcmp(op, "mcpo_u_is_interactive") == 0) o = emit_mcpo_u_is_interactive(c);
        if (strcmp(op, "mcpo_u_raise_if_non_interactive") == 0) o = emit_mcpo_u_raise_if_non_interactive(c);
        if (strcmp(op, "mcpo_force_interactive_oauth") == 0) o = emit_mcpo_force_interactive_oauth(c);
        if (strcmp(op, "mcpo_suppress_interactive_oauth") == 0) o = emit_mcpo_suppress_interactive_oauth(c);
        if (strcmp(op, "mcpo_u_can_open_browser") == 0) o = emit_mcpo_u_can_open_browser(c);
        if (strcmp(op, "mcpo_u_read_json") == 0) o = emit_mcpo_u_read_json(c);
        if (strcmp(op, "mcpo_u_write_json") == 0) o = emit_mcpo_u_write_json(c);
        if (strcmp(op, "mcpo_u_tokens_path") == 0) o = emit_mcpo_u_tokens_path(c);
        if (strcmp(op, "mcpo_u_client_info_path") == 0) o = emit_mcpo_u_client_info_path(c);
        if (strcmp(op, "mcpo_u_meta_path") == 0) o = emit_mcpo_u_meta_path(c);
        if (strcmp(op, "mcpo_get_tokens") == 0) o = emit_mcpo_get_tokens(c);
        if (strcmp(op, "mcpo_set_tokens") == 0) o = emit_mcpo_set_tokens(c);
        if (strcmp(op, "mcpo_get_client_info") == 0) o = emit_mcpo_get_client_info(c);
        if (strcmp(op, "mcpo_set_client_info") == 0) o = emit_mcpo_set_client_info(c);
        if (strcmp(op, "mcpo_save_oauth_metadata") == 0) o = emit_mcpo_save_oauth_metadata(c);
        if (strcmp(op, "mcpo_load_oauth_metadata") == 0) o = emit_mcpo_load_oauth_metadata(c);
        if (strcmp(op, "mcpo_poison_client_registration") == 0) o = emit_mcpo_poison_client_registration(c);
        if (strcmp(op, "mcpo_has_cached_tokens") == 0) o = emit_mcpo_has_cached_tokens(c);
        if (strcmp(op, "mcpo_u_make_callback_handler") == 0) o = emit_mcpo_u_make_callback_handler(c);
        if (strcmp(op, "mcpo_u_make_redirect_handler") == 0) o = emit_mcpo_u_make_redirect_handler(c);
        if (strcmp(op, "mcpo_u_wait_for_callback") == 0) o = emit_mcpo_u_wait_for_callback(c);
        if (strcmp(op, "mcpo_u_make_callback_waiter") == 0) o = emit_mcpo_u_make_callback_waiter(c);
        if (strcmp(op, "mcpo_u_paste_callback_reader") == 0) o = emit_mcpo_u_paste_callback_reader(c);
        if (strcmp(op, "mcpo_remove_oauth_tokens") == 0) o = emit_mcpo_remove_oauth_tokens(c);
        if (strcmp(op, "mcpo_u_configure_callback_port") == 0) o = emit_mcpo_u_configure_callback_port(c);
        if (strcmp(op, "mcpo_u_resolve_redirect_uri") == 0) o = emit_mcpo_u_resolve_redirect_uri(c);
        if (strcmp(op, "mcpo_u_build_client_metadata") == 0) o = emit_mcpo_u_build_client_metadata(c);
        if (strcmp(op, "mcpo_u_maybe_preregister_client") == 0) o = emit_mcpo_u_maybe_preregister_client(c);
        if (strcmp(op, "mcpo_build_oauth_auth") == 0) o = emit_mcpo_build_oauth_auth(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
