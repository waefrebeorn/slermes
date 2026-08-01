/* AUTO-GENERATED integration oracle harness for port_env_base_wrappers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_env_base_wrappers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int envb_buffered_chars(const char *);
extern int envb_total_chars(const char *);
extern int envb_append(const char *);
extern int envb_set_activity_callback(const char *);
extern int envb_u_get_activity_callback(const char *);
extern int envb_touch_activity_if_due(const char *);
extern int envb_get_sandbox_dir(const char *);
extern int envb_u_pipe_stdin(const char *);
extern int envb_u_popen_bash(const char *);
extern int envb_u_load_json_store(const char *);
extern int envb_u_save_json_store(const char *);
extern int envb_u_file_mtime_key(const char *);
extern int envb_stdout(const char *);
extern int envb_returncode(const char *);
extern int envb_stdout_2(const char *);
extern int envb_returncode_2(const char *);
extern int envb_u_cwd_marker(const char *);
extern int envb_get_temp_dir(const char *);
extern int envb_init_session(const char *);
extern int envb_u_quote_cwd_for_cd(const char *);
extern int envb_u_quote_shell_path(const char *);
extern int envb_u_wrap_command(const char *);
extern int envb_u_embed_stdin_heredoc(const char *);
extern int envb_u_wait_for_process(const char *);
extern int envb_u_update_cwd(const char *);
extern int envb_u_extract_cwd_from_output(const char *);
extern int envb_u__del__(const char *);
extern int envb_u_prepare_command(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_envb_buffered_chars(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envb_buffered_chars(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envb_buffered_chars"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envb_total_chars(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envb_total_chars(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envb_total_chars"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envb_append(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envb_append(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envb_append"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envb_set_activity_callback(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envb_set_activity_callback(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envb_set_activity_callback"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envb_u_get_activity_callback(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envb_u_get_activity_callback(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envb_u_get_activity_callback"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envb_touch_activity_if_due(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envb_touch_activity_if_due(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envb_touch_activity_if_due"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envb_get_sandbox_dir(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envb_get_sandbox_dir(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envb_get_sandbox_dir"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envb_u_pipe_stdin(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envb_u_pipe_stdin(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envb_u_pipe_stdin"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envb_u_popen_bash(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envb_u_popen_bash(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envb_u_popen_bash"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envb_u_load_json_store(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envb_u_load_json_store(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envb_u_load_json_store"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envb_u_save_json_store(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envb_u_save_json_store(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envb_u_save_json_store"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envb_u_file_mtime_key(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envb_u_file_mtime_key(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envb_u_file_mtime_key"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envb_stdout(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envb_stdout(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envb_stdout"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envb_returncode(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envb_returncode(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envb_returncode"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envb_stdout_2(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envb_stdout_2(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envb_stdout_2"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envb_returncode_2(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envb_returncode_2(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envb_returncode_2"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envb_u_cwd_marker(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envb_u_cwd_marker(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envb_u_cwd_marker"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envb_get_temp_dir(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envb_get_temp_dir(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envb_get_temp_dir"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envb_init_session(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envb_init_session(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envb_init_session"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envb_u_quote_cwd_for_cd(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envb_u_quote_cwd_for_cd(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envb_u_quote_cwd_for_cd"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envb_u_quote_shell_path(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envb_u_quote_shell_path(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envb_u_quote_shell_path"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envb_u_wrap_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envb_u_wrap_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envb_u_wrap_command"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envb_u_embed_stdin_heredoc(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envb_u_embed_stdin_heredoc(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envb_u_embed_stdin_heredoc"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envb_u_wait_for_process(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envb_u_wait_for_process(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envb_u_wait_for_process"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envb_u_update_cwd(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envb_u_update_cwd(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envb_u_update_cwd"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envb_u_extract_cwd_from_output(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envb_u_extract_cwd_from_output(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envb_u_extract_cwd_from_output"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envb_u__del__(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envb_u__del__(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envb_u__del__"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_envb_u_prepare_command(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)envb_u_prepare_command(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("envb_u_prepare_command"));
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
        if (strcmp(op, "envb_buffered_chars") == 0) o = emit_envb_buffered_chars(c);
        if (strcmp(op, "envb_total_chars") == 0) o = emit_envb_total_chars(c);
        if (strcmp(op, "envb_append") == 0) o = emit_envb_append(c);
        if (strcmp(op, "envb_set_activity_callback") == 0) o = emit_envb_set_activity_callback(c);
        if (strcmp(op, "envb_u_get_activity_callback") == 0) o = emit_envb_u_get_activity_callback(c);
        if (strcmp(op, "envb_touch_activity_if_due") == 0) o = emit_envb_touch_activity_if_due(c);
        if (strcmp(op, "envb_get_sandbox_dir") == 0) o = emit_envb_get_sandbox_dir(c);
        if (strcmp(op, "envb_u_pipe_stdin") == 0) o = emit_envb_u_pipe_stdin(c);
        if (strcmp(op, "envb_u_popen_bash") == 0) o = emit_envb_u_popen_bash(c);
        if (strcmp(op, "envb_u_load_json_store") == 0) o = emit_envb_u_load_json_store(c);
        if (strcmp(op, "envb_u_save_json_store") == 0) o = emit_envb_u_save_json_store(c);
        if (strcmp(op, "envb_u_file_mtime_key") == 0) o = emit_envb_u_file_mtime_key(c);
        if (strcmp(op, "envb_stdout") == 0) o = emit_envb_stdout(c);
        if (strcmp(op, "envb_returncode") == 0) o = emit_envb_returncode(c);
        if (strcmp(op, "envb_stdout_2") == 0) o = emit_envb_stdout_2(c);
        if (strcmp(op, "envb_returncode_2") == 0) o = emit_envb_returncode_2(c);
        if (strcmp(op, "envb_u_cwd_marker") == 0) o = emit_envb_u_cwd_marker(c);
        if (strcmp(op, "envb_get_temp_dir") == 0) o = emit_envb_get_temp_dir(c);
        if (strcmp(op, "envb_init_session") == 0) o = emit_envb_init_session(c);
        if (strcmp(op, "envb_u_quote_cwd_for_cd") == 0) o = emit_envb_u_quote_cwd_for_cd(c);
        if (strcmp(op, "envb_u_quote_shell_path") == 0) o = emit_envb_u_quote_shell_path(c);
        if (strcmp(op, "envb_u_wrap_command") == 0) o = emit_envb_u_wrap_command(c);
        if (strcmp(op, "envb_u_embed_stdin_heredoc") == 0) o = emit_envb_u_embed_stdin_heredoc(c);
        if (strcmp(op, "envb_u_wait_for_process") == 0) o = emit_envb_u_wait_for_process(c);
        if (strcmp(op, "envb_u_update_cwd") == 0) o = emit_envb_u_update_cwd(c);
        if (strcmp(op, "envb_u_extract_cwd_from_output") == 0) o = emit_envb_u_extract_cwd_from_output(c);
        if (strcmp(op, "envb_u__del__") == 0) o = emit_envb_u__del__(c);
        if (strcmp(op, "envb_u_prepare_command") == 0) o = emit_envb_u_prepare_command(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
