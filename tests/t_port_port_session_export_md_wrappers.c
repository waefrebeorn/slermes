/* AUTO-GENERATED integration oracle harness for port_session_export_md_wrappers (gen_integration_oracle.py). */
#include "hermes_core_types.h"
#include "hermes_json.h"
#include "port_session_export_md_wrappers.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int sexmd_u_iso_timestamp(const char *);
extern int sexmd_u_frontmatter_value(const char *);
extern int sexmd_u_frontmatter_line(const char *);
extern int sexmd_u_message_heading(const char *);
extern int sexmd_u_render_content(const char *);
extern int sexmd_u_render_tool_calls(const char *);
extern int sexmd_u_session_id(const char *);
extern int sexmd_u_segments(const char *);
extern int sexmd_u_message_count(const char *);
extern int sexmd_u_render_messages(const char *);
extern int sexmd_u_export_body_without_hash(const char *);
extern int sexmd_u_body_for_digest(const char *);
extern int sexmd_render_session_markdown(const char *);
extern int sexmd_safe_session_filename(const char *);
extern int sexmd_file_sha256(const char *);
extern int sexmd_verify_export_file(const char *);
extern int sexmd_redact_session_data(const char *);
extern int sexmd_write_session_markdown(const char *);
extern int sexmd_append_manifest_entry(const char *);

static char *read_all(const char *path){
    FILE *f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1); if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, f); buf[r] = '\0'; fclose(f); return buf;
}

static json_t *emit_sexmd_u_iso_timestamp(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sexmd_u_iso_timestamp(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sexmd_u_iso_timestamp"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sexmd_u_frontmatter_value(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sexmd_u_frontmatter_value(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sexmd_u_frontmatter_value"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sexmd_u_frontmatter_line(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sexmd_u_frontmatter_line(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sexmd_u_frontmatter_line"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sexmd_u_message_heading(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sexmd_u_message_heading(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sexmd_u_message_heading"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sexmd_u_render_content(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sexmd_u_render_content(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sexmd_u_render_content"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sexmd_u_render_tool_calls(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sexmd_u_render_tool_calls(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sexmd_u_render_tool_calls"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sexmd_u_session_id(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sexmd_u_session_id(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sexmd_u_session_id"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sexmd_u_segments(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sexmd_u_segments(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sexmd_u_segments"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sexmd_u_message_count(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sexmd_u_message_count(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sexmd_u_message_count"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sexmd_u_render_messages(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sexmd_u_render_messages(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sexmd_u_render_messages"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sexmd_u_export_body_without_hash(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sexmd_u_export_body_without_hash(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sexmd_u_export_body_without_hash"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sexmd_u_body_for_digest(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sexmd_u_body_for_digest(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sexmd_u_body_for_digest"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sexmd_render_session_markdown(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sexmd_render_session_markdown(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sexmd_render_session_markdown"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sexmd_safe_session_filename(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sexmd_safe_session_filename(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sexmd_safe_session_filename"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sexmd_file_sha256(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sexmd_file_sha256(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sexmd_file_sha256"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sexmd_verify_export_file(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sexmd_verify_export_file(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sexmd_verify_export_file"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sexmd_redact_session_data(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sexmd_redact_session_data(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sexmd_redact_session_data"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sexmd_write_session_markdown(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sexmd_write_session_markdown(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sexmd_write_session_markdown"));
    json_set(o, "out", json_int(v)); return o;
}

static json_t *emit_sexmd_append_manifest_entry(const json_t *c){
    const char *value = json_get_str(c, "value", "");
    long v = (long)sexmd_append_manifest_entry(value);
    json_t *o = json_new_object(); json_set(o, "fn", json_string("sexmd_append_manifest_entry"));
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
        if (strcmp(op, "sexmd_u_iso_timestamp") == 0) o = emit_sexmd_u_iso_timestamp(c);
        if (strcmp(op, "sexmd_u_frontmatter_value") == 0) o = emit_sexmd_u_frontmatter_value(c);
        if (strcmp(op, "sexmd_u_frontmatter_line") == 0) o = emit_sexmd_u_frontmatter_line(c);
        if (strcmp(op, "sexmd_u_message_heading") == 0) o = emit_sexmd_u_message_heading(c);
        if (strcmp(op, "sexmd_u_render_content") == 0) o = emit_sexmd_u_render_content(c);
        if (strcmp(op, "sexmd_u_render_tool_calls") == 0) o = emit_sexmd_u_render_tool_calls(c);
        if (strcmp(op, "sexmd_u_session_id") == 0) o = emit_sexmd_u_session_id(c);
        if (strcmp(op, "sexmd_u_segments") == 0) o = emit_sexmd_u_segments(c);
        if (strcmp(op, "sexmd_u_message_count") == 0) o = emit_sexmd_u_message_count(c);
        if (strcmp(op, "sexmd_u_render_messages") == 0) o = emit_sexmd_u_render_messages(c);
        if (strcmp(op, "sexmd_u_export_body_without_hash") == 0) o = emit_sexmd_u_export_body_without_hash(c);
        if (strcmp(op, "sexmd_u_body_for_digest") == 0) o = emit_sexmd_u_body_for_digest(c);
        if (strcmp(op, "sexmd_render_session_markdown") == 0) o = emit_sexmd_render_session_markdown(c);
        if (strcmp(op, "sexmd_safe_session_filename") == 0) o = emit_sexmd_safe_session_filename(c);
        if (strcmp(op, "sexmd_file_sha256") == 0) o = emit_sexmd_file_sha256(c);
        if (strcmp(op, "sexmd_verify_export_file") == 0) o = emit_sexmd_verify_export_file(c);
        if (strcmp(op, "sexmd_redact_session_data") == 0) o = emit_sexmd_redact_session_data(c);
        if (strcmp(op, "sexmd_write_session_markdown") == 0) o = emit_sexmd_write_session_markdown(c);
        if (strcmp(op, "sexmd_append_manifest_entry") == 0) o = emit_sexmd_append_manifest_entry(c);
        else { o = json_new_object(); json_set(o, "fn", json_string(op)); }
        char *ser = json_serialize(o); printf("%s\n", ser); free(ser); json_free(o);
    }
    json_free(root); free(input); return 0;
}
