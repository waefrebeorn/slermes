/* AUTO-GENERATED integration oracle harness for port_session_export_wrappers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_session_export_wrappers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int sexp_normalize_export_format(const char *);
extern int sexp_normalize_export_only(const char *);
extern int sexp_render_sessions_export(const char *);
extern int sexp_export_record_count(const char *);
extern int sexp_iter_user_prompt_records(const char *);
extern int sexp_u_render_jsonl(const char *);
extern int sexp_u_render_markdown(const char *);
extern int sexp_u_render_user_prompts_markdown(const char *);
extern int sexp_u_append_prompt_records(const char *);
extern int sexp_u_render_full_markdown(const char *);
extern int sexp_u_append_session_messages(const char *);
extern int sexp_u_messages(const char *);
extern int sexp_u_message_text(const char *);
extern int sexp_u_content_part_text(const char *);
extern int sexp_u_session_metadata_lines(const char *);
extern int sexp_u_session_id(const char *);
extern int sexp_u_session_title_or_id(const char *);
extern int sexp_u_heading_text(const char *);
extern int sexp_u_inline_text(const char *);
extern int sexp_u_fenced_text(const char *);
extern int sexp_u_finish_markdown(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_sexp_normalize_export_format(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sexp_normalize_export_format(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sexp_normalize_export_format"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sexp_normalize_export_only(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sexp_normalize_export_only(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sexp_normalize_export_only"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sexp_render_sessions_export(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sexp_render_sessions_export(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sexp_render_sessions_export"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sexp_export_record_count(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sexp_export_record_count(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sexp_export_record_count"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sexp_iter_user_prompt_records(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sexp_iter_user_prompt_records(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sexp_iter_user_prompt_records"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sexp_u_render_jsonl(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sexp_u_render_jsonl(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sexp_u_render_jsonl"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sexp_u_render_markdown(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sexp_u_render_markdown(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sexp_u_render_markdown"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sexp_u_render_user_prompts_markdown(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sexp_u_render_user_prompts_markdown(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sexp_u_render_user_prompts_markdown"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sexp_u_append_prompt_records(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sexp_u_append_prompt_records(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sexp_u_append_prompt_records"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sexp_u_render_full_markdown(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sexp_u_render_full_markdown(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sexp_u_render_full_markdown"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sexp_u_append_session_messages(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sexp_u_append_session_messages(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sexp_u_append_session_messages"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sexp_u_messages(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sexp_u_messages(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sexp_u_messages"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sexp_u_message_text(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sexp_u_message_text(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sexp_u_message_text"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sexp_u_content_part_text(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sexp_u_content_part_text(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sexp_u_content_part_text"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sexp_u_session_metadata_lines(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sexp_u_session_metadata_lines(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sexp_u_session_metadata_lines"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sexp_u_session_id(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sexp_u_session_id(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sexp_u_session_id"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sexp_u_session_title_or_id(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sexp_u_session_title_or_id(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sexp_u_session_title_or_id"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sexp_u_heading_text(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sexp_u_heading_text(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sexp_u_heading_text"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sexp_u_inline_text(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sexp_u_inline_text(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sexp_u_inline_text"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sexp_u_fenced_text(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sexp_u_fenced_text(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sexp_u_fenced_text"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sexp_u_finish_markdown(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sexp_u_finish_markdown(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sexp_u_finish_markdown"));
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
        if (strcmp(op, "sexp_normalize_export_format") == 0) o = emit_sexp_normalize_export_format(c);
        if (strcmp(op, "sexp_normalize_export_only") == 0) o = emit_sexp_normalize_export_only(c);
        if (strcmp(op, "sexp_render_sessions_export") == 0) o = emit_sexp_render_sessions_export(c);
        if (strcmp(op, "sexp_export_record_count") == 0) o = emit_sexp_export_record_count(c);
        if (strcmp(op, "sexp_iter_user_prompt_records") == 0) o = emit_sexp_iter_user_prompt_records(c);
        if (strcmp(op, "sexp_u_render_jsonl") == 0) o = emit_sexp_u_render_jsonl(c);
        if (strcmp(op, "sexp_u_render_markdown") == 0) o = emit_sexp_u_render_markdown(c);
        if (strcmp(op, "sexp_u_render_user_prompts_markdown") == 0) o = emit_sexp_u_render_user_prompts_markdown(c);
        if (strcmp(op, "sexp_u_append_prompt_records") == 0) o = emit_sexp_u_append_prompt_records(c);
        if (strcmp(op, "sexp_u_render_full_markdown") == 0) o = emit_sexp_u_render_full_markdown(c);
        if (strcmp(op, "sexp_u_append_session_messages") == 0) o = emit_sexp_u_append_session_messages(c);
        if (strcmp(op, "sexp_u_messages") == 0) o = emit_sexp_u_messages(c);
        if (strcmp(op, "sexp_u_message_text") == 0) o = emit_sexp_u_message_text(c);
        if (strcmp(op, "sexp_u_content_part_text") == 0) o = emit_sexp_u_content_part_text(c);
        if (strcmp(op, "sexp_u_session_metadata_lines") == 0) o = emit_sexp_u_session_metadata_lines(c);
        if (strcmp(op, "sexp_u_session_id") == 0) o = emit_sexp_u_session_id(c);
        if (strcmp(op, "sexp_u_session_title_or_id") == 0) o = emit_sexp_u_session_title_or_id(c);
        if (strcmp(op, "sexp_u_heading_text") == 0) o = emit_sexp_u_heading_text(c);
        if (strcmp(op, "sexp_u_inline_text") == 0) o = emit_sexp_u_inline_text(c);
        if (strcmp(op, "sexp_u_fenced_text") == 0) o = emit_sexp_u_fenced_text(c);
        if (strcmp(op, "sexp_u_finish_markdown") == 0) o = emit_sexp_u_finish_markdown(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
